#ifndef _HTPCLIENT_H
#define _HTPCLIENT_H

#include <pico/time.h>
#include <lwip/udp.h>
#include "WiFi.h"
#include "compat.h"
#include "WString.h"

#define SEVENZYYEARS 2208988800UL
#define NTP_PACKET_SIZE 48
#define NTP_DEFAULT_LOCAL_PORT 1337
#define LEAP_YEAR(Y) ((Y > 0) && !(Y % 4) && ((Y % 100) || !(Y % 400)))

/**
 * Callback function when UDP packet comes in.  Declare here so it can be made a friend later
 */
static void udp_raw_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);

class NTPClient
{
private:
    struct udp_pcb *_udp = nullptr;

    const char *_poolServerName = "pool.ntp.org"; // Default time server
    IPAddress _destIP;
    ip4_addr_t _destIPAddr;
    int _timeZone = 0;
    unsigned long _updateInterval = 60000; // In ms
    unsigned long _currentEpoc = 0; // In s
    absolute_time_t _lastUpdate = nil_time;  // In ms

    bool isValid(byte *ntpPacket);

public:
    NTPClient() {;}
    NTPClient(const char *poolServerName, int timeZone, unsigned long updateInterval) :
    _poolServerName(poolServerName), _timeZone(timeZone), _updateInterval(updateInterval) {;}

    /**
     * Starts the underlying UDP client with the default local port
     */
    void begin() { begin(NTP_DEFAULT_LOCAL_PORT); }

    /**
     * Starts the underlying UDP client with the specified local port
     */
    void begin(int port);

    /**
     * Stops the underlying UDP client
     */
    void end();

    /**
     * Changes the time offset. Useful for changing timezones dynamically
     */
    void setTimeZone(int timeZone);

    /**
     * Replace the NTP-fetched time with seconds since Jan. 1, 1970
     */
    void setEpochTime(unsigned long secs);

    /**
     * @return time in seconds since Jan. 1, 1970
     */
    unsigned long getEpochTime();

    /*
    * Functions to return current time for day, hours, minutes and seconds
    */
    int getDay();
    int getHours();
    int getMinutes();
    int getSeconds();

    /**
     * @return secs argument (or 0 for current time) formatted like `hh:mm:ss`
     * Returns local time (with the location unspecified) since timeZone is
     * applied but the time offset is not reported.
     */
    String getFormattedTime(unsigned long secs = 0);

    /**
     * @return secs argument (or 0 for current date) formatted to ISO 8601
     * like `2023-01-26T12:36:21`
     * Time is local time.  See getFormattedTime
     */
    String getFormattedDate(unsigned long secs = 0);

    /**
     * Set the update interval to another frequency. E.g. useful when the
     * timeZone should not be set in the constructor
     */
    void setUpdateInterval(unsigned long updateInterval);

    /**
     * This should be called in the main loop of your application. By default an update from the NTP Server is only
     * made every 60 seconds. This can be configured in the NTPClient constructor.
     *
     * @return true on success, false on failure
     */
    bool update();

    /**
     * This will force the update from the NTP Server.
     *
     * @return true on request sent success, false on failure
     */
    bool forceUpdate();

    /**
     * Callback function can access private variables
     */
    friend void udp_raw_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);
};

#endif // _HTPCLIENT_H