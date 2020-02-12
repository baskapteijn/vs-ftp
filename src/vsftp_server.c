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
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "vsftp_server.h"
#include "vsftp_commands.h"
#include "vsftp_filesystem.h"
#include "config.h"

typedef enum {
    VSFTP_STATE_UNINITIALIZED = 0,
    VSFTP_STATE_INITIALIZED = 1,
    VSFTP_STATE_STARTED = 2,
} VSFTPStates_e;

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

    char cwd[100];
    size_t cwdBufSize; //buffer size, not the length of the cwd string!

    bool isConnected;
} vsftpServerData_s;

static vsftpServerData_s serverData;

static int VSFTPServerCreatePassiveSocket(uint16_t portNum, int *sock, const struct sockaddr_in *sockData);

/*!
 * \brief Maintains the server state.
 * \details
 *      The server state flow is as follows:
 *
 *      VSFTPServerInitialize() called
 *        VSFTP_STATE_UNINITIALIZED -> VSFTP_STATE_INITIALIZED
 *      VSFTPServerStart() called
 *        VSFTP_STATE_INITIALIZED -> VSFTP_STATE_STARTED
 *      VSFTPServerStop() called
 *        VSFTP_STATE_STARTED -> VSFTP_STATE_INITIALIZED
 *
 *      This means that initialization can only be performed once, and after initialization the
 *      server can be started and stopped as many times as required.
 */
static VSFTPStates_e state = VSFTP_STATE_UNINITIALIZED;

static inline void StripCRAndNewline(char *buf, ssize_t *len);

/*!
 * \brief Initialize the VS-FTP server.
 * \pre Can only be called when the server state is VSFTP_STATE_UNINITIALIZED.
 * \details
 *      Initializes the Server with configuration data.
 * \param vsftpConfigData
 *      A pointer to the VS-FTP configuration data.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
int VSFTPServerInitialize(const VSFTPConfigData_s *vsftpConfigData)
{
    int retval = -1;

    /* Check the server state. */
    if (state == VSFTP_STATE_UNINITIALIZED) {
        retval = 0;
    }

    //TODO: we might want to check the server configuration data for validity here

    if (retval == 0) {
        /* Copy server configuration data. */
        (void)memcpy(&serverData.vsftpConfigData, vsftpConfigData,
                     sizeof(serverData.vsftpConfigData));

        /* Set the new server state. */
        state = VSFTP_STATE_INITIALIZED;
    }

    return retval;
}

/*!
 * \brief Start the VS-FTP server.
 * \pre Can only be called when the server state is VSFTP_STATE_INITIALIZED.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
int VSFTPServerStart(void)
{
    int retval = -1;

    /* Check the server state. */
    if (state == VSFTP_STATE_INITIALIZED) {
        retval = 0;
    }

    if (retval == 0) {
        /* Initialize structure data to invalid values. */
        serverData.transferSock = -1;
        serverData.serverSock = -1;
        serverData.clientSock = -1;
        if (VSFTPServerGetDirAbsPath(serverData.vsftpConfigData.rootPath, serverData.vsftpConfigData.rootPathLen,
        serverData.cwd, sizeof(serverData.cwd)) != 0) {
            return -1;
        }
        serverData.cwdBufSize = sizeof(serverData.cwd);

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
        /* Set the new server state. */
        state = VSFTP_STATE_STARTED;
    }

    return retval;
}

/*!
 * \brief Stop the VS-FTP server.
 * \pre Can only be called when the server state is VSFTP_STATE_STARTED.
 * \details
 *      Stop serving and clean-up to restore the initialized state.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
int VSFTPServerStop(void)
{
    int retval = -1;

    if (state == VSFTP_STATE_STARTED) {
        retval = 0;
    }

    if (retval == 0) {
        /* We don't know in what state we currently are, just orderly shutdown and close everything. */
        (void)shutdown(serverData.transferSock, SHUT_RDWR);
        (void)close(serverData.transferSock);

        (void)shutdown(serverData.clientSock, SHUT_RDWR);
        (void)close(serverData.clientSock);

        (void)shutdown(serverData.serverSock, SHUT_RDWR);
        (void)close(serverData.serverSock);
    }

    return retval;
}

