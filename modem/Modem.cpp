/*
   Pico W partial adaptation of TheOldNet.com RS232 Serial WIFI Modem
   by Stefan Wessels, 2023 under GPLv3 license.

   based on https://github.com/ssshake/vintage-computer-wifi-modem
    Copyright (C) 2020 Richard Bettridge under GPLv3 license.
   based on https://github.com/RolandJuno/esp8266_modem
    Copyright (C) 2016 Paul Rickards rickards@gmail.com under GPLv3 license.
   based on https://github.com/jsalin/esp8266_modem
    Copyright (C) 2015 Jussi Salin salinjus@gmail.com under GPLv3 license.

   WiFi SIXFOUR - A virtual WiFi modem based on the ESP 8266 chipset
   Copyright (C) 2016 Paul Rickards <rickards@gmail.com>
   Added EEPROM Read/Write, status/help pages, busy answering of incoming calls
   uses the readily available Sparkfun ESP8266 WiFi Shield which has 5v level
   shifters and 3.3v voltage regulation present-- easily tcp_connect to a C64
   https:                                      //www.sparkfun.com/products/13287

   based on
   ESP8266 based virtual modem
   Copyright (C) 2016 Jussi Salin <salinjus@gmail.com>

   https:                                      //github.com/jsalin/esp8266_modem

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <errno.h>
#include <fcntl.h>

#include <pico/types.h>
#include <pico/time.h>
#include <pico/cyw43_arch.h>
#include <hardware/flash.h>
#include <hardware/flash.h>
#include "pico/multicore.h"

#include <FreeRTOS.h>
#include <task.h>

#include <lwip/ip4_addr.h>
#include <lwip/dns.h>
#include <lwip/netif.h>

#include <smb2/smb2.h>
#include <smb2/libsmb2.h>

#include "WiFi.h"
#include "WString.h"
#include "Queue.h"
#include "MemBuffer.h"
#include "NTPClient.h"
#include "CoreUART.h"

namespace Modem
{

// Version
String build = __TIMESTAMP__;

// For saving to flash
#define MEM_SAVE_SIZE 1024       // Max size at the moment
#define FLASH_END_ZONE_KILO 2044 // * 1024 for offset in flash of where to save
#define CURRENT_SAVE_VERSION 0
MemBuffer flashSaveBuffer;
bool sd_init_driver = false;

// For Network Time
NTPClient ntp;

// Modem variables
String cmd = "";            // Gather a new AT command to this string from serial
#define MAX_CMD_LENGTH 256  // Maximum length for AT command
bool cmdMode = true;        // Are we in AT command mode or connected mode
bool callConnected = false; // Are we currently in a call
bool telnet = false;        // Is telnet control code handling enabled
bool verboseResults = true;
bool echo = false;
bool hex = false;
bool quietMode = false;

char plusCount = 0; // Go to AT mode at "+++" sequence, that has to be counted
#define PLUS_TIME 1000
absolute_time_t plusTime = nil_time; // When did we last receive a "+++" sequence
#define LED_TIME 15                  // How many ms to keep LED on at activity
absolute_time_t ledTime = nil_time;

String speedDials[10];
int bauds[] = {300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
byte serialspeed = 5;
String ssid, password, ssh_user, ssh_pass;

#define TX_BUF_SIZE 512+32 // Buffer where to Read from serial before writing to TCP, or for sending as adtVServer
uint8_t txBuf[TX_BUF_SIZE];
#define RX_BUF_SIZE 256 // Buffer where to Read from serial for adtVServerCommands
uint8_t rxBuf[RX_BUF_SIZE];
String resultCodes[] = {"OK", "CONNECT", "RING", "NO CARRIER", "ERROR", "", "NO DIALTONE", "BUSY", "NO ANSWER"};
enum resultCodes_t
{
    R_OK,
    R_CONNECT,
    R_RING,
    R_NOCARRIER,
    R_ERROR,
    R_NONE,
    R_NODIALTONE,
    R_BUSY,
    R_NOANSWER
};
absolute_time_t connectTime = nil_time;

// Telnet codes
#define DO 0xfd
#define WONT 0xfc
#define WILL 0xfb
#define DONT 0xfe

WiFiClient tcpClient; // Connections over WiFi
#ifdef USE_UART
Queue c0cmd;
#endif
Queue c0rx;
Queue c0tx;

typedef struct VDrive_
{
    // SMB2 structures
    struct smb2_context *smb2;
    struct smb2_url *url;
    // struct smb2dir *dir;
    struct smb2dirent *ent;
    struct smb2fh *fh;
    struct smb2_stat_64 st;
    int headerSize;
    bool mounted;
} VDrive;
VDrive vdrive[2];

/**
 * Turn the connected delta time into a human readable string
 */
String connectTimeString()
{
    String out = "";
    if (!is_nil_time(connectTime))
    {
        int secs = us_to_ms(absolute_time_diff_us(connectTime, get_absolute_time())) / 1000;
        int mins = secs / 60;
        int hours = mins / 60;
        if (hours < 10)
            out.concat("0");
        out.concat(String(hours));
        out.concat(":");
        if (mins % 60 < 10)
            out.concat("0");
        out.concat(String(mins % 60));
        out.concat(":");
        if (secs % 60 < 10)
            out.concat("0");
        out.concat(String(secs % 60));
    }
    else
    {
        out = "NOT CONNECTED";
    }
    return out;
}

/**
 * Make sure the ssid and password are set, and use those to try to connect the Pico W to the named WiFi
 */
int connectWiFi()
{
    if (ssid == "" || password == "")
    {
        c0tx.println("CONFIGURE SSID AND PASSWORD. TYPE AT? FOR HELP.");
        return -1;
    }

    c0tx.print("\nCONNECTING TO SSID ");
    c0tx.println(ssid);
    if (WiFi.begin(ssid.c_str(), password.c_str()))
    {
        c0tx.print("COULD NOT CONNECT TO ");
        c0tx.println(ssid);
        return -1;
    }
    return 0;
}

/**
 * Show the user the result of an action - either as number or as text depending on result/verbosity setting
 */
