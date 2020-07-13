/*
 * This file is part of the vs-ftp distribution (https://github.com/baskapteijn/vs-ftp).
 * Copyright (c) 2019 Bas Kapteijn.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "vsftp_server.h"
#include "vsftp_commands.h"
#include "vsftp_filesystem.h"
#include "config.h"

typedef struct {
    /*!
     * A copy of the VS-FTP configuration data as given by the caller.
     */
    VSFTPConfigData_s vsftpConfigData;

    int serverSock;
    int clientSock;
    int transferSock;
    struct sockaddr_in server;
    struct sockaddr_in client;
    struct sockaddr_in transfer;
    bool transferModeBinary;

    char cwd[PATH_LEN_MAX];
    size_t cwdBufSize; //buffer size, not the length of the cwd string!
    size_t cwdLen;

    bool isConnected;
    bool isServerSocketCreated;
} vsftpServerData_s;

static vsftpServerData_s serverData;

static int VSFTPServerCreatePassiveSocket(uint16_t portNum, int *sock, const struct sockaddr_in *sockData);

static inline void StripCRAndNewline(char *buf, size_t *len);
static int WaitForIncomingConnection(void);
static int HandleConnection(void);

/*!
 * \brief Initialize the VS-FTP server.
 * \details
 *      Initializes the Server with configuration data.
 * \param vsftpConfigData
 *      A pointer to the VS-FTP configuration data.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
int VSFTPServerInitialize(const VSFTPConfigData_s *vsftpConfigData)
{
    /* Copy server configuration data. */
    (void)memcpy(&serverData.vsftpConfigData, vsftpConfigData,
                 sizeof(serverData.vsftpConfigData));

    return 0;
}

/*!
 * \brief Start the VS-FTP server.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
int VSFTPServerStart(void)
{
    int retval = -1;

    /* Initialize structure data to invalid values. */
    serverData.transferSock = -1;
    serverData.serverSock = -1;
    serverData.clientSock = -1;
    serverData.cwdBufSize = sizeof(serverData.cwd);
    retval = VSFTPServerSetCwd(serverData.vsftpConfigData.rootPath, serverData.vsftpConfigData.rootPathLen);

    if (retval == 0) {
        serverData.transferModeBinary = false;

        /* Prepare sockaddr_in structure. */
        serverData.server.sin_family = AF_INET;
        serverData.server.sin_addr.s_addr = INADDR_ANY;
        serverData.server.sin_port = htons(serverData.vsftpConfigData.port);

        /* Create the socket. */
        retval = VSFTPServerCreatePassiveSocket((in_port_t)serverData.vsftpConfigData.port,
                                                &serverData.serverSock,
                                                &serverData.server);
    }

    if (retval == 0) {
        serverData.isServerSocketCreated = true;
    }

    return retval;
}

/*!
 * \brief Stop the VS-FTP server.
 * \details
 *      Stop serving and clean-up to restore the initialized state.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
int VSFTPServerStop(void)
{
    /* We don't know in what state we currently are, just orderly shutdown and close everything. */
    (void)shutdown(serverData.transferSock, SHUT_RDWR);
    (void)close(serverData.transferSock);
    serverData.transferSock = -1;

    (void)shutdown(serverData.clientSock, SHUT_RDWR);
    (void)close(serverData.clientSock);
    serverData.clientSock = -1;

    (void)shutdown(serverData.serverSock, SHUT_RDWR);
    (void)close(serverData.serverSock);
    serverData.serverSock = -1;

    serverData.isServerSocketCreated = false;
    serverData.isConnected = false;

    return 0;
}

static inline void StripCRAndNewline(char *buf, size_t *len)
{
    for (unsigned long i = 0; i < *len; i++) {
        if ((buf[i] == '\r') ||
            (buf[i] == '\n')) {
            buf[i] = '\0';
            *len = i;
            break;
        }
    }
}

static int WaitForIncomingConnection(void)
{
    int c = 0;
    int retval = 0;

    /* Poll for an incoming connection and accept it when there is 1. */
    c = sizeof(struct sockaddr_in);
    serverData.clientSock = accept(serverData.serverSock,
                                   (struct sockaddr *)&serverData.client,
                                   (socklen_t *)&c);
    if (serverData.clientSock >= 0) {
        puts("Connection accepted");

        /* Set cwd to rootdir. */
        retval = VSFTPServerSetCwd(serverData.vsftpConfigData.rootPath, serverData.vsftpConfigData.rootPathLen);
        if (retval == 0) {
            retval = VSFTPServerSendReply("220 Service ready for new user.");
            if (retval == 0) {
                serverData.isConnected = true;
            }
        }
    } else {
        if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
            /* No incoming connection. */
        } else {
            printf("Socket listen failed with errno %d\n", errno);
            (void)VSFTPServerStop();
        }
    }

    return retval;
}

