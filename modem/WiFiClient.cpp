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

#include "WiFi.h"
#include "WiFiClient.h"
#include <lwip/sockets.h>

// #ifdef USE_UART
// #include "Serial.h"
// extern Serial_ Serial;
// #endif

static byte userPassword[256];

static int wsUserAuth(byte authType,
                      WS_UserAuthData *authData,
                      void *ctx)
{
    int ret = WOLFSSH_USERAUTH_SUCCESS;

#ifdef DEBUG_WOLFSSH
    /* inspect supported types from server */
    printf("Server supports:\n");
    if (authData->type & WOLFSSH_USERAUTH_PASSWORD)
    {
        printf(" - password\n");
    }
    if (authData->type & WOLFSSH_USERAUTH_PUBLICKEY)
    {
        printf(" - publickey\n");
    }
    printf("wolfSSH requesting to use type %d\n", authType);
#endif

    // /* Wait for request of public key on names known to have one */
    // if ((authData->type & WOLFSSH_USERAUTH_PUBLICKEY) &&
    //         authData->username != NULL &&
    //         authData->usernameSz > 0) {

    //     /* in the case that the name is hansel or in the case that the user
    //      * passed in a public key file, use public key auth */
    //     if ((XSTRNCMP((char*)authData->username, "hansel",
    //             authData->usernameSz) == 0) ||
    //         pubKeyName != NULL || certName != NULL) {

    //         if (authType == WOLFSSH_USERAUTH_PASSWORD) {
    //             printf("rejecting password type with %s in favor of pub key\n",
    //                 (char*)authData->username);
    //             return WOLFSSH_USERAUTH_FAILURE;
    //         }
    //     }
    // }

    // if (authType == WOLFSSH_USERAUTH_PUBLICKEY) {
    //     WS_UserAuthData_PublicKey* pk = &authData->sf.publicKey;

    //     pk->publicKeyType = userPublicKeyType;
    //     pk->publicKeyTypeSz = userPublicKeyTypeSz;
    //     pk->publicKey = userPublicKey;
    //     pk->publicKeySz = userPublicKeySz;
    //     pk->privateKey = userPrivateKey;
    //     pk->privateKeySz = userPrivateKeySz;

    //     ret = WOLFSSH_USERAUTH_SUCCESS;
    // }
    // else
    if (authType == WOLFSSH_USERAUTH_PASSWORD)
    {
        const char *defaultPassword = (const char *)ctx;
        word32 passwordSz = 0;

        if (defaultPassword != NULL)
        {
            passwordSz = (word32)strlen(defaultPassword);
            memcpy(userPassword, defaultPassword, passwordSz);
        }
        // else {
        // printf("Password: ");
        // fflush(stdout);
        // SetEcho(0);
        // if (fgets((char*)userPassword, sizeof(userPassword), stdin) == NULL) {
        //     printf("Getting password failed.\n");
        //     ret = WOLFSSH_USERAUTH_FAILURE;
        // }
        // else {
        //     char* c = strpbrk((char*)userPassword, "\r\n");
        //     if (c != NULL)
        //         *c = '\0';
        // }
        // passwordSz = (word32)strlen((const char*)ctx);
        // SetEcho(1);
        // #ifdef USE_WINDOWS_API
        //     printf("\r\n");
        // #endif
        // }

        if (ret == WOLFSSH_USERAUTH_SUCCESS)
        {
            authData->sf.password.password = userPassword;
            authData->sf.password.passwordSz = passwordSz;
        }
    }

    return ret;
}

