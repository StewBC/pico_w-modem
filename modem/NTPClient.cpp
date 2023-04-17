/**
 * The MIT License (MIT)
 * Copyright (c) 2015 by Fabrice Weinberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "NTPClient.h"
#include <lwip/dns.h>

/*
 * Callback function when a UDP packet is received (in reply to a reqest to poolServer)
 */
static void udp_raw_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    NTPClient* ntp = (NTPClient*)arg;
    if(!ntp)
        return;

    byte* buffer = (byte*)p->payload;
    if(!ntp->isValid(buffer))
        return;

    unsigned long secsSince1900;
    secsSince1900  = (unsigned long)buffer[40] << 24;
    secsSince1900 |= (unsigned long)buffer[41] << 16;
    secsSince1900 |= (unsigned long)buffer[42] << 8;
    secsSince1900 |= (unsigned long)buffer[43];
    ntp->_currentEpoc = secsSince1900 - SEVENZYYEARS + ntp->_timeZone * 3600; // 3600 = SECS_PER_HOUR;
}

/*
 * Check that the recived packet is a valid NTP packet.
 * Version number not checked as I get back 0x20 (0b00010100) in
 * byte 0, which would be Version 2.  But it is V4.
 * https://www.rfc-editor.org/rfc/rfc5905#section-7.3
 */
bool NTPClient::isValid(byte *ntpPacket)
{
    // Perform a few validity checks on the packet
    if ((ntpPacket[0] & 0b11000000) == 0b11000000) // Check for LI=UNSYNC
        return false;

    // if ((ntpPacket[0] & 0b00111000) >> 3 < 0b100) // Check for Version >= 4
    //     return false;

    if ((ntpPacket[0] & 0b00000111) != 0b100) // Check for Mode == Server
        return false;

    if ((ntpPacket[1] < 1) || (ntpPacket[1] > 15)) // Check for valid Stratum
        return false;

    if (ntpPacket[16] == 0 && ntpPacket[17] == 0 &&
        ntpPacket[18] == 0 && ntpPacket[19] == 0 &&
        ntpPacket[20] == 0 && ntpPacket[21] == 0 &&
        ntpPacket[22] == 0 && ntpPacket[22] == 0) // Check for ReferenceTimestamp != 0
        return false;

    return true;
}

/*
 * Alloc udp_pcb, resolve poolServerName, bind locally, connect and set up a receive callback
 */
void NTPClient::begin(int port)
{
    if(_udp)
        return;

    err_t err = ERR_OK;
    _udp = udp_new();
    if (_udp == NULL)
    {
        return;
    }

    if(!WiFi.hostByName(_poolServerName, _destIP))
    {
        udp_remove(_udp);
        _udp = nullptr;
        return;
    }

    err = udp_bind(_udp, IP4_ADDR_ANY, port);
    if (err != ERR_OK){
        udp_remove(_udp);
        _udp = nullptr;
        return;
    }

    IP4_ADDR(&_destIPAddr, _destIP[0], _destIP[1], _destIP[2], _destIP[3]);

    err = udp_connect(_udp, &_destIPAddr, 123); // ntp is on port 123
    if (err != ERR_OK){
        udp_remove(_udp);
        _udp = nullptr;
        return;
    }

    udp_recv(_udp, udp_raw_recv, this);
}

/*
 * Clean up and deallocate udp_pcb
 */
void NTPClient::end()
{
    if(_udp)
    {
        udp_disconnect(_udp);
        udp_remove(_udp);
        _udp = nullptr;
    }
}

/*
 * Sets a "time zone" offset.  For example Pacific Time is -8.
 */
void NTPClient::setTimeZone(int timeZone)
{
    _timeZone = timeZone;
}

/*
 * Set seconds since since Thursday 1 January 1970 00:00:00 UT
 */
void NTPClient::setEpochTime(unsigned long secs)
{
    _currentEpoc = secs;
}

/*
 * Return seconds since Thursday 1 January 1970 00:00:00 UT
 */
unsigned long NTPClient::getEpochTime()
{
    return _timeZone +                      // User offset
           _currentEpoc +                   // Epoc returned by the NTP server
           us_to_ms(absolute_time_diff_us(_lastUpdate, get_absolute_time())) / 1000;
}