static int HandleConnection(void)
{
    int retval = -1;
    /* Polling read commands from client. */
    size_t bytes_read = 0;
    char buffer[REQUEST_LEN_MAX];

    retval = VSFTPServerReceive(serverData.clientSock, buffer, sizeof(buffer), &bytes_read);

    /* Terminate the buffer. */
    buffer[bytes_read] = '\0';
    if ((retval == 0) && (bytes_read > 0)) {
        StripCRAndNewline(buffer, &bytes_read);
        printf("Received command from client: %s\n", buffer);

        /* Handle the command, we currently ignore errors. */
        retval = VSFTPCommandsParse(buffer, bytes_read);
        if (retval != 0) {
            printf("Command failed with code %d\n\n", retval);
            retval = 0; /* We do not break on a command parse failure, the printout is enough. */
        }
    } else if ((retval == 0) && (bytes_read == 0)) {
        /* Clean-up connection on disconnect, including server socket. */
        puts("Client disconnected");
        (void)VSFTPServerStop();
    } else {
        if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
            /* No incoming data. */
        } else {
            puts("Socket read failed");
            (void)VSFTPServerStop();
        }
    }

    return retval;
}

/*!
 * \brief Handle the next server iteration.
 * \details
 *      When not connected
 *      - Check for a connection and connect when a request is received
 *
 *      When connected:
 *      - Check for a (newly) received command.
 *
 *      And optionally:
 *
 *      - Handle a received command.
 *      - Continue a transmission part.
 *
 *      This function shall return swiftly as to allow the user to do tasks in between.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
int VSFTPServerHandler(void)
{
    int retval = -1;

    if (serverData.isServerSocketCreated == false) {
        /* Create server socket. */
        retval = VSFTPServerStart();
    } else { /* serverData.isServerSocketCreated == true. */
        /* Wait for connection and/or handle connection. */
        if (serverData.isConnected == false) {
            /* Poll for an incoming connection and accept it when there is 1. */
            retval = WaitForIncomingConnection();
        } else { /* serverData.isConnected == true */
            retval = HandleConnection();
        }
    }

    return retval;
}

int VSFTPServerClientDisconnect(void)
{
    return VSFTPServerStop();
}

int VSFTPServerCreateTransferSocket(const uint16_t portNum, int *sock)
{
    int retval = -1;

    if (serverData.transferSock == -1) {
        retval = 0;
    } /* Else already created. */

    if (retval == 0) {
        /* Prepare sockaddr_in structure. */
        serverData.transfer.sin_family = AF_INET;
        serverData.transfer.sin_addr.s_addr = INADDR_ANY;
        serverData.transfer.sin_port = htons(portNum);
        /* Create a new socket connection. */
        retval = VSFTPServerCreatePassiveSocket(portNum, &serverData.transferSock, &serverData.transfer);
    }

    if (retval == 0) {
        *sock = serverData.transferSock;
    }

    return retval;
}

int VSFTPServerCloseTransferSocket(const int sock)
{
    int retval = -1;

    if (sock >= 0) {
        retval = 0;
    } /* Else already closed. */

    if (retval == 0) {
        if (sock == serverData.transferSock) {
            (void)shutdown(serverData.transferSock, SHUT_RDWR);
            (void)close(serverData.transferSock);
            serverData.transferSock = -1;
        }
    }

    return retval;
}