void sendResult(int resultCode)
{
    c0tx.print("\r\n");
    if (quietMode == 1)
    {
        return;
    }
    if (!verboseResults)
    {
        c0tx.println(resultCode);
        return;
    }
    if (resultCode == R_CONNECT)
    {
        c0tx.print(String(resultCodes[R_CONNECT]) + " " + String(bauds[serialspeed]));
    }
    else if (resultCode == R_NOCARRIER)
    {
        c0tx.print(String(resultCodes[R_NOCARRIER]) + " (" + connectTimeString() + ")");
    }
    else
    {
        c0tx.print(String(resultCodes[resultCode]));
    }
    c0tx.print("\r\n");
}

/**
 * write a string surrounded by carride retur-line feed to the Serial port
 */
void sendString(String msg)
{
    c0tx.print("\r\n");
    c0tx.print(msg);
    c0tx.print("\r\n");
}

/**
 * Write a 1-byte string length and then the string data to memory buffer.  Not committed to flash yet
 */
int Save(String &str)
{
    uint8_t length = str.length();
    if(length > 64)
        length = 64;
    if (flashSaveBuffer.Write((uint8_t *)&length, sizeof(uint8_t)) < 0)
        return -1;
    if (flashSaveBuffer.Write((const uint8_t *)str.c_str(), length) < 0)
        return -1;
    return 1;
}

/**
 * Write a single character to memory buffer.  Not committed to flash yet
 */
int Save(byte a)
{
    return flashSaveBuffer.Write(a);
}

/**
 * Load a string (max len 64) from flash at index. 1-byte length and then data.  Advance index past loaded data
 * errors return a nullptr and then subsequent calls will fail as index in the buffer won't have advanced
 */
const char *LoadString()
{
    static uint8_t readBuffer[65];

    uint8_t length;
    if (flashSaveBuffer.Read((uint8_t *)&length, sizeof(uint8_t)) < 0)
        return nullptr;
    if (length > 64)
        return nullptr;
    if (flashSaveBuffer.Read(readBuffer, length) < 0)
        return nullptr;
    readBuffer[length] = '\0';
    return (char*)readBuffer;
}

/**
 * Load a single character from flash at current index, and advance index by 1
 */
int Load()
{
    return flashSaveBuffer.Read();
}

/**
 * Save the in-memory settings/options to flash
 */
void saveSettings()
{
#ifdef USE_UART    
    // This is an attempt to stop Core1 so the flash write will not fail
    // I believe this works since flash saving now does seem to "take" in UART mode
    c0cmd.Write('S');
    delay(100);
    multicore_reset_core1();
    // Flash saving in PIO mode seems to always work, without stopping Core 1
    // Putting the uart_interface into __time_critical_func did not help the 
    // UART flash save work
#endif    


    flashSaveBuffer.begin(MEM_SAVE_SIZE);
    Save(0x9C);
    Save(0x15);
    Save(0x40);
    Save(0x85);
    Save(CURRENT_SAVE_VERSION);
    Save(ssid);
    Save(password);
    Save(ssh_user);
    Save(ssh_pass);
    Save(serialspeed);
    Save(byte(echo));
    Save(byte(hex));
    Save(byte(telnet));
    Save(byte(verboseResults));
    Save(byte(quietMode));

    for (int i = 0; i < 10; i++)
    {
        Save(speedDials[i]);
    }

    vPortEnterCritical();
    flash_range_erase((FLASH_END_ZONE_KILO * 1024), FLASH_SECTOR_SIZE);
    flash_range_program((FLASH_END_ZONE_KILO * 1024), flashSaveBuffer.GetData(), ((flashSaveBuffer.GetWrittenLength() / 256) + 1) * 256);
    vPortExitCritical();

#ifdef USE_UART
    // This attempts to restart core 1.  This does not appear to work and afte a flash save, 
    // a restart of the Pico W is still needed
    multicore_launch_core1(CoreUART::uart_interface);
#endif
}

/**
 * Load the flash settings into memory - apply saved setting options
 */
void loadSettings()
{
    uint8_t hash1, hash2, hash3, hash4, saveVer = CURRENT_SAVE_VERSION;
    flashSaveBuffer.begin((uint8_t *)(XIP_BASE + (FLASH_END_ZONE_KILO * 1024)), MEM_SAVE_SIZE);

    vPortEnterCritical();
    hash1 = Load();
    hash2 = Load();
    hash3 = Load();
    hash4 = Load();
    saveVer = Load();
    if (hash1 == 0x9C && hash2 == 0x15 && hash3 == 0x40 && hash4 == 0x85 && saveVer == CURRENT_SAVE_VERSION)
    {
        ssid = LoadString();
        password = LoadString();
        ssh_user = LoadString();
        ssh_pass = LoadString();
        serialspeed = Load();
        echo = Load();
        hex = Load();
        telnet = Load();
        verboseResults = Load();
        quietMode = Load();

        for (int i = 0; i < 10; i++)
        {
            speedDials[i] = LoadString();
        }
    }
    vPortExitCritical();
}

/**
 * Show the settings currently saved to flash ram (without altering the in-memory settings)
 */
