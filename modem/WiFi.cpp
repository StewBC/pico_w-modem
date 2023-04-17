/*
  WiFi.cpp - Library for Arduino Wifi shield.
  Copyright (c) 2011-2014 Arduino LLC.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, Write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "WiFi.h"
#include <lwip/netdb.h>

int NameSort(_wifi_node *lhs, _wifi_node *rhs)
{
    int r = strncmp((const char *)lhs->result.ssid, (const char *)rhs->result.ssid, lhs->result.ssid_len);
    if (!r)
        return lhs->result.rssi - rhs->result.rssi;
    else
        return r;
}

int scanResult(void *, const cyw43_ev_scan_result_t *result)
{
    if (result)
    {
        _wifi_node *r = new _wifi_node;
        if (r)
        {
            memcpy(&r->result, result, sizeof(cyw43_ev_scan_result_t));
            WiFiClass::_wifi_nodes.AddSorted(r, NameSort);
        }
    }
    return 0;
}

wcList<_wifi_node> WiFiClass::_wifi_nodes;
int WiFiClass::itf = CYW43_ITF_STA;
String WiFiClass::wifi_ssid = "";

WiFiClass::WiFiClass()
{
}

void WiFiClass::init()
{
    cyw43_arch_enable_sta_mode();
    itf = CYW43_ITF_STA;
}

char *WiFiClass::firmwareVersion()
{
    return nullptr;
}

int WiFiClass::begin(const char *ssid)
{

    wifi_ssid = ssid;
    return cyw43_arch_wifi_connect_timeout_ms(ssid, nullptr, CYW43_AUTH_OPEN, WIFI_CONNECTION_MS);
}

int WiFiClass::begin(const char *ssid, uint8_t key_idx, const char *key)
{
    wifi_ssid = ssid;
    return cyw43_arch_wifi_connect_timeout_ms(ssid, key, CYW43_AUTH_WPA_TKIP_PSK, WIFI_CONNECTION_MS);
}

int WiFiClass::begin(const char *ssid, const char *passphrase)
{
    wifi_ssid = ssid;
    return cyw43_arch_wifi_connect_timeout_ms(ssid, passphrase, CYW43_AUTH_WPA2_AES_PSK, WIFI_CONNECTION_MS);
}

int WiFiClass::disconnect()
{
    if(!cyw43_wifi_leave(&cyw43_state, itf))
    {
        wifi_ssid = "";
        cyw43_state.netif->ip_addr.addr = IPADDR4_INIT(0);
        cyw43_state.netif->netmask.addr = IPADDR4_INIT(0);
        cyw43_state.netif->gw.addr =  IPADDR4_INIT(0);
        return 0;
    }
    return 1;
}

uint8_t *WiFiClass::macAddress(uint8_t *mac)
{
    cyw43_hal_get_mac(0, mac);
    return mac;
}

IPAddress WiFiClass::localIP()
{
    return cyw43_state.netif->ip_addr.addr;
}

IPAddress WiFiClass::subnetMask()
{
    return *(uint32_t *)(&cyw43_state.netif->netmask.addr);
}

IPAddress WiFiClass::gatewayIP()
{
    return *(uint32_t *)(&cyw43_state.netif->gw.addr);
}

char *WiFiClass::SSID()
{
    return (char *)wifi_ssid.c_str();
}

int8_t WiFiClass::scanNetworks()
{
    uint8_t numOfNetworks = 0;
    cyw43_wifi_scan_options_t scan_options = {0};

    _wifi_nodes.FlushList();
    if (cyw43_wifi_scan(&cyw43_state, &scan_options, nullptr, scanResult))
        return 0;

    while (cyw43_wifi_scan_active(&cyw43_state))
    {
        ;
    }
    return _wifi_nodes.NumElements();
}

uint8_t WiFiClass::status()
{
    return cyw43_wifi_link_status(&cyw43_state, itf);
}

int WiFiClass::hostByName(const char *aHostname, IPAddress &aResult)
{
    struct hostent *host = gethostbyname(aHostname);
    if (host)
    {
        aResult[0] = host->h_addr_list[0][0];
        aResult[1] = host->h_addr_list[0][1];
        aResult[2] = host->h_addr_list[0][2];
        aResult[3] = host->h_addr_list[0][3];
        return 1;
    }
    else
        aResult.fromString("0.0.0.0");

    return 0;
}

WiFiClass WiFi;