static inline void StripCRAndNewline(char *buf, ssize_t *len)
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

/*!
 * \brief Handle the next server iteration.
 * \pre Can only be called when the server state is VSFTP_STATE_STARTED.
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
    int c;

    if (state == VSFTP_STATE_STARTED) {
        retval = 0;
    }

    if (retval == 0) {
        if (serverData.isConnected == false) {
            /* Poll for an incoming connection and accept it when there is 1. */
            c = sizeof(struct sockaddr_in);
            serverData.clientSock = accept(serverData.serverSock,
                                            (struct sockaddr *)&serverData.client,
                                            (socklen_t *)&c);
            if (serverData.clientSock >= 0) {
                puts("Connection accepted");

                /* Set cwd to rootdir. */
                if (VSFTPServerGetDirAbsPath(serverData.vsftpConfigData.rootPath, serverData.vsftpConfigData.rootPathLen,
                                             serverData.cwd, sizeof(serverData.cwd)) != 0) {
                    return -1;
                }

                (void)write(serverData.clientSock, "220 Service ready for new user.\n", 32); //TODO: make this size stuff better

                serverData.isConnected = true;
            } else {
                if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
                    /* No incoming connection. */
                } else {
                    puts("Socket listen failed");
                    retval = -1;
                }
            }
        } else { /* serverData.isConnected == true */
            /* Polling read commands from client. */
            ssize_t bytes_read = 0;
            char buffer[256]; //TODO: make this nice

            bytes_read = read(serverData.clientSock, buffer, sizeof(buffer));
            //TODO: handle -1 bytes_read
            /* Terminate the buffer. */
            buffer[bytes_read] = '\0';
            if (bytes_read > 0) {
                StripCRAndNewline(buffer, &bytes_read);
                printf("Received command from client: %s\n", buffer);

                //TODO: handle returned error command parser (disconnect client?)

                /* Handle the command, we currently ignore errors. */
                (void)VSFTPCommandsParse(buffer, bytes_read);
            } else if (bytes_read == 0) {
                /* Client disconnected. */
                puts("Client disconnected");
                serverData.isConnected = false;

                /* close sockets except server socket */
                (void)shutdown(serverData.transferSock, SHUT_RDWR);
                (void)close(serverData.transferSock);

                (void)shutdown(serverData.clientSock, SHUT_RDWR);
                (void)close(serverData.clientSock);

                serverData.transferSock = -1;
                serverData.clientSock = -1;

            } else {
                if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
                    /* No incoming data. */
                } else {
                    puts("Socket read failed");
                    retval = -1;
                    serverData.isConnected = false;
                }
            }

            // TODO: can we handle commands during data transfer, other than ABORT?
            // TODO: continue data transfer here if there is 1 going
        }
    }

    return retval;
}