static int VSFTPServerCreatePassiveSocket(uint16_t portNum, int *sock, const struct sockaddr_in *sockData)
{
    int retval = -1;
    int option = 1;

    /* Create the socket. */
    *sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*sock == -1) {
        printf("Could not create socket on port %d\n", portNum);
    } else {
        retval = 0;
    }

    if (retval == 0) {
        /* Change the socket to be non-blocking. */
        retval = fcntl(*sock, F_SETFL, fcntl(*sock, F_GETFL, 0) | O_NONBLOCK);
        if (retval != 0) {
            puts("Socket fcntl failed");
        }
    }

    if (retval == 0) {
        /* Change the socket options. */
        retval = setsockopt(*sock, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR),
                            (char *)&option, sizeof(option));
        if (retval != 0) {
            puts("Socket setsockopt failed");
        }
    }

    if (retval == 0) {
        /* Bind to the socket. */
        retval = bind(*sock, (struct sockaddr *)sockData,
                      sizeof(*sockData));
        if (retval != 0) {
            puts("Socket binding failed");
        }
    }

    if (retval == 0) {
        /* Mark the socket as a passive socket. */
        retval = listen(*sock, 1);
        if (retval != 0) {
            puts("Socket listen failed");
        }
    }

    if (retval == 0) {
        printf("Socket %d on port %d ready for connection...\n", *sock, portNum);
    } else {
        /* Just orderly shutdown and close the socket. */
        (void)shutdown(*sock, SHUT_RDWR);
        (void)close(*sock);
    }

    return retval;
}

int VSFTPServerGetTransferSocket(int *sock)
{
    int retval = -1;

    /* Check if the socket is valid. */
    if (serverData.transferSock >= 0) {
        *sock = serverData.transferSock;
        retval = 0;
    }

    return retval;
}

int VSFTPServerAcceptTransferConnection(const int sock, int *con_sock)
{
    socklen_t addrlen = 0;
    struct sockaddr_in client_address;
    int lsock = -1;
    int retval = -1;

    addrlen = sizeof(client_address);
    lsock = accept(sock, (struct sockaddr *)&client_address, &addrlen);
    if (lsock >= 0) {
        *con_sock = lsock;
        retval = 0;
    }

    return retval;
}

int VSFTPServerGetCwd(char *buf, const size_t size, size_t *len)
{
    int retval = -1;
    int written = 0;

    /* Check if CWD has been initialized and if the buffer is large enough to contain it. */
    if ((serverData.cwdLen > 0) && (serverData.cwdLen < size)) {
        retval = 0;
    }
    if (retval == 0) {
        written = snprintf(buf, size, serverData.cwd, serverData.cwdLen);
        if ((written >= 0) && (written < size)) {
            *len = (size_t)written;
        } else {
            retval = -1;
        }
    }

    return retval;
}

int VSFTPServerGetDirAbsPath(const char *dir, const size_t len, char *absPath, const size_t size,
                             size_t *absPathLen)
{
    int retval = -1;

    if ((dir != NULL) && (len > 0) && (absPath != NULL) && (size > 0)) {
        retval = 0;
    }

    if (retval == 0) {
        retval = VSFTPFilesystemGetAbsPath(dir, len, absPath, size, absPathLen);
    }

    if (retval == 0) {
        retval = VSFTPFilesystemIsDir(absPath, *absPathLen);
    }

    /* Make sure the new path is not above the root dir. */
    if (retval == 0) {
        if (*absPathLen < serverData.vsftpConfigData.rootPathLen) {
            retval = -1;
        }
    }

    if (retval == 0) {
        for (unsigned long i = 0; i < serverData.vsftpConfigData.rootPathLen; i++) {
            if (serverData.vsftpConfigData.rootPath[i] != absPath[i]) {
                retval = -1;
                break;
            }
        }
    }

    return retval;
}

int VSFTPServerSetCwd(const char *dir, const size_t len)
{
    int retval = -1;
    char cwd[PATH_LEN_MAX]; /* Local copy first. */
    size_t cwdLen = 0;

    /* Checks are performed in callee. */

    retval = VSFTPServerGetDirAbsPath(dir, len, cwd, sizeof(cwd), &cwdLen);
    if (retval == 0) {
        if (serverData.cwdBufSize > cwdLen) {
            (void)strncpy(serverData.cwd, cwd, serverData.cwdBufSize);
            serverData.cwdLen = cwdLen;
        } else {
            retval = -1;
        }
    }

    return retval;
}

int VSFTPServerGetFileAbsPath(const char *file, const size_t len, char *absFilePath, const size_t size,
                              size_t *absFilePathLen)
{
    int retval = -1;

    /* Checks are performed in callee. */

    retval = VSFTPFilesystemGetAbsPath(file, len, absFilePath, size, absFilePathLen);
    if (retval == 0) {
        retval = VSFTPFilesystemIsFile(absFilePath, *absFilePathLen);
    }

    return retval;
}

