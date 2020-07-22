/*
 * This file is part of the vs-ftp distribution (https://github.com/baskapteijn/vs-ftp).
 * Copyright (c) 2020 Bas Kapteijn.
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include "vsftp_server.h"
#include "vsftp_commands.h"
#include "vsftp_filesystem.h"
#include "config.h"
#include "io.h"

typedef struct {
    /* Configuration data. */
    uint16_t port;
    char rootPath[PATH_LEN_MAX];
    char ipAddr[INET_ADDRSTRLEN];
    size_t ipAddrLen;
    size_t rootPathLen;

    /* Internal data. */
    char cwd[PATH_LEN_MAX];
    size_t cwdLen;
    int serverSock;
    int clientSock;
    int transferSock;
    struct sockaddr_in server;
    struct sockaddr_in client;
    struct sockaddr_in transfer;
    bool transferModeBinary;

    bool isConnected;
    bool isServerSocketCreated;
} vsftpServerData_s;

static vsftpServerData_s serverData;

static int CreatePassiveSocket(uint16_t portNum, int *sock, const struct sockaddr_in *sockData);
static int WaitForIncomingConnection(void);
static int HandleConnection(void);
static int SendOwnSock(int sock, const char *buf, size_t size, size_t *send);
static int ReceiveOwnSock(int sock, char *buf, size_t size, size_t *received);

/*!
 * \brief Create a passive socket.
 * \details
 *      Passive, blocking, re-using port.
 * \param portNum
 *      The port number to create a socket with.
 * \param[out] sock
 *      A pointer to the storage location for the created socket.
 * \param sockData
 *      A pointer to information for binding the socket.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
static int CreatePassiveSocket(uint16_t portNum, int *sock, const struct sockaddr_in *sockData)
{
    int retval = -1;
    int option = 1;

    /* Argument checks are performed by the caller. */

    /* Create the socket. */
    *sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*sock == -1) {
        FTPLOG("Could not create socket on port %d\n", portNum);
    } else {
        retval = 0;
    }

    if (retval == 0) {
        /* The socket is blocking. */
        retval = fcntl(*sock, F_SETFL, fcntl(*sock, F_GETFL, 0));
        if (retval != 0) {
            FTPLOG("Socket fcntl failed with error %d\n", retval);
        }
    }

    if (retval == 0) {
        /* Change the socket options. */
        retval = setsockopt(*sock, SOL_SOCKET, SO_REUSEPORT, (char *)&option, sizeof(option));
        if (retval != 0) {
            FTPLOG("Socket setsockopt failed with error %d\n", retval);
        }
    }

    if (retval == 0) {
        /* Bind to the socket. */
        retval = bind(*sock, (struct sockaddr *)sockData, sizeof(*sockData));
        if (retval != 0) {
            FTPLOG("Socket binding failed with error %d\n", retval);
        }
    }

    if (retval == 0) {
        /* Mark the socket as a passive socket. */
        retval = listen(*sock, 1);
        if (retval != 0) {
            FTPLOG("Socket listen failed with error %d\n", retval);
        }
    }

    if (retval == 0) {
        FTPLOG("Socket %d on port %d ready for connection...\n", *sock, portNum);
    } else {
        /* Just orderly shutdown and close the socket. */
        (void)shutdown(*sock, SHUT_RDWR);
        (void)close(*sock);
    }

    return retval;
}

/*!
 * \brief Wait for a client connection.
 * \details
 *      This is a synchronous call.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
static int WaitForIncomingConnection(void)
{
    int c = 0;
    int retval = 0;

    /* Argument checks are performed by the caller. */

    /* Poll for an incoming connection and accept it when there is 1. */
    c = sizeof(struct sockaddr_in);
    serverData.clientSock = accept(serverData.serverSock,
                                   (struct sockaddr *)&serverData.client,
                                   (socklen_t *)&c);
    if (serverData.clientSock >= 0) {
        FTPLOG("Client socket %d connection accepted\n", serverData.clientSock);

        /* Set cwd to rootdir. */
        retval = VSFTPServerSetCwd(serverData.rootPath, serverData.rootPathLen);
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
            FTPLOG("Socket listen failed with error %d\n", errno);
            (void)VSFTPServerStop();
        }
    }

    return retval;
}