void displaySavedSettings()
{
    uint8_t i, hash1, hash2, hash3, hash4, temp[6], saveVer = CURRENT_SAVE_VERSION;
    flashSaveBuffer.begin((uint8_t *)(XIP_BASE + (FLASH_END_ZONE_KILO * 1024)), MEM_SAVE_SIZE);
    String tempStr[4], speedStr[10];

    vPortEnterCritical();
    hash1 = Load();
    hash2 = Load();
    hash3 = Load();
    hash4 = Load();
    saveVer = Load();
    if (hash1 == 0x9C && hash2 == 0x15 && hash3 == 0x40 && hash4 == 0x85 && saveVer == CURRENT_SAVE_VERSION)
    {
        if (saveVer == CURRENT_SAVE_VERSION)
        {
            for (i = 0; i < 4; i++)
                tempStr[i] = LoadString();
            for (i = 0; i < 6; i++)
                temp[i] = Load();
            for (i = 0; i < 10; i++)
                speedStr[i] = LoadString();
        }
    }
    vPortExitCritical();
    if (hash1 == 0x9C && hash2 == 0x15 && hash3 == 0x40 && hash4 == 0x85)
    {
        if (saveVer == CURRENT_SAVE_VERSION)
        {
            c0tx.printf("SSID = %s\r\n", tempStr[0].c_str());
            c0tx.printf("PASSWORD = %s\r\n", tempStr[1].c_str());
            c0tx.printf("SSH USER = %s\r\n", tempStr[2].c_str());
            c0tx.print("SSH PASS = ");
            if (tempStr[3].length())
                c0tx.print("********");
            c0tx.println();
            c0tx.printf("BAUD = %d (%d)\r\n", bauds[temp[0]], temp[0]);
            c0tx.printf("ECHO = %d\r\n", temp[1]);
            c0tx.printf("HEX = %d\r\n", temp[2]);
            c0tx.printf("TELNET = %d\r\n", temp[3]);
            c0tx.printf("VERBOSE = %d\r\n", temp[4]);
            c0tx.printf("QUIET MODE = %d\r\n", temp[5]);
            for (i = 0; i < 10; i++)
                c0tx.printf("Speed Dial %d = %s\r\n", i, speedStr[i].c_str());
        }
        else
        {
            c0tx.printf("Save version mismatch. Expected %d, got %d\r\n", CURRENT_SAVE_VERSION, saveVer);
        }
    }
    else
    {
        c0tx.println("There is no valid save in flash");
    }
}

/**
 * Set sane default configurations for user alter-able settings
 */
void defaultSettings()
{
    ssid = "";
    password = "";
    ssh_user = "";
    ssh_pass = "";
    serialspeed = 4;
    echo = true;
    hex = false;
    telnet = false; // Is telnet control code handling enabled
    verboseResults = true;
    quietMode = false;

    speedDials[0] = "theoldnet.com:23";
    speedDials[1] = "bbs.retrocampus.com:23";
    speedDials[2] = "bbs.eotd.com:23";
    speedDials[3] = "blackflag.acid.org:31337";
    speedDials[4] = "bbs.starbase21.net:23";
    speedDials[5] = "reflections.servebbs.com:23";
    speedDials[6] = "heatwavebbs.com:9640";
    speedDials[7] = "";
    speedDials[8] = "";
    speedDials[9] = "";
}

/**
 * Show visible WiFi netyworks, seen in a scan, to the user over Serial
 */
void scanNetworks()
{
    c0tx.println("SCANNING FOR WIFI NETWORKS...");
    WiFi.scanNetworks();
    while (_wifi_node *aNode = WiFiClass::_wifi_nodes.GetHead())
    {
        String s((char *)aNode->result.ssid, aNode->result.ssid_len);
        c0tx.print(s);
        c0tx.print(" ");
        c0tx.print(aNode->result.auth_mode, HEX);
        c0tx.print(" ");
        c0tx.println(aNode->result.rssi);
        WiFiClass::_wifi_nodes.RemNode(aNode);
    }
}

/**
 * Disconnect the Pico W from the WiFi network.  Update the local IP/Mask/Gateway to 0.0.0.0
 */
void disconnectWiFi()
{
    WiFi.disconnect();
}

/**
 * Make sure the new rate is valid.  Inform the user the baud rate will change in 5 seconds and do so after 5
 */
void setBaudRate(int inSpeed)
{
    if (inSpeed == 0)
    {
        sendResult(R_ERROR);
        return;
    }
    int foundBaud = -1;
    for (int i = 0; i < sizeof(bauds) / sizeof(bauds[0]); i++)
    {
        if (inSpeed == bauds[i])
        {
            foundBaud = i;
            break;
        }
    }

    // requested baud rate not found, return error
    if (foundBaud == -1)
    {
        sendResult(R_ERROR);
        return;
    }
    if (foundBaud == serialspeed)
    {
        sendResult(R_OK);
        return;
    }
#ifdef USE_UART
    c0tx.print("SWITCHING SERIAL PORT TO ");
    c0tx.print(inSpeed);
    c0tx.println(" IN 5 SECONDS");
    delay(5000);
    c0cmd.Write('B');
    c0cmd.Write(foundBaud);
#endif
    serialspeed = foundBaud;
    sendResult(R_OK);
}

/**
 * Show the connection state and applicable network information
 */
void displayNetworkStatus()
{
    c0tx.print("WIFI STATUS: ");
    if (WiFi.status() == CYW43_LINK_UP || WiFi.status() == CYW43_LINK_JOIN)
    {
        c0tx.println("CONNECTED");
    }
    else if (WiFi.status() == CYW43_LINK_NOIP)
    {
        c0tx.println("NO IP ASSIGNED");
    }
    else if (WiFi.status() == CYW43_LINK_BADAUTH)
    {
        c0tx.println("CONNECT FAILED BAD AUTHENTICATION");
    }
    else if (WiFi.status() == CYW43_LINK_NONET)
    {
        c0tx.println("SSID UNAVAILABLE");
    }
    else if (WiFi.status() == CYW43_LINK_FAIL)
    {
        c0tx.println("CONNECTION LOST");
    }
    else if (WiFi.status() == CYW43_LINK_DOWN)
    {
        c0tx.println("DISCONNECTED");
    }
    else
    {
        c0tx.println("UNDEFINED");
    }
    c0tx.print("SSID.......: ");
    c0tx.println(WiFi.SSID());

    //  c0tx.print("ENCRYPTION: ");
    //  switch(WiFi.encryptionType()) {
    //    case 2:
    //      c0tx.println("TKIP (WPA)");
    //      break;
    //    case 5:
    //      c0tx.println("WEP");
    //      break;
    //    case 4:
    //      c0tx.println("CCMP (WPA)");
    //      break;
    //    case 7:
    //      c0tx.println("NONE");
    //      break;
    //    case 8:
    //      c0tx.println("AUTO");
    //      break;
    //    default:
    //      c0tx.println("UNKNOWN");
    //      break;
    //  }

    byte mac[6];
    WiFi.macAddress(mac);
    c0tx.print("MAC ADDRESS: ");
    c0tx.print(mac[0], HEX);
    c0tx.print(":");
    c0tx.print(mac[1], HEX);
    c0tx.print(":");
    c0tx.print(mac[2], HEX);
    c0tx.print(":");
    c0tx.print(mac[3], HEX);
    c0tx.print(":");
    c0tx.print(mac[4], HEX);
    c0tx.print(":");
    c0tx.println(mac[5], HEX);
    c0tx.print("IP ADDRESS.: ");
    c0tx.println(WiFi.localIP());
    c0tx.print("GATEWAY....: ");
    c0tx.println(WiFi.gatewayIP());
    c0tx.print("SUBNET MASK: ");
    c0tx.println(WiFi.subnetMask());
    c0tx.print("WEB CONFIG.: HTTP://");
    c0tx.println(WiFi.localIP());
    c0tx.print("CALL STATUS: ");
    if (callConnected)
    {
        c0tx.print("CALL LENGTH: ");
        c0tx.println(connectTimeString());
    }
    else
    {
        c0tx.println("NOT CONNECTED");
    }
    c0tx.print("DATE & TIME: ");
    c0tx.println(ntp.getFormattedDate());
}