static int wsPublicKeyCheck(const byte *pubKey, word32 pubKeySz, void *ctx)
{
    int ret = 0;

#ifdef DEBUG_WOLFSSH
    printf("Sample public key check callback\n"
           "  public key = %p\n"
           "  public key size = %u\n"
           "  ctx = %s\n",
           pubKey, pubKeySz, (const char *)ctx);
#else
    (void)pubKey;
    (void)pubKeySz;
    (void)ctx;
#endif

#ifdef WOLFSSH_CERTS
#if defined(OPENSSL_ALL) || defined(WOLFSSL_IP_ALT_NAME)
    /* try to parse the certificate and check it's IP address */
    if (pubKeySz > 0)
    {
        DecodedCert dCert;
        byte *der = NULL;
        word32 derSz = 0;

        if (ParseRFC6187(pubKey, pubKeySz, &der, &derSz) == WS_SUCCESS)
        {
            wc_InitDecodedCert(&dCert, der, derSz, NULL);
            if (wc_ParseCert(&dCert, CERT_TYPE, NO_VERIFY, NULL) != 0)
            {
                printf("public key not a cert\n");
            }
            else
            {
                int ipMatch = 0;
                DNS_entry *current = dCert.altNames;

                while (current != NULL)
                {
                    if (current->type == ASN_IP_TYPE)
                    {
                        printf("host cert alt. name IP : %s\n",
                               current->ipString);
                        printf("\texpecting host IP : %s\n", (char *)ctx);
                        if (XSTRCMP(ctx, current->ipString) == 0)
                        {
                            printf("\tmatched!\n");
                            ipMatch = 1;
                        }
                    }
                    current = current->next;
                }

                if (ipMatch == 0)
                {
                    printf("IP did not match expected IP\n");
                    ret = -1;
                }
            }
            FreeDecodedCert(&dCert);
        }
    }
#endif
#endif

    return ret;
}

WiFiClient::WiFiClient() : _socket(NA_STATE)
{
}

WiFiClient::WiFiClient(uint8_t sock) : _socket(sock)
{
}

int WiFiClient::ssh_connect(const char *username, const char *password)
{
    String cmd = "bash";
    if (!(ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_CLIENT, NULL)))
        return 0;
    wolfSSH_SetUserAuth(ctx, wsUserAuth);
    if (!(ssh = wolfSSH_new(ctx)))
        return 0;
    wolfSSH_SetUserAuthCtx(ssh, (void *)password);
    // wolfSSH_CTX_SetPublicKeyCheck(ctx, wsPublicKeyCheck);
    // wolfSSH_SetPublicKeyCheckCtx(ssh, (void*)"You've been sampled!");
    if (WS_SUCCESS != wolfSSH_SetUsername(ssh, username))
        return 0;
    if (WS_SUCCESS != wolfSSH_set_fd(ssh, _socket))
        return 0;
    if (WS_SUCCESS != wolfSSH_SetChannelType(ssh, WOLFSSH_SESSION_TERMINAL, NULL, 0))
    // if (WS_SUCCESS != wolfSSH_SetChannelType(ssh, WOLFSSH_SESSION_EXEC, (uint8_t*)cmd.c_str(), cmd.length()))
        return 0;
    if (WS_SUCCESS != wolfSSH_connect(ssh))
        return 0;

    return 1;
}

int WiFiClient::tcp_connect(const char *host, uint16_t port)
{
    IPAddress remote_addr;
    if (WiFi.hostByName(host, remote_addr))
        return tcp_connect(remote_addr, port);
    return 0;
}

