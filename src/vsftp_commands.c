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

#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include "vsftp_commands.h"
#include "vsftp_server.h"
#include "config.h"

#define DIM(_a)             (sizeof((_a)) / sizeof(*(_a)))
#define STRLEN(_a)          ((sizeof((_a)) / sizeof(*(_a))) - 1)

#define FTP_COMMAND_USER            "USER"
#define FTP_COMMAND_PASV            "PASV"
#define FTP_COMMAND_NLST            "NLST"
#define FTP_COMMAND_PWD             "PWD"
#define FTP_COMMAND_CWD             "CWD"
#define FTP_COMMAND_RETR            "RETR"
#define FTP_COMMAND_TYPE            "TYPE"
#define FTP_COMMAND_HELP            "HELP"
#define FTP_COMMAND_QUIT            "QUIT"

typedef int (* CommandHandle)(const char *args, size_t len);

typedef struct {
    const char *name;
    size_t nameLen;
    CommandHandle handle;
}Command_s;

static int CommandHandlerUser(const char *args, size_t len);
static int CommandHandlerPasv(const char *args, size_t len);
static int CommandHandlerNlst(const char *args, size_t len);
static int CommandHandlerPwd(const char *args, size_t len);
static int CommandHandlerCwd(const char *args, size_t len);
static int CommandHandlerRetr(const char *args, size_t len);
static int CommandHandlerType(const char *args, size_t len);
static int CommandHandlerHelp(const char *args, size_t len);
static int CommandHandlerQuit(const char *args, size_t len);

static Command_s commands[] = {
        { FTP_COMMAND_USER, STRLEN(FTP_COMMAND_USER), CommandHandlerUser },
        { FTP_COMMAND_PASV, STRLEN(FTP_COMMAND_PASV), CommandHandlerPasv },
        { FTP_COMMAND_NLST, STRLEN(FTP_COMMAND_NLST), CommandHandlerNlst },
        { FTP_COMMAND_PWD, STRLEN(FTP_COMMAND_PWD), CommandHandlerPwd },
        { FTP_COMMAND_CWD, STRLEN(FTP_COMMAND_CWD), CommandHandlerCwd },
        { FTP_COMMAND_RETR, STRLEN(FTP_COMMAND_RETR), CommandHandlerRetr },
        { FTP_COMMAND_TYPE, STRLEN(FTP_COMMAND_TYPE), CommandHandlerType },
        { FTP_COMMAND_HELP, STRLEN(FTP_COMMAND_HELP), CommandHandlerHelp },
        { FTP_COMMAND_QUIT, STRLEN(FTP_COMMAND_QUIT), CommandHandlerQuit }
};

static int CommandHandlerUser(const char *args, size_t len)
{
    int retval = -1;

    /* (args != NULL) guaranteed by caller. */

    if (strncmp("anonymous", args, len) == 0) {
        retval = VSFTPServerSend("230 User logged in, proceed.");
    } else {
        retval = VSFTPServerSend("530 Login incorrect.");
        (void)VSFTPServerClientDisconnect();
    }

    return retval;
}