/*
 * Functions to return current time for day, hours, minutes and seconds
 */
int NTPClient::getDay()
{
    return (((getEpochTime() / 86400L) + 4) % 7); // 0 is Sunday
}
int NTPClient::getHours()
{
    return ((getEpochTime() % 86400L) / 3600);
}
int NTPClient::getMinutes()
{
    return ((getEpochTime() % 3600) / 60);
}
int NTPClient::getSeconds()
{
    return (getEpochTime() % 60);
}

/*
 * Current local time in HH:MM:SS format
 */
String NTPClient::getFormattedTime(unsigned long secs)
{
    unsigned long rawTime = secs ? secs : getEpochTime();
    unsigned long hours = (rawTime % 86400L) / 3600;
    String hoursStr = hours < 10 ? "0" + String(hours) : String(hours);

    unsigned long minutes = (rawTime % 3600) / 60;
    String minuteStr = minutes < 10 ? "0" + String(minutes) : String(minutes);

    unsigned long seconds = rawTime % 60;
    String secondStr = seconds < 10 ? "0" + String(seconds) : String(seconds);

    return hoursStr + ":" + minuteStr + ":" + secondStr;
}

// 
// 
/*
 * Current time as YYYY-MM-DDTHH:MM:SS
 * Based on https://github.com/PaulStoffregen/Time/blob/master/Time.cpp
 */
String NTPClient::getFormattedDate(unsigned long secs)
{
    unsigned long rawTime = (secs ? secs : getEpochTime()) / 86400L; // in days
    unsigned long days = 0, year = 1970;
    uint8_t month;
    static const uint8_t monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    while ((days += (LEAP_YEAR(year) ? 366 : 365)) <= rawTime)
        year++;
    rawTime -= days - (LEAP_YEAR(year) ? 366 : 365); // now it is days in this year, starting at 0
    days = 0;
    for (month = 0; month < 12; month++)
    {
        uint8_t monthLength;
        if (month == 1)
        { // february
            monthLength = LEAP_YEAR(year) ? 29 : 28;
        }
        else
        {
            monthLength = monthDays[month];
        }
        if (rawTime < monthLength)
            break;
        rawTime -= monthLength;
    }
    String monthStr = ++month < 10 ? "0" + String(month) : String(month);     // jan is month 1
    String dayStr = ++rawTime < 10 ? "0" + String(rawTime) : String(rawTime); // day of month
    return String(year) + "-" + monthStr + "-" + dayStr + "T" + getFormattedTime(secs ? secs : 0);
}

/*
 * How often update will send an update packet to the poolServer
 */
void NTPClient::setUpdateInterval(unsigned long updateInterval)
{
    _updateInterval = updateInterval;
}

/*
 * Check id udp is init and if not, try to start it
 * If udp is init, call forceUpdate to send an NTP packet to poolServerName
 */
bool NTPClient::update()
{
    if(time_reached(delayed_by_ms(_lastUpdate, _updateInterval)))
    {
        _lastUpdate = get_absolute_time();
        if (!_udp)
            begin(); // setup the udp_pcb client if needed
        if (_udp)
            return forceUpdate();
    }
    return true;
}

/*
 * Send a UDP NTP packet to the poolServer
 */
bool NTPClient::forceUpdate()
{
    bool rVal = false;

    if(!_udp)
        return false;

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_PACKET_SIZE, PBUF_POOL);
    if(!p)
        return false;

    // set all bytes in the buffer to 0
    memset(p->payload, 0, NTP_PACKET_SIZE);
    // Initialize values needed to form NTP request
    // (see URL above for details on the packets)

    byte* buffer = (byte*)p->payload;

    buffer[0] = 0b11100011; // LI, Version, Mode
    buffer[1] = 0;          // Stratum, or type of clock
    buffer[2] = 6;          // Polling Interval
    buffer[3] = 0xEC;       // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    buffer[12] = 0x49;
    buffer[13] = 0x4E;
    buffer[14] = 0x49;
    buffer[15] = 0x52;

    if(ERR_OK == udp_send(_udp, p))
        rVal = true;

    pbuf_free(p);

    return rVal;
}