/**
 * Show the in-memory state of all the configured options
 */
void displayCurrentSettings()
{
    c0tx.println("ACTIVE PROFILE:");
    c0tx.print("BAUD: ");
    c0tx.println(bauds[serialspeed]);
    c0tx.print("SSID: ");
    c0tx.println(ssid);
    c0tx.print("PASS: ");
    c0tx.println(password);
    c0tx.print("SSH USER: ");
    c0tx.println(ssh_user);
    c0tx.print("SSH PASS: ");
    if(ssh_pass.length())
        c0tx.print("********");
    c0tx.println();
    c0tx.print("E");
    c0tx.print(echo);
    c0tx.print(" ");
    c0tx.print("Q");
    c0tx.print(quietMode);
    c0tx.print(" ");
    c0tx.print("V");
    c0tx.print(verboseResults);
    c0tx.print(" ");
    c0tx.print("NET");
    c0tx.print(telnet);
    c0tx.println();

    c0tx.println("SPEED DIAL:");
    for (int i = 0; i < 10; i++)
    {
        c0tx.print(i);
        c0tx.print(": ");
        c0tx.println(speedDials[i]);
    }
    c0tx.println();
}

/**
 * Read characters from the serial port till a space is read
 */
void waitForSpace()
{
    c0tx.print("PRESS SPACE");
    char c = 0;
    while (c != 0x20)
    {
        if (c0rx.available() > 0)
        {
            c = c0rx.Read();
        }
    }
    c0tx.print("\r");
}

/**
 * Send a notice about this pico-modem to the serial port
 */
void welcome()
{
    c0tx.println();
    c0tx.println("StewBC's RPi Pico W WiFi modem emulator");
    c0tx.println("github.com/StewBC/pico_w-modem");
    c0tx.println("BUILD " + build + "");
    c0tx.println("Based on The Old Net - RS232 Serial WIFI Modem");
    c0tx.println("GPL3 github.com/ssshake/vintage-computer-wifi-modem");
    c0tx.println();
}

/**
 * Show the AT commands that are supported
 */
void displayHelp()
{
    welcome();
    c0tx.println("AT COMMAND SUMMARY:");
    c0tx.println("WIFI SCAN............: ATC?");
    c0tx.println("SET SSID.............: AT$SSID=WIFISSID");
    c0tx.println("SET WIFI PASSWORD....: AT$PASS=WIFIPASSWORD");
    c0tx.println("WIFI OFF/ON..........: ATC0 / ATC1");
    c0tx.println("NETWORK INFO.........: ATI");
    c0tx.println("DIAL HOST............: ATDTHOST:PORT");
    c0tx.println("SET SPEED DIAL.......: AT&ZN=HOST:PORT (N=0-9)");
    c0tx.println("SPEED DIAL...........: ATDSN (N=0-9)");
    c0tx.println("SET SSH USER NAME....: AT$SSHU=SSHUSERNAME");
    c0tx.println("SET SSH PASSWORD.....: AT$SSHP=SSHPASSWORD");
    c0tx.println("SSH DIAL.............: ATSSHHOST:PORT");
    c0tx.println("HTTP GET.............: ATGET<URL>");
    c0tx.println("GOPHER REQUEST.......: ATGPH<URL>");
    waitForSpace();
    c0tx.println("HANDLE TELNET........: ATNETN (N=0,1)");
    c0tx.println("MOUNT SMB VSDRIVE....: ATVSNSMB://HOST/FILEPATH (N=1-2)");
    c0tx.println("VSDRIVE ONLINE.......: ATVSO");
    c0tx.println("ECHO OFF/ON..........: ATE0 / ATE1");
    c0tx.println("QUIET MODE OFF/ON....: ATQ0 / ATQ1");
    c0tx.println("VERBOSE OFF/ON.......: ATV0 / ATV1");
    c0tx.println("SET BAUD RATE........: AT$SB=N (3,12,24,48,96");
    c0tx.println("                        192,384,576,1152)*100");
    c0tx.println("HANGUP...............: ATH");
    c0tx.println("ENTER CMD MODE.......: +++");
    c0tx.println("EXIT CMD MODE........: ATO");
    c0tx.println("LOAD SETTINGS........: ATZ");
    c0tx.println("SAVE SETTINGS........: AT&W");
    c0tx.println("SHOW SAVED SETTINGS..: AT&V?");
    c0tx.println("SHOW PROFILE.........: AT&V");
    c0tx.println("FACT. DEFAULTS.......: AT&F");
    c0tx.println("QUERY MOST COMMANDS FOLLOWED BY '?'");
}

/**
 * Speed Dials are 0 - 9.  This updates an in-memory speed dial
 */