/*!
 * \brief Handle commands on an active client connection.
 * \details
 *      This is a synchronous call.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
static int HandleConnection(void)
{
    int retval = -1;
    /* Polling read commands from client. */
    size_t bytes_read = 0;
    char buffer[REQUEST_LEN_MAX];

    /* Argument checks are performed by the caller. */

    retval = ReceiveOwnSock(serverData.clientSock, buffer, sizeof(buffer), &bytes_read);

    /* Terminate the buffer. */
    buffer[bytes_read] = '\0';
    if ((retval == 0) && (bytes_read > 2) && (strncmp(&buffer[bytes_read - 2], "\r\n", 2) == 0)) {
        FTPLOG("Received command from client: %s\n", buffer);

        /* Handle the command, we currently ignore errors. */
        buffer[bytes_read - 2] = '\0';
        bytes_read -= 2;
        retval = VSFTPCommandsParse(buffer, bytes_read);
        if (retval != 0) {
            FTPLOG("Command failed with error %d\n", retval);
            /* In case we get a list command (which creates a transfer socket, but is then rejected),
             * or for any other reason, make sure to close a created but not used transfer socket.
             */
            (void)VSFTPServerCloseTransferSocket(serverData.transferSock);
            /* We do not break on a command parse failure, the printout is enough. */
            retval = 0;
        } else {
            FTPLOG("Command handled successfully\n");
        }
    } else if ((retval == 0) && (bytes_read == 0)) {
        /* Clean-up connection on disconnect, including server socket. */
        FTPLOG("Client connection lost\n");

        VSFTPServerClientDisconnect();
    } else {
        if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
            /* No incoming data. */
        } else {
            FTPLOG("Socket read failed with error %d\n", errno);
            (void)VSFTPServerStop();
        }
    }

    return retval;
}

/*!
 * \brief Send data on a given socket.
 * \param sock
 *      The socket to send the file to.
 * \param buf
 *      A pointer to the storage location containing data.
 * \param size
 *      The size of 'buf'.
 * \param[out] send
 *      A pointer to the storage location for the total send data.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
static int SendOwnSock(const int sock, const char *buf, const size_t size, size_t *send)
{
    ssize_t lsend = 0;
    int retval = -1;

    /* Argument checks are performed by the caller. */

    lsend = write(sock, buf, size);
    if (lsend != -1) {
        *send = (size_t)lsend;
        retval = 0;
    }

    return retval;
}

/*!
 * \brief Receive data on a given socket.
 * \param sock
 *      The socket to send the file to.
 * \param buf
 *      A pointer to the storage location for data.
 * \param size
 *      The size of 'buf'.
 * \param[out] received
 *      A pointer to the storage location for the total received data.
 * \return 
 */
static int ReceiveOwnSock(const int sock, char *buf, const size_t size, size_t *received)
{
    ssize_t bytesRead = 0;
    int retval = -1;

    /* Argument checks are performed by the caller. */

    bytesRead = read(sock, buf, size);
    if (bytesRead != -1) {
        *received = (size_t)bytesRead;
        retval = 0;
    }

    return retval;
}

/*!
 * \brief Initialize the VS-FTP server.
 * \details
 *      Initializes the Server with configuration data received from the caller.
 * \param rootPath
 *      The root path to start the FTP server with.
 *      The server will not allow any path to be accessed above/before it.
 * \param rootPathLen
 *      The length of 'rootPath'.
 * \param ipAddr
 *      The IP-Address to start a client listen socket on.
 * \param ipAddrLen
 *      The length of 'ipAddr'.
 * \param port
 *      The port to start a client listen socket on.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
int VSFTPServerInitialize(const char *rootPath, const size_t rootPathLen, const char *ipAddr, const size_t ipAddrLen,
                          const uint16_t port)
{
    int retval = -1;

    FTPLOG("Initializing server\n");

    if ((rootPath != NULL) && (rootPathLen > 0) && (ipAddr != NULL) && (ipAddrLen > 0)) {
        retval = 0;
    }

    if (retval == 0) {
        /* Copy server configuration data. */
        (void)strncpy(serverData.ipAddr, ipAddr, sizeof(serverData.ipAddr));
        serverData.ipAddrLen = ipAddrLen;
        serverData.port = port;

        retval = VSFTPFilesystemGetRealPath(NULL, 0, rootPath, rootPathLen, serverData.rootPath,
                                            sizeof(serverData.rootPath),
                                            &serverData.rootPathLen);
    }

    if (retval == 0) {
        retval = VSFTPFilesystemIsDir(serverData.rootPath, serverData.rootPathLen);
    }

    return retval;
}

