/*
  WiFiClient.cpp - Library for Arduino Wifi shield.
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

#ifndef wificlient_h
#define wificlient_h

#include "Print.h"
#include "Client.h"
#include "IPAddress.h"
#include <wolfssh/ssh.h>

class WiFiClient : public Client
{
public:

    WiFiClient();
    WiFiClient(uint8_t sock);

    static uint gCounter;
    
    virtual int ssh_connect(const char *username, const char *password);
    virtual int tcp_connect(IPAddress ip, uint16_t port);
    virtual int tcp_connect(const char *host, uint16_t port);
    virtual size_t Write(uint8_t);
    virtual size_t Write(const uint8_t *buf, size_t size);
    virtual int available();
    virtual int ssh_Read(uint8_t *buf, int bufSz);
    virtual int Read();
    virtual int Read(uint8_t *buf, size_t size);
    virtual int peek();
    virtual void flush();
    virtual void setNoDelay(int delayState);
    virtual void stop();
    virtual uint8_t connected();
    virtual uint8_t status();
    virtual operator bool();

    friend class WiFiServer;

    using Print::Write;

private:
    WOLFSSH_CTX *ctx = NULL;
    WOLFSSH *ssh = NULL;
    uint16_t _socket;
};

#endif