void storeSpeedDial(byte num, String location)
{
    if (num < 0 || num > 9)
    {
        return;
    }
    // Max 64 characters per entry
    speedDials[num] = location.substring(0,64);
    c0tx.print("STORED ");
    c0tx.print(num);
    c0tx.print(": ");
    c0tx.println(location);
}

/**
 * Turn on the LED and store the time, so the LED will be shortly after turned off
 */
void led_set(bool on)
{
    //   digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
    ledTime = get_absolute_time();
}

/**
 * Make a TCP connection to a remote host.  Possibly wrap the connection in SSH
 */
void dialOut(String upCmd)
{
    int numberIndex = 4;

    // Can't place a call while in a call
    if (callConnected)
    {
        sendResult(R_ERROR);
        return;
    }
    String host, port;
    int portIndex;
    // Dialing a stored number (note atdssh excluded)
    if (upCmd.indexOf("ATDS") == 0 && upCmd.indexOf("ATDSS") != 0)
    {
        byte speedNum = upCmd.substring(4, 5).toInt();
        portIndex = speedDials[speedNum].indexOf(':');
        if (portIndex != -1)
        {
            host = speedDials[speedNum].substring(0, portIndex);
            port = speedDials[speedNum].substring(portIndex + 1);
        }
        else
        {
            host = speedDials[speedNum];
            port = "23";
        }
    }
    else
    {
        // Dialing an ad-hoc number
        if (upCmd.indexOf("ATDSSH") == 0)
            numberIndex = 6;

        int portIndex = cmd.indexOf(":");
        if (portIndex != -1)
        {
            host = cmd.substring(numberIndex, portIndex);
            port = cmd.substring(portIndex + 1, cmd.length());
        }
        else
        {
            host = cmd.substring(numberIndex, cmd.length());
            if (numberIndex == 6)
                port = "22"; // SSH default
            else
                port = "23"; // Telnet default
        }
    }
    host.trim(); // remove leading or trailing spaces
    port.trim();

    c0tx.print("DIALING ");
    c0tx.print(host);
    c0tx.print(":");
    c0tx.println(port);
    int portInt = port.toInt();
    if (tcpClient.tcp_connect(host.c_str(), portInt))
    {
        callConnected = true;
        if (numberIndex == 6)
        {
            if (!tcpClient.ssh_connect(ssh_user.c_str(), ssh_pass.c_str()))
                callConnected = false;
            else
                tcpClient.setNoDelay(true); // Try to enable naggle
        }
        if (callConnected)
        {
            tcpClient.setNoDelay(true); // Try to disable naggle
            sendResult(R_CONNECT);
            connectTime = get_absolute_time();
            cmdMode = false;
            // c0tx.flush();
        }
    }

    if (!callConnected)
    {
        sendResult(R_NOANSWER);
        callConnected = false;
        tcpClient.stop();
    }
}

/**
 * Terminate the online connection
 */
void hangUp()
{
    tcpClient.stop();
    callConnected = false;
    sendResult(R_NOCARRIER);
    connectTime = nil_time;
}

void adtVSend(int drive, int block)
{
    int size = 512;
    int offset = block * 512 + vdrive[drive].headerSize;

    if(!vdrive[drive].mounted)
        return;

    uint16_t pd_date, pd_time;
    time_t rawtime = ntp.getEpochTime();
    struct tm *t;
    t = localtime(&rawtime);

    pd_date = ((t->tm_year - 100) << 9) | ((t->tm_mon + 1) << 5) | t->tm_mday;
    pd_time = (t->tm_hour << 8) | t->tm_min;            

    txBuf[0] = 0xc5;
    txBuf[1] = rxBuf[1];
    txBuf[2] = rxBuf[2];
    txBuf[3] = rxBuf[3];
    txBuf[4] = pd_time & 0xff;
    txBuf[5] = (pd_time >> 8) & 0xff;
    txBuf[6] = pd_date & 0xff;
    txBuf[7] = (pd_date >> 8) & 0xff;
    txBuf[8] = 0;

    for(size_t i=0; i<8; i++)
        txBuf[8] ^= txBuf[i];

    size_t count = size, index = 9;
    while(index < size + 9)
    {
        size_t read = smb2_pread(vdrive[drive].smb2, vdrive[drive].fh, &txBuf[index], count, offset);
        if (read == -EAGAIN)
        {
            continue;
        }

        if (read < 0)
        {
            break;
        }

        index += read ;
        count -= read;
        offset += read;
    }

    // Checksum the file data
    uint8_t checksum = 0;
    for(int i=0; i<size; i++)
    {
        checksum ^= txBuf[9+i];
    }

    // If the read did not get the right number of bytes,
    // Make it the right size but make sure the checksum fails
    if(index < size + 9)
    {
        index = size + 9;
        checksum++;
    }

    // Add the file data checksup
    txBuf[index++] = checksum;

    // Send the whole packet
    c0tx.Write(txBuf, index);
}

void adtVRecv(int drive, int block)
{
    if(!vdrive[drive].mounted)
        return;
}

void adtVOnline(byte old_serial_speed)
{
    while(1)
    {
        c0rx.readBytes(rxBuf, 1);
        if(rxBuf[0] == 0xC5)
        {
            c0rx.readBytes(&rxBuf[1], 4);
            int drive = rxBuf[1] >> 2;
            int block = rxBuf[2] + 256 * rxBuf[3];

            bool checksum = rxBuf[4] == rxBuf[0] ^ rxBuf[1] ^ rxBuf[2] ^ rxBuf[3];
            if(!checksum)
                continue;
            if(/*rxBuf[i+1] == 0x01 ||*/ rxBuf[1] == 0x03 || rxBuf[1] == 0x05)
            {
                adtVSend(drive, block);
            }
            else if(rxBuf[1] == 0x02 || rxBuf[1] == 0x04)
            {
                adtVRecv(drive, block);
            }
            else if(rxBuf[1] == 0x80)
            {
                break;
            }
        }
    }
#ifdef USE_UART    
    c0cmd.Write('B');
    c0cmd.Write(old_serial_speed);     // 8 = 115200
#endif
}

/**
 * Use SMB to mount a disk
 */