static int CommandHandlerPasv(const char *args, size_t len)
{
    int retval = -1;
    int pasv_server_sock = -1;
    char buf[RESPONSE_LEN_MAX];
    char ipAddrBuf[INET_ADDRSTRLEN];
    uint8_t p1 = 0;
    uint8_t p2 = 0;
    uint16_t portNumber = PASV_PORT_NUMBER;

    /* Close a socket if it is still open (could be happening with unsupported reception of list command). */
    retval = VSFTPServerGetTransferSocket(&pasv_server_sock);
    if (retval == 0) {
        retval = VSFTPServerCloseTransferSocket(pasv_server_sock);
    } else {
        /* socket was already closed. */
        retval = 0;
    }

    if (retval == 0) {
        /* Create a new pasv_server_sock connection. */
        retval = VSFTPServerCreateTransferSocket(portNumber, &pasv_server_sock);
    }

    /* Transmit socket to client. */
    if (retval == 0) {
        /* Set P1 and P2. */
        p1 = (uint8_t)(portNumber >> 8U);
        p2 = (uint8_t)(portNumber & 0xFFU);

        retval = VSFTPServerGetServerIP4(ipAddrBuf, sizeof(ipAddrBuf));
        if (retval == 0) {
            for (unsigned long i = 0; i < sizeof(ipAddrBuf); i++) {
                if (ipAddrBuf[i] == '.') {
                    ipAddrBuf[i] = ',';
                }
            }
        }
    }

    if (retval == 0) {
        /* (h1,h2,h3,h4,p1,p2) */
        (void)snprintf(buf, sizeof(buf), "227 Entering Passive Mode (%s,%d,%d).",
                       ipAddrBuf, p1, p2);
        retval = VSFTPServerSend(buf);
    } else {
        retval = VSFTPServerSend("425 Cannot open data connection.");
    }

    return retval;
}

static int CommandHandlerNlst(const char *args, size_t len)
{
    int retval = -1;
    int pasv_server_sock = -1;
    int pasv_client_sock = -1;
    char buf[PATH_LEN_MAX];
    char dir[PATH_LEN_MAX];
    size_t dirLen = 0;
    DIR *d = NULL;

    /* (args != NULL) guaranteed by caller, len may be 0. */

    /* Get socket. */
    retval = VSFTPServerGetTransferSocket(&pasv_server_sock);

    if (retval == 0) {
        if (len == 0) {
            /* Get cwd. */
            retval = VSFTPServerGetCwd(dir, sizeof(dir));
        } else {
            /* Get requested dir. */
            retval = VSFTPServerGetDirAbsPath(args, len, dir, sizeof(dir));
        }
    }

    if (retval == 0) {
        retval = VSFTPServerAcceptTransferConnection(pasv_server_sock, &pasv_client_sock);
    }

    if (retval == 0) {
        retval = VSFTPServerSend("150 Here comes the directory listing.");
    }

    if (retval == 0) {
        dirLen = strnlen(dir, sizeof(dir));

        /* List dirs and files of given dir. */
        do {
            retval = VSFTPServerListDirPerFile(dir, dirLen, buf, sizeof(buf), (len != 0), &d);
            if (retval != 0) {
                break;
            }

            (void)write(pasv_client_sock, buf, strlen(buf));
        } while(d != NULL);
    }

    (void)close(pasv_client_sock);
    (void)VSFTPServerCloseTransferSocket(pasv_server_sock);

    if (retval == 0) {
        retval = VSFTPServerSend("226 Directory send OK.");
    } else {
        retval = VSFTPServerSend("451 Requested action aborted: local error in processing.");
    }

    return retval;
}

static int CommandHandlerPwd(const char *args, size_t len)
{
    char buf[RESPONSE_LEN_MAX];
    char cwd[PATH_LEN_MAX];
    int retval = -1;

    retval = VSFTPServerGetCwd(cwd, sizeof(cwd));
    if (retval == 0) {
        (void)snprintf(buf, sizeof(buf), "257 \"%s\"", cwd);
        retval = VSFTPServerSend(buf);
    } else {
        retval = VSFTPServerSend("550 Failed to get directory.");
    }

    return retval;
}

static int CommandHandlerCwd(const char *args, size_t len)
{
    int retval = -1;

    /* (args != NULL) guaranteed by caller. */
    if (len > 0) {
        retval = VSFTPServerSetCwd(args, len);
    }

    if (retval == 0) {
        retval = VSFTPServerSend("250 Directory successfully changed.");
    } else {
        retval = VSFTPServerSend("550 Failed to change directory.");
    }

    return retval;
}