/*!
 * \brief Start the VS-FTP server.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
int VSFTPServerStart(void)
{
    int retval = -1;

    FTPLOG("Starting server\n");

    /* Initialize structure data to invalid values. */
    serverData.transferSock = -1;
    serverData.serverSock = -1;
    serverData.clientSock = -1;
    retval = VSFTPServerSetCwd(serverData.rootPath, serverData.rootPathLen);

    if (retval == 0) {
        serverData.transferModeBinary = true;

        /* Prepare sockaddr_in structure. */
        serverData.server.sin_family = AF_INET;
        serverData.server.sin_addr.s_addr = INADDR_ANY;
        serverData.server.sin_port = htons(serverData.port);

        /* Create the socket. */
        retval = CreatePassiveSocket((in_port_t)serverData.port,
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
    FTPLOG("Stopping server\n");

    /* We don't know in what state we currently are, just orderly shutdown and close everything. */
    if (serverData.transferSock != -1) {
        FTPLOG("Closing transfer socket %d\n", serverData.transferSock);
        (void)shutdown(serverData.transferSock, SHUT_RDWR);
        (void)close(serverData.transferSock);
        serverData.transferSock = -1;
    }

    if (serverData.clientSock != -1) {
        FTPLOG("Closing client socket %d\n", serverData.clientSock);
        (void)shutdown(serverData.clientSock, SHUT_RDWR);
        (void)close(serverData.clientSock);
        serverData.clientSock = -1;
    }

    if (serverData.serverSock != -1) {
        FTPLOG("Closing server socket %d\n", serverData.serverSock);
        (void)shutdown(serverData.serverSock, SHUT_RDWR);
        (void)close(serverData.serverSock);
        serverData.serverSock = -1;
    }

    serverData.isServerSocketCreated = false;
    serverData.isConnected = false;

    return 0;
}

/*!
 * \brief Handle the next server iteration.
 * \details
 *      When not connected
 *      - Check for a connection and connect when a request is received
 *
 *      When connected:
 *      - Check for a (newly) received command.
 *      - Handle a received command.
 *      - Repeat.
 *      - Handle closing a connection.
 *
 *      This function blocks waiting on a client connection or client data.
 *      Each action (connection, data reception, disconnection) this function
 *      will loop back to the caller.
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

/*!
 * \brief Disconnect a client or clean up a partial disconnection.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
int VSFTPServerClientDisconnect(void)
{
    FTPLOG("Disconnecting client\n");

    /* We don't know in what state we currently are, just orderly shutdown and close everything. */
    if (serverData.transferSock != -1) {
        FTPLOG("Closing transfer socket %d\n", serverData.transferSock);
        (void)shutdown(serverData.transferSock, SHUT_RDWR);
        (void)close(serverData.transferSock);
        serverData.transferSock = -1;
    }

    if (serverData.clientSock != -1) {
        FTPLOG("Closing client socket %d\n", serverData.clientSock);
        (void)shutdown(serverData.clientSock, SHUT_RDWR);
        (void)close(serverData.clientSock);
        serverData.clientSock = -1;
    }

    serverData.isConnected = false;

    return 0;
}

/*!
 * \brief Create the transfer socket.
 * \param portNum
 *      The port number to create the transfer socket on.
 * \param[out] sock
 *      Pointer to the storage location of the socket.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
int VSFTPServerCreateTransferSocket(const uint16_t portNum, int *sock)
{
    int retval = -1;

    if ((sock != NULL) && (serverData.transferSock == -1)) {
        retval = 0;
    } /* Else already created. */

    if (retval == 0) {
        /* Prepare sockaddr_in structure. */
        serverData.transfer.sin_family = AF_INET;
        serverData.transfer.sin_addr.s_addr = INADDR_ANY;
        serverData.transfer.sin_port = htons(portNum);
        /* Create a new socket connection. */
        retval = CreatePassiveSocket(portNum, &serverData.transferSock, &serverData.transfer);
    }

    if (retval == 0) {
        *sock = serverData.transferSock;
    }

    return retval;
}