void adtVServeSetup(String upCmd)
{
    int driveNum = upCmd.charAt(4) - '0';
    if(driveNum == 1 || driveNum == 2)
    {
        driveNum--;
        if(vdrive[driveNum].mounted)
        {
            smb2_close(vdrive[driveNum].smb2, vdrive[driveNum].fh);
            smb2_disconnect_share(vdrive[driveNum].smb2);
            smb2_destroy_url(vdrive[driveNum].url);
            smb2_destroy_context(vdrive[driveNum].smb2);
            vdrive[driveNum].mounted = false;
        }
        vdrive[driveNum].smb2 = smb2_init_context();
        if (vdrive[driveNum].smb2 == NULL)
            goto vserror;

        upCmd.toLowerCase();
        vdrive[driveNum].url = smb2_parse_url(vdrive[driveNum].smb2, upCmd.substring(5).c_str());
        if (vdrive[driveNum].url == NULL)
            goto vserror;

        smb2_set_security_mode(vdrive[driveNum].smb2, SMB2_NEGOTIATE_SIGNING_ENABLED);

        if (smb2_connect_share(vdrive[driveNum].smb2, vdrive[driveNum].url->server, vdrive[driveNum].url->share, vdrive[driveNum].url->user) < 0)
            goto vserror;

        vdrive[driveNum].fh = smb2_open(vdrive[driveNum].smb2, vdrive[driveNum].url->path, O_RDONLY);
        if (vdrive[driveNum].fh == NULL)
            goto vserror;

        if (smb2_fstat(vdrive[driveNum].smb2, vdrive[driveNum].fh, &vdrive[driveNum].st) < 0)
            goto vserror;

        if(TX_BUF_SIZE != smb2_pread(vdrive[driveNum].smb2, vdrive[driveNum].fh, txBuf, TX_BUF_SIZE, 0))
            goto vserror;

        if(0 == strncmp((const char*)txBuf, "2IMG", 4))
        {
            vdrive[driveNum].headerSize = (txBuf[0x18]);
        }
        else
        {
            vdrive[driveNum].headerSize = 0;
        }
        vdrive[driveNum].mounted = true;
        sendResult(R_OK);
    }
    else if (upCmd == "ATVSO")
    {
#ifdef USE_UART        
        c0cmd.Write('B');
        c0cmd.Write(8);     // 8 = 115200
#endif        
        adtVOnline(serialspeed);
    }
    else
    {
        sendResult(R_ERROR);
    }
    return;

vserror:
    smb2_close(vdrive[driveNum].smb2, vdrive[driveNum].fh);
    smb2_disconnect_share(vdrive[driveNum].smb2);
    smb2_destroy_url(vdrive[driveNum].url);
    smb2_destroy_context(vdrive[driveNum].smb2);
    sendResult(R_ERROR);
}

/**
 * Handle the AT command the user entered
 */