int VSFTPServerSendfile(const int sock, const char *pathTofile, const size_t len)
{
    char fileBuf[FILE_READ_BUF_SIZE];
    size_t toRead = 0;
    ssize_t numRead = 0;
    size_t numSent = 0;
    size_t count = 0;
    int fd = -1;
    int retval = -1;

    retval = VSFTPFilesystemOpenFile(pathTofile, len, &fd, &count);
    if (retval == 0) {
        while (count > 0) {
            toRead = count < sizeof(fileBuf) ? count : sizeof(fileBuf);
            numRead = read(fd, fileBuf, toRead);
            if (numRead == -1) {
                retval = -1;
                break;
            }
            if (numRead == 0) {
                break;                      /* EOF */
            }

            retval = VSFTPServerSendBinaryOwnSock(sock, fileBuf, (size_t)numRead, &numSent);
            if (retval == -1) {
                break;
            }
            if (numSent == 0) {               /* Should never happen */
                retval = -1;
                break;
            }

            count -= numSent;
        }
    }

    (void)VSFTPFilesystemCloseFile(fd);

    return retval;
}

int VSFTPServerSetTransferMode(const bool binary)
{
    serverData.transferModeBinary = binary;

    return 0;
}

int VSFTPServerGetTransferMode(bool *binary)
{
    int retval = -1;

    if (binary != NULL) {
        *binary = serverData.transferModeBinary;
        retval = 0;
    }

    return retval;
}

int VSFTPServerListDirPerFile(const char *dir, size_t len, char *buf, size_t size, size_t *bufLen,
                              bool prependDir, void **cookie)
{
    return VSFTPFilesystemListDirPerLine(dir, len, buf, size, bufLen, prependDir, cookie);
}

int VSFTPServerGetServerIP4(char *buf, const size_t size)
{
    int retval = -1;

    if (size >= INET_ADDRSTRLEN) {
        (void)strncpy(buf, serverData.vsftpConfigData.ipAddr, size);
        retval = 0;
    }

    return retval;
}

int VSFTPServerSendReply(const char *__restrict __format, ...)
{
    char buf[RESPONSE_LEN_MAX];
    int written = 0;
    int retval = -1;
    va_list ap;

    /* Write reply to buffer. */
    va_start(ap, __format);
    written = vsnprintf(buf, sizeof(buf), __format, ap);
    va_end(ap);
    if ((written >= 0) && (written < sizeof(buf))) {
        retval = 0;
    }

    /* Append \r\n. */
    if (retval == 0) {
        written += snprintf(&buf[written], sizeof(buf) - written, "\r\n");
        if ((written < 0) || (written >= sizeof(buf))) {
            retval = -1;
        }
    }

    if (retval == 0) {
        if (write(serverData.clientSock, buf, (size_t)written) == -1) {
            retval = -1;
        }
    }

    return retval;
}

int VSFTPServerSendReplyOwnBuf(char *buf, const size_t size, const size_t len)
{
    int written = 0;
    int retval = -1;

    /* Append \r\n. */
    written = snprintf(&buf[len], size - len, "\r\n");
    if ((written >= 0) && (written < size)) {
        retval = 0;
    }

    if (retval == 0) {
        if (write(serverData.clientSock, buf, len + written) == -1) {
            retval = -1;
        }
    }

    return retval;
}

int VSFTPServerSendReplyOwnBufOwnSock(const int sock, char *buf, const size_t size, const size_t len)
{
    int written = 0;
    int retval = -1;

    /* Append \r\n. */
    written = snprintf(&buf[len], size - len, "\r\n");
    if ((written >= 0) && (written < size)) {
        retval = 0;
    }

    if (retval == 0) {
        if (write(sock, buf, len + written) == -1) {
            retval = -1;
        }
    }

    return retval;
}

int VSFTPServerSendBinaryOwnSock(const int sock, const char *buf, const size_t size, size_t *send)
{
    ssize_t lsend = 0;
    int retval = -1;

    lsend = write(sock, buf, size);
    if (lsend != -1) {
        *send = lsend;
        retval = 0;
    }

    return retval;
}

int VSFTPServerReceive(const int sock, char *buf, const size_t size, size_t *received)
{
    ssize_t bytesRead = 0;
    int retval = -1;

    bytesRead = read(sock, buf, size);
    if (bytesRead != -1) {
        *received = (size_t)bytesRead;
        retval = 0;
    }

    return retval;
}