/*!
 * \brief Close the transfer socket.
 * \param sock
 *      The socket to close.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
int VSFTPServerCloseTransferSocket(const int sock)
{
    int retval = -1;

    if (sock == serverData.transferSock) {
        retval = 0;
    }

    if (retval == 0) {
        FTPLOG("Closing transfer socket %d\n", sock);
        retval = shutdown(serverData.transferSock, SHUT_RDWR);
    }

    if (retval == 0) {
        retval = close(serverData.transferSock);
    }

    serverData.transferSock = -1;

    return retval;
}

/*!
 * \brief Get the transfer socket.
 * \param sock
 *      A pointer to the storage location for the socket.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
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

    if (con_sock != NULL) {
        retval = 0;
    }

    if (retval == 0) {
        addrlen = sizeof(client_address);
        lsock = accept(sock, (struct sockaddr *)&client_address, &addrlen);
        if (lsock >= 0) {
            *con_sock = lsock;
            retval = 0;
        }
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

    /* Arguments checked by callees. */

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

            retval = SendOwnSock(sock, fileBuf, (size_t)numRead, &numSent);
            if (retval == -1) {
                break;
            }
            if (numSent == 0) {
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

int VSFTPServerIsValidIPAddress(char *ipAddress)
{
    int retval = -1;
    struct sockaddr_in sa;

    if (ipAddress != NULL) {
        retval = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
    }

    return retval;
}

int VSFTPServerGetServerIP4(char *buf, const size_t size, size_t *len)
{
    int retval = -1;

    if ((buf != NULL) && (size >= INET_ADDRSTRLEN) && (len != NULL)) {
        retval = 0;
    }

    if (retval == 0) {
        (void)strncpy(buf, serverData.ipAddr, size);
        *len = strnlen(buf, size);
    }

    return retval;
}

int VSFTPServerAbsPathIsNotAboveRootPath(const char *absPath, const size_t absPathLen)
{
    int retval = -1;

    if ((absPath != NULL) && (absPathLen > 0)) {
        retval = 0;
    }

    if (retval == 0) {
        /* Make sure the new path is not above the root path. */
        if (absPathLen < serverData.rootPathLen) {
            retval = -1;
        }
    }

    if (retval == 0) {
        for (unsigned long i = 0; i < serverData.rootPathLen; i++) {
            if (serverData.rootPath[i] != absPath[i]) {
                retval = -1;
                break;
            }
        }
    }

    return retval;
}

int VSFTPServerServerPathToRealPath(const char *serverPath, const size_t serverPathLen, char *realPath,
                                    const size_t size, size_t *realPathLen)
{
    char cwd[PATH_LEN_MAX];
    size_t cwdLen = 0;
    char buf[PATH_LEN_MAX];
    size_t bufLen = 0;
    int retval = -1;

    if ((serverPath != NULL) && (serverPathLen > 0) && (realPath != NULL) && (size > 0) && (realPathLen != NULL)) {
        retval = 0;
    }

    if (retval == 0) {
        if (VSFTPFilesystemIsAbsPath(serverPath) == 0) {
            /* This seems to be an absolute path, prepend it with the root path. */
            (void)strncpy(buf, serverData.rootPath, sizeof(buf));
            (void)strncpy(&buf[serverData.rootPathLen], serverPath, sizeof(buf) - serverData.rootPathLen);
            bufLen = strnlen(buf, sizeof(buf));
            retval = VSFTPFilesystemGetRealPath(NULL, 0, buf, bufLen, realPath, size, realPathLen);
        } else {
            retval = VSFTPServerGetCwd(cwd, sizeof(cwd), &cwdLen);

            if (retval == 0) {
                retval = VSFTPFilesystemGetRealPath(cwd, cwdLen, serverPath, serverPathLen, realPath, size,
                                                    realPathLen);
            }
        }
    }

    return retval;
}

int VSFTPServerRealPathToServerPath(const char *realPath, const size_t realPathLen, char *serverPath,
                                    const size_t size, size_t *serverPathLen)
{
    int retval = -1;

    if ((realPath != NULL) && (realPathLen > 0) && (serverPath != NULL) && (size > 0) && (serverPathLen != NULL)) {
        retval = 0;
    }

    if (retval == 0) {
        serverPath[0] = '/';
        serverPath[1] = '\0';
        *serverPathLen = 1;

        for (int i = 0; i < (int)realPathLen; i++) {
            if (realPath[i] == serverData.rootPath[i]) {
                continue;
            } else {
                (void)strncpy(serverPath, &realPath[i], size);
                *serverPathLen = strnlen(serverPath, size);
                break;
            }
        }
    }

    return retval;
}

int VSFTPServerSetCwd(const char *dir, const size_t len)
{
    int retval = -1;
    char realPath[PATH_LEN_MAX]; /* Local copy first. */
    size_t realPathLen = 0;
    char cwd[PATH_LEN_MAX];
    size_t cwdLen = 0;

    /* Checks are performed in callees. */

    retval = VSFTPServerGetCwd(cwd, sizeof(cwd), &cwdLen);

    /* Get the absolute path. */
    if (retval == 0) {
        retval = VSFTPFilesystemGetRealPath(cwd, cwdLen, dir, len, realPath, sizeof(realPath), &realPathLen);
    } else {
        /* It could be that CWD has not yet been set, just pass it as NULL with length 0. */
        retval = VSFTPFilesystemGetRealPath(NULL, 0, dir, len, realPath, sizeof(realPath), &realPathLen);
    }

    if (retval == 0) {
        retval = VSFTPFilesystemIsDir(realPath, realPathLen);
    }

    /* Make sure the new path is not above the root path. */
    if (retval == 0) {
        retval = VSFTPServerAbsPathIsNotAboveRootPath(realPath, realPathLen);
    }

    /* Set the new path. */
    if (retval == 0) {
        if (sizeof(serverData.cwd) > realPathLen) {
            (void)strncpy(serverData.cwd, realPath, sizeof(serverData.cwd));
            serverData.cwdLen = realPathLen;
        } else {
            retval = -1;
        }
    }

    return retval;
}

int VSFTPServerGetCwd(char *buf, const size_t size, size_t *len)
{
    int retval = -1;
    int written = 0;

    /* Check if CWD has been initialized and if the buffer is large enough to contain it. */
    if ((buf != NULL) && (size > 0) && (len != NULL) &&
        (serverData.cwdLen > 0) && (serverData.cwdLen < size)) {
        retval = 0;
    }

    if (retval == 0) {
        written = snprintf(buf, size, serverData.cwd, serverData.cwdLen);
        if ((written >= 0) && ((size_t)written < size)) {
            *len = (size_t)written;
        } else {
            retval = -1;
        }
    }

    return retval;
}

int VSFTPServerSendReply(const char *__restrict format, ...)
{
    char buf[RESPONSE_LEN_MAX];
    int written = 0;
    int retval = -1;
    va_list ap;

    if (format != NULL) {
        retval = 0;
    }

    if (retval == 0) {
        /* Write reply to buffer. */
        va_start(ap, format);
        written = vsnprintf(buf, sizeof(buf), format, ap);
        va_end(ap);
        if ((written < 0) || ((size_t)written >= sizeof(buf))) {
            retval = -1;
        }
    }

    /* Append \r\n. */
    if (retval == 0) {
        written += snprintf(&buf[written], sizeof(buf) - written, "\r\n");
        if ((written < 0) || ((size_t)written >= sizeof(buf))) {
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

    if ((buf != NULL) && (size > 0) && (len > 0)) {
        retval = 0;
    }

    if (retval == 0) {
        /* Append \r\n. */
        written = snprintf(&buf[len], size - len, "\r\n");
        if ((written >= 0) && ((size_t)written < size)) {
            retval = 0;
        }
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

    /* No need to check 'sock', this is handled by write(). */

    if ((buf != NULL) && (size > 0) && (len > 0)) {
        retval = 0;
    }

    if (retval == 0) {
        /* Append \r\n. */
        written = snprintf(&buf[len], size - len, "\r\n");
        if ((written >= 0) && ((size_t)written < size)) {
            retval = 0;
        }
    }

    if (retval == 0) {
        if (write(sock, buf, len + written) == -1) {
            retval = -1;
        }
    }

    return retval;
}