void command()
{
    cmd.trim();
    if (cmd == "")
        return;
    c0tx.println();
    String upCmd = cmd;
    upCmd.toUpperCase();

    /**** Just AT ****/
    if (upCmd == "AT")
        sendResult(R_OK);

    /**** Dial to host ****/
    else if ((upCmd.indexOf("ATDT") == 0) || (upCmd.indexOf("ATDS") == 0))
    {
        dialOut(upCmd);
    }

    /**** Change telnet mode ****/
    else if (upCmd == "ATNET0")
    {
        telnet = false;
        sendResult(R_OK);
    }
    else if (upCmd == "ATNET1")
    {
        telnet = true;
        sendResult(R_OK);
    }

    else if (upCmd == "ATNET?")
    {
        c0tx.println(String(telnet));
        sendResult(R_OK);
    }

    /**** Display Help ****/
    else if (upCmd == "AT?" || upCmd == "ATHELP")
    {
        displayHelp();
        sendResult(R_OK);
    }

    /**** Reset, reload settings from FLASH ****/
    else if (upCmd == "ATZ")
    {
        loadSettings();
        sendResult(R_OK);
    }

    /**** List WiFi SSIDs ****/
    else if (upCmd == "ATC?")
    {
        scanNetworks();
        sendResult(R_OK);
    }

    /**** Disconnect WiFi ****/
    else if (upCmd == "ATC0")
    {
        disconnectWiFi();
        ntp.end();
        sendResult(R_OK);
    }

    /**** Connect WiFi ****/
    else if (upCmd == "ATC1")
    {
        if(!connectWiFi())
        {
            ntp.begin();
            ntp.forceUpdate();
            sendResult(R_OK);
        }
        else
        {
            sendResult(R_ERROR);
        }
    }

    /**** Control local echo in command mode ****/
    else if (upCmd.indexOf("ATE") == 0)
    {
        if (upCmd.substring(3, 4) == "?")
        {
            sendString(String(echo));
            sendResult(R_OK);
        }
        else if (upCmd.substring(3, 4) == "0")
        {
            echo = 0;
            sendResult(R_OK);
        }
        else if (upCmd.substring(3, 4) == "1")
        {
            echo = 1;
            sendResult(R_OK);
        }
        else
        {
            sendResult(R_ERROR);
        }
    }

    /**** Control verbosity or serve up ADTProtocol ****/
    else if (upCmd.indexOf("ATV") == 0)
    {
        if (upCmd.substring(3, 4) == "?")
        {
            sendString(String(verboseResults));
            sendResult(R_OK);
        }
        else if (upCmd.substring(3, 4) == "0")
        {
            verboseResults = false;
            sendResult(R_OK);
        }
        else if (upCmd.substring(3, 4) == "1")
        {
            verboseResults = true;
            sendResult(R_OK);
        }
        else if (upCmd.indexOf("ATVS") == 0)
        {
            adtVServeSetup(upCmd);
        }
        else
        {
            sendResult(R_ERROR);
        }

    }

    /**** Set current baud rate ****/
    else if (upCmd.indexOf("AT$SB=") == 0)
    {
        setBaudRate(upCmd.substring(6).toInt());
    }

    /**** Display current baud rate ****/
    else if (upCmd.indexOf("AT$SB?") == 0)
    {
        sendString(String(bauds[serialspeed]));
        ;
    }

    /**** Display Network settings ****/
    else if (upCmd == "ATI")
    {
        displayNetworkStatus();
        sendResult(R_OK);
    }

    /**** Display profile settings ****/
    else if (upCmd == "AT&V")
    {
        displayCurrentSettings();
        sendResult(R_OK);
    }

    /**** Display Saved settings ****/
    else if (upCmd == "AT&V?")
    {
        displaySavedSettings();
        sendResult(R_OK);
    }

    /**** Save (Write) current settings to FLASH ****/
    else if (upCmd == "AT&W")
    {
        saveSettings();
        sendResult(R_OK);
    }

    /**** Set or display a speed dial number ****/
    else if (upCmd.indexOf("AT&Z") == 0)
    {
        byte speedNum = upCmd.substring(4, 5).toInt();
        if (speedNum >= 0 && speedNum <= 9)
        {
            if (upCmd.substring(5, 6) == "=")
            {
                String speedDial = cmd;
                storeSpeedDial(speedNum, speedDial.substring(6));
                sendResult(R_OK);
            }
            if (upCmd.substring(5, 6) == "?")
            {
                sendString(speedDials[speedNum]);
                sendResult(R_OK);
            }
        }
        else
        {
            sendResult(R_ERROR);
        }
    }

    /**** Set WiFi SSID ****/
    else if (upCmd.indexOf("AT$SSID=") == 0)
    {
        // Max 32 characters
        ssid = cmd.substring(8,40);
        sendResult(R_OK);
    }

    /**** Display WiFi SSID ****/
    else if (upCmd == "AT$SSID?")
    {
        sendString(ssid);
        sendResult(R_OK);
    }

    /**** Set WiFi Password ****/
    else if (upCmd.indexOf("AT$PASS=") == 0)
    {
        // Max 64 characters
        password = cmd.substring(8,72);
        sendResult(R_OK);
    }

    /**** Display WiFi Password ****/
    else if (upCmd == "AT$PASS?")
    {
        sendString(password);
        sendResult(R_OK);
    }

    /**** Set SSH User ****/
    else if (upCmd.indexOf("AT$SSHU=") == 0)
    {
        // Max 64
        ssh_user = cmd.substring(8,72);
        sendResult(R_OK);
    }

    /**** Display SSH User ****/
    else if (upCmd == "AT$SSHU?")
    {
        sendString(ssh_user);
        sendResult(R_OK);
    }

    /**** Set SSH Password ****/
    else if (upCmd.indexOf("AT$SSHP=") == 0)
    {
        // Max 64
        ssh_pass = cmd.substring(8, 72);
        sendResult(R_OK);
    }

    /**** (Don't) Display SSH Password ****/
    else if (upCmd == "AT$SSHP?")
    {
        sendString("****** (It's a secret!)");
        sendResult(R_OK);
    }

    /**** Reset current memory settings to factory defaults ****/
    else if (upCmd == "AT&F")
    {
        defaultSettings();
        sendResult(R_OK);
    }

    /**** Set HEX Translate On ****/
    else if (upCmd == "ATHEX=1")
    {
        hex = true;
        sendResult(R_OK);
    }

    /**** Set HEX Translate Off ****/
    else if (upCmd == "ATHEX=0")
    {
        hex = false;
        sendResult(R_OK);
    }

    /**** Hang up a call ****/
    else if (upCmd.indexOf("ATH") == 0)
    {
        hangUp();
    }

    /**** Exit modem command mode, go online ****/
    else if (upCmd == "ATO")
    {
        if (callConnected == 1)
        {
            sendResult(R_CONNECT);
            cmdMode = false;
        }
        else
        {
            sendResult(R_ERROR);
        }
    }


    /**** HTTP GET request ****/
    else if (upCmd.indexOf("ATGET") == 0)
    {
        // From the URL, aquire required variables
        // (12 = "ATGEThttp://")
        int portIndex = cmd.indexOf(":", 12); // Index where port number might begin
        int pathIndex = cmd.indexOf("/", 12); // Index first host name and possible port ends and path begins
        int port;
        String path, host;
        if (pathIndex < 0)
        {
            pathIndex = cmd.length();
        }
        if (portIndex < 0)
        {
            port = 80;
            portIndex = pathIndex;
        }
        else
        {
            port = cmd.substring(portIndex + 1, pathIndex).toInt();
        }
        host = cmd.substring(12, portIndex);
        path = cmd.substring(pathIndex, cmd.length());
        if (path == "")
            path = "/";

        // Establish connection
        if (!tcpClient.tcp_connect(host.c_str(), port))
        {
            sendResult(R_NOCARRIER);
            callConnected = false;
        }
        else
        {
            sendResult(R_CONNECT);
            connectTime = get_absolute_time();
            cmdMode = false;
            callConnected = true;

            // Send a HTTP request before continuing the connection as usual
            String request = "GET ";
            request += path;
            request += " HTTP/1.1\r\nHost: ";
            request += host;
            request += "\r\nConnection: close\r\n\r\n";
            tcpClient.print(request);
        }
    }

    /**** Gopher request ****/
    else if (upCmd.indexOf("ATGPH") == 0)
    {
        // From the URL, aquire required variables
        // (14 = "ATGPHgopher://")
        int portIndex = cmd.indexOf(":", 14); // Index where port number might begin
        int pathIndex = cmd.indexOf("/", 14); // Index first host name and possible port ends and path begins
        int port;
        String path, host;
        if (pathIndex < 0)
        {
            pathIndex = cmd.length();
        }
        if (portIndex < 0)
        {
            port = 70;
            portIndex = pathIndex;
        }
        else
        {
            port = cmd.substring(portIndex + 1, pathIndex).toInt();
        }
        host = cmd.substring(14, portIndex);
        path = cmd.substring(pathIndex, cmd.length());
        if (path == "")
            path = "/";

        // Establish connection
        if (!tcpClient.tcp_connect(host.c_str(), port))
        {
            sendResult(R_NOCARRIER);
            callConnected = false;
        }
        else
        {
            sendResult(R_CONNECT);
            connectTime = get_absolute_time();
            cmdMode = false;
            callConnected = true;
            tcpClient.print(path + "\r\n");
        }
    }

    /**** Control quiet mode ****/
    else if (upCmd.indexOf("ATQ") == 0)
    {
        if (upCmd.substring(3, 4) == "?")
        {
            sendString(String(quietMode));
            sendResult(R_OK);
        }
        else if (upCmd.substring(3, 4) == "0")
        {
            quietMode = 0;
            sendResult(R_OK);
        }
        else if (upCmd.substring(3, 4) == "1")
        {
            quietMode = 1;
            sendResult(R_OK);
        }
        else
        {
            sendResult(R_ERROR);
        }
    }

    /**** Unknown command ****/
    else
        sendResult(R_ERROR);

    cmd = "";
}