int VSFTPServerClientDisconnect(void)
{
    /* Client disconnected. */
    puts("Client disconnected");
    serverData.isConnected = false;

    /* close sockets except server socket */
    (void)shutdown(serverData.transferSock, SHUT_RDWR);
    (void)close(serverData.transferSock);

    (void)shutdown(serverData.clientSock, SHUT_RDWR);
    (void)close(serverData.clientSock);

    serverData.transferSock = -1;
    serverData.clientSock = -1;

    return 0;
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

int VSFTPServerGetCwd(char *buf, const size_t size)
{
    int retval = -1;

    if (strlen(serverData.cwd) < size) {
        retval = 0;
    }
    (void)snprintf(buf, size, serverData.cwd, strlen(serverData.cwd));

    return retval;
}

int VSFTPServerGetDirAbsPath(const char *dir, const size_t len, char *absPath, const size_t size)
{
    int retval = -1;
    char ldir[PATH_LEN_MAX];

    if ((dir != NULL) && (len > 0) && (absPath != NULL) && (size > 0)) {
        retval = 0;
    }

    if (retval == 0) {
        /* check if it is an absolute dir */
        if ((dir[0] != '.') && (VSFTPFilesystemIsDir(dir) == 0)) {
            retval = VSFTPFilesystemGetAbsPath(dir, len, ldir, sizeof(ldir));
        } else {
            retval = VSFTPFilesystemIsRelativeDir(serverData.cwd, serverData.cwdBufSize, dir, len, ldir, sizeof(ldir));
        }
    }

    // make sure the new path is not above the root dir
    if (retval == 0) {
        if (strlen(ldir) < serverData.vsftpConfigData.rootPathLen) {
            retval = -1;
        }
    }

    if (retval == 0) {
        for (unsigned long i = 0; i < serverData.vsftpConfigData.rootPathLen; i++) {
            if (serverData.vsftpConfigData.rootPath[i] != ldir[i]) {
                retval = -1;
                break;
            }
        }
    }

    if (retval == 0) {
        if (size > strlen(ldir)) {
            strncpy(absPath, ldir, size);
        } else {
            retval = -1;
        }
    }

    return retval;
}

int VSFTPServerSetCwd(const char *dir, const size_t len)
{
    /* Checks are performed in callee. */
    return VSFTPServerGetDirAbsPath(dir, len, serverData.cwd, serverData.cwdBufSize);
}

int VSFTPServerGetFileAbsPath(const char *file, const size_t len, char *absFilePath, const size_t size)
{
    /* Checks are performed in callee. */
    return VSFTPFilesystemIsRelativeFile(serverData.cwd, serverData.cwdBufSize, file, len, absFilePath, size);
}

int VSFTPServerSendfile(const int sock, const char *pathTofile, const size_t len)
{
    char fileBuf[FILE_READ_BUF_SIZE];
    size_t toRead = 0;
    ssize_t numRead = 0;
    ssize_t numSent = 0;
    size_t totSent = 0;
    size_t count = 0;
    int fd = -1;
    struct stat stat_buf;
    int retval = -1;

    //TODO: move file interactions to filesystem component

    if (access(pathTofile, R_OK) == 0) {
        retval = 0;
    }

    if (retval == 0) {
        fd = open(pathTofile, O_RDONLY);
        if (fd == -1) {
            retval = -1;
        } else {
            fstat(fd, &stat_buf);
            count = (size_t)stat_buf.st_size;
        }
    }

    if (retval == 0) {
        totSent = 0;
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

            numSent = write(sock, fileBuf, (size_t)numRead);
            if (numSent == -1) {
                retval = -1;
                break;
            }
            if (numSent == 0) {               /* Should never happen */
                retval = -1;
                break;
            }

            count -= numSent;
            totSent += numSent;
        }
    }

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

int VSFTPServerListDirPerFile(const char *dir, size_t len, char *buf, size_t size, bool prependDir, DIR **d)
{
    return VSFTPFilesystemListDirPerFile(dir, len, buf, size, prependDir, d);
}
#if 0
int VSFTPServerGetIP4FromSocket(const int sock, char *buf, const size_t size)
{
    int retval = -1;
    struct sockaddr_in* pV4Addr = (struct sockaddr_in*)&sock;
    struct in_addr ipAddr = pV4Addr->sin_addr;

    if ((buf != NULL) && (size >= INET_ADDRSTRLEN)) {
        if (inet_ntop(AF_INET, &ipAddr, buf, INET_ADDRSTRLEN) != NULL) {
            retval = 0;
        }
    }

    return retval;
}
#endif

int VSFTPServerGetServerIP4(char *buf, const size_t size)
{
    (void)strncpy(buf, serverData.vsftpConfigData.ipAddr, size);
    return 0;
}

int VSFTPServerSend(const char *string)
{
    char buf[RESPONSE_LEN_MAX];
    size_t written = 0;

    written = (size_t)snprintf(buf, sizeof(buf), "%s\r\n", string);

    return write(serverData.clientSock, buf, written) >= 0 ? 0 : -1;
}