static int CommandHandlerRetr(const char *args, size_t len)
{
    int retval = -1;
    int pasv_client_sock = -1;
    int pasv_server_sock = -1;
    char buf[RESPONSE_LEN_MAX];
    char bufFilePath[PATH_LEN_MAX];
    bool isBinary = false;
    const char *fileNotFound = "550 File not found.";
    const char *localError = "451 Requested action aborted: Local error in processing.";
    bool isFileError = false;

    /* (args != NULL) guaranteed by caller. */
    if (len > 0) {
        /* Get socket. */
        retval = VSFTPServerGetTransferSocket(&pasv_server_sock);
    }

    if (retval == 0) {
        retval = VSFTPServerGetFileAbsPath(args, len, bufFilePath, sizeof(bufFilePath));
        if (retval != 0) {
            isFileError = true;
        }
    }

    if (retval == 0) {
        retval = VSFTPServerAcceptTransferConnection(pasv_server_sock, &pasv_client_sock);
    }

    if (retval == 0) {
        retval = VSFTPServerGetTransferMode(&isBinary);
    }

    if (retval == 0) {
        if (isBinary == true) {
            (void)snprintf(buf, sizeof(buf), "150 BINARY mode data connection for %s.", args);
        } else {
            (void)snprintf(buf, sizeof(buf), "150 ASCII mode data connection for %s.", args);
        }
        retval = VSFTPServerSend(buf);
    }

    if (retval == 0) {
        retval = VSFTPServerSendfile(pasv_client_sock, bufFilePath, strlen(bufFilePath));
    }

    (void)close(pasv_client_sock);
    (void)VSFTPServerCloseTransferSocket(pasv_server_sock);

    if (retval == 0) {
        retval = VSFTPServerSend("226 Transfer Complete.");
    } else {
        retval = VSFTPServerSend(isFileError == true ? fileNotFound : localError);
    }

    return retval;
}

static int CommandHandlerType(const char *args, size_t len)
{
    int retval = -1;

    if ((len == 1) && ((args[0] == 'I') || (args[0] == 'i'))) {
        retval = VSFTPServerSend("200 Switching to Binary mode.");
        if (retval == 0) {
            retval = VSFTPServerSetTransferMode(true);
        }
    } else if ((len == 1) && ((args[0] == 'A') || (args[0] == 'a'))) {
        /* Type A must be always accepted according to RFC, but we do not support it. */
        retval = VSFTPServerSend("200 Switching to ASCII mode.");
        if (retval == 0) {
            retval = VSFTPServerSetTransferMode(false);
        }
    } else {
        retval = VSFTPServerSend("504 Command not implemented for that parameter.");
    }

    return retval;
}

static int CommandHandlerHelp(const char *args, size_t len)
{
    char buf[HELP_LEN_MAX];
    int written = 0;
    int retval = -1;

    written = snprintf(&buf[written], sizeof(buf) - written, "214-The following commands are recognized.\r\n");
    for (unsigned long i = 0; i < DIM(commands); i++) {
        written += snprintf(&buf[written], sizeof(buf) - written, " %s", commands[i].name);
    }
    (void)snprintf(&buf[written], sizeof(buf) - written, "\r\n214 Help OK.");
    retval = VSFTPServerSend(buf);

    return retval;
}

static int CommandHandlerQuit(const char *args, size_t len)
{
    return VSFTPServerSend("221 Bye.");
}

int VSFTPCommandsParse(const char *buffer, size_t len)
{
    bool commandFound = false;
    int retval = -1;

    for (unsigned long i = 0; i < DIM(commands); i++) {
        if (strncmp(buffer, commands[i].name, commands[i].nameLen) == 0) {
            if (len > commands[i].nameLen) {
                /* Commands and their arguments are separated by a ' ', pass the handler the argument. */
                retval = commands[i].handle(&buffer[commands[i].nameLen + 1], len - commands[i].nameLen - 1);
            } else {
                retval = commands[i].handle(NULL, 0);
            }

            commandFound = true;
            break;
        }
    }

    if (commandFound == false) {
        retval = VSFTPServerSend("502 Command not implemented.");
    }

    return retval;
}