/**
 * Inifinite loop - either in command or connected mode.  In Command mode react to AT command
 * and in online mode, handle the connection (example telnet)
 */
void loop()
{
    while (1)
    {
        // Run the NTP update if the WiFi is connected
        if (WiFi.status() == CYW43_LINK_UP || WiFi.status() == CYW43_LINK_JOIN)
        {
            ntp.update();
        }
        /**** AT command mode ****/
        if (cmdMode == true)
        {
            // In command mode - don't exchange with TCP but gather characters to a string
            if (c0rx.available())
            {
                led_set(true);
                char chr = c0rx.Read();

                // Return, enter, new line, carriage return.. anything goes to end the command
                if ((chr == '\n') || (chr == '\r'))
                {
                    command();
                }
                // Backspace or delete deletes previous character
                else if ((chr == 8) || (chr == 127) || (chr == 20))
                {
                    cmd.remove(cmd.length() - 1);
                    if (echo == true)
                    {
                        c0tx.Write(chr);
                    }
                }
                else if (chr >= 32 && chr < 128)
                {
                    if (cmd.length() < MAX_CMD_LENGTH)
                        cmd.concat(chr);
                    if (echo == true)
                    {
                        c0tx.Write(chr);
                    }
                    if (hex)
                    {
                        c0tx.print(chr, HEX);
                    }
                }
            }
        }
        /**** Connected mode ****/
        else
        {
            // Transmit from terminal to TCP
            if (c0rx.available())
            {
                led_set(true);

                // In telnet in worst case we have to escape every byte
                // so leave half of the buffer always free
                int max_buf_size;
                if (telnet == true)
                    max_buf_size = TX_BUF_SIZE / 2;
                else
                    max_buf_size = TX_BUF_SIZE;

                // Read from serial, the amount available up to
                // maximum size of the buffer
                size_t len = min(c0rx.available(), max_buf_size);
                c0rx.readBytes(&txBuf[0], len);

                // Enter command mode with "+++" sequence
                for (int i = 0; i < (int)len; i++)
                {
                    if (txBuf[i] == '+')
                    {
                        plusCount++;
                        if (plusCount >= 3)
                        {
                            plusTime = get_absolute_time();
                        }
                    }
                    else
                    {
                        plusCount = 0;
                    }
                }

                // Double (escape) every 0xff for telnet, shifting the following bytes
                // towards the end of the buffer from that point
                if (telnet == true)
                {
                    for (int i = len - 1; i >= 0; i--)
                    {
                        if (txBuf[i] == 0xff)
                        {
                            for (int j = TX_BUF_SIZE - 1; j > i; j--)
                            {
                                txBuf[j] = txBuf[j - 1];
                            }
                            len++;
                        }
                    }
                }
                tcpClient.Write(&txBuf[0], len);
            }

            // Transmit from TCP to terminal
            while (tcpClient.available())
            {
                led_set(true);
                uint8_t rxByte = tcpClient.Read();

                // Is a telnet control code starting?
                if ((telnet == true) && (rxByte == 0xff))
                {
                    rxByte = tcpClient.Read();
                    if (rxByte == 0xff)
                    {
                        // 2 times 0xff is just an escaped real 0xff
                        c0tx.Write(0xff);
                        // c0tx.flush();
                    }
                    else
                    {
                        // rxByte has now the first byte of the actual non-escaped control code
                        uint8_t cmdByte1 = rxByte;
                        rxByte = tcpClient.Read();
                        uint8_t cmdByte2 = rxByte;
                        // rxByte has now the second byte of the actual non-escaped control code
                        // We are asked to do some option, respond we won't
                        if (cmdByte1 == DO)
                        {
                            tcpClient.Write((uint8_t)255);
                            tcpClient.Write((uint8_t)WONT);
                            tcpClient.Write(cmdByte2);
                        }
                        // Server wants to do any option, allow it
                        else if (cmdByte1 == WILL)
                        {
                            tcpClient.Write((uint8_t)255);
                            tcpClient.Write((uint8_t)DO);
                            tcpClient.Write(cmdByte2);
                        }
                    }
                }
                else
                {
                    // Non-control codes pass through freely
                    c0tx.Write(rxByte);
                    // c0tx.flush();
                }
            }
        }

        // If we received "+++" as last bytes from serial port and there
        // has been over a second without any more bytes
        if (plusCount >= 3)
        {
            if (time_reached(delayed_by_ms(plusTime, PLUS_TIME)))
            {
                cmdMode = true;
                sendResult(R_OK);
                plusCount = 0;
            }
        }

        // Go to command mode if TCP disconnected and not in command mode
        if (!tcpClient.connected() && cmdMode == false && callConnected == true)
        {
            hangUp();
            cmdMode = true;
            sendResult(R_NOCARRIER);
            connectTime = nil_time;
            callConnected = false;
        }

        // Turn off tx/rx led if it has been lit long enough to be visible
        if (time_reached(delayed_by_ms(ledTime, LED_TIME)))
            led_set(false);
    }
}

/**
 * Init defaults, try to load saved settings, init UART and enter endless loop
 */
void pico_modem_main()
{
    vdrive[0].mounted = vdrive[1].mounted = false;

    welcome();
    loop();
}
}