int WiFiClient::tcp_connect(IPAddress ip, uint16_t port)
{
    if (_socket == NA_STATE)
    {
        struct sockaddr_in serv_addr;
        memset(&serv_addr, '0', sizeof(serv_addr));
        _socket = socket(AF_INET, SOCK_STREAM, 0);
        if (_socket < 0)
        {
            _socket = NA_STATE;
            return _socket;
        }
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = ip;
        serv_addr.sin_port = htons(port);
        return connect(_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0 ? 0 : 1;
    }
    return NA_STATE;
}

size_t WiFiClient::Write(uint8_t b)
{
    return Write(&b, 1);
}

size_t WiFiClient::Write(const uint8_t *buf, size_t size)
{
    if (_socket >= MAX_SOCK_NUM)
    {
        setWriteError();
        return -1;
    }

    if (!ssh)
    {
        if (write(_socket, buf, size) < 0)
        {
            setWriteError();
            return -1;
        }
    }
    else
    {
        /* lock SSH structure access */
        // wc_LockMutex(&args->lock);
        if (wolfSSH_stream_send(ssh, (uint8_t *)buf, size) <= 0)
        {
            setWriteError();
            return -1;
        }
        // wc_UnLockMutex(&args->lock);
    }

    return size;
}

int WiFiClient::available()
{
    if (_socket != NA_STATE)
    {
        int count = 0;
        if (!ssh)
            ioctl(_socket, FIONREAD, &count);
        else
        {
            count = wolfSSH_stream_peek(ssh, NULL, 16384);
            if(!count)
                ioctl(_socket, FIONREAD, &count);
        }
        return count;
    }

    return -1;
}

int WiFiClient::ssh_Read(uint8_t *buf, int size)
{
    int ret = 0;

    ret = wolfSSH_stream_read(ssh, buf, size);
    if (ret == WS_EXTDATA)
    { /* handle extended data */
        do
        {
            WMEMSET(buf, 0, size);
            ret = wolfSSH_extended_data_read(ssh, buf, size);
            if (ret < 0)
            {
                printf("Extended data read failed.");
            }
            // buf[size - 1] = '\0';
        } while (ret > 0);
    }
    else if (ret <= 0)
    {
        if (ret != WS_EOF)
        {
            printf("Stream read failed.");
        }
    }
    // else
    // {
    //     buf[size - 1] = '\0';
    // }
    return ret;
}

int WiFiClient::Read()
{
    uint8_t b;
    if (!available())
        return -1;

    if (!ssh)
        read(_socket, &b, 1);
    else
        ssh_Read(&b, 1);

    return b;
}

int WiFiClient::Read(uint8_t *buf, size_t size)
{
    int ret = 0;

    if (!ssh)
    {
        if (ret = (read(_socket, buf, size) < 0))
            return -1;
    }
    else
    {
        if (ret = (ssh_Read(buf, size) < 0))
            return -1;
    }

    return ret;
}

int WiFiClient::peek()
{
    uint8_t b;
    if (!available())
        return -1;

    return b;
}

void WiFiClient::flush()
{
    // SQW lwip doesn't seem to have SIOCOUTQ
    //   int pending = 1;
    //   while(ioctl(_socket, SIOCOUTQ, &pending) >= 0 && pending > 0) {;}
}

void WiFiClient::setNoDelay(int delayState)
{
    setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, &delayState, sizeof(delayState));
}

void WiFiClient::stop()
{
    if (ssh)
    {
        wolfSSH_free(ssh);
        ssh = NULL;
    }
    if (ctx)
    {
        wolfSSH_CTX_free(ctx);
        ctx = NULL;
    }
    if (_socket == NA_STATE)
        return;

    close(_socket);
    _socket = NA_STATE;
}

uint8_t WiFiClient::connected()
{
    if (_socket == NA_STATE)
    {
        return 0;
    }
    else
    {
        uint8_t s = status();
        // SQW I am not sure about this...
        return !(s == ENOMEM || s == ENOBUFS || s == EHOSTUNREACH || s == EINVAL || s == EADDRINUSE ||
                 s == ENOTCONN || s == -1 || s == ECONNABORTED || s == ECONNRESET || s == ENOTCONN || s == EIO);
        // uint8_t s = status();
        // return !(s == LISTEN || s == CLOSED || s == FIN_WAIT_1 ||
        // 		s == FIN_WAIT_2 || s == TIME_WAIT ||
        // 		s == SYN_SENT || s== SYN_RCVD ||
        // 		(s == CLOSE_WAIT));
    }
}

uint8_t WiFiClient::status()
{
    if (_socket == NA_STATE)
    {
        return 0;
    }
    else
    {
        // SQW - What I really need is access to sock->conn->pcb.tcp
        // I am not sure this is the best way to check the status of a socket but it's the only way I could think...
        int optVal = 0;
        socklen_t len = sizeof(optVal);
        if (getsockopt(_socket, SOL_SOCKET, SO_ERROR, &optVal, &len) < 0)
            return 0;
        return optVal;
    }
}

WiFiClient::operator bool()
{
    return _socket != NA_STATE;
}
