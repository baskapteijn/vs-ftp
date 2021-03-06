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

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/stat.h>
#include "vsftp_filesystem.h"
#include "vsftp_server.h"
#include "config.h"
#include "vsftp_commands.h"

#define DIM(_a)                     (sizeof((_a)) / sizeof(*(_a)))
#define STRLEN(_a)                  ((sizeof((_a)) / sizeof(*(_a))) - 1)

#define FTP_COMMAND_USER            "USER"
#define FTP_COMMAND_SYST            "SYST"
#define FTP_COMMAND_PASV            "PASV"
#define FTP_COMMAND_NLST            "NLST"
#define FTP_COMMAND_PWD             "PWD"
#define FTP_COMMAND_CWD             "CWD"
#define FTP_COMMAND_RETR            "RETR"
#define FTP_COMMAND_SIZE            "SIZE"
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
static int CommandHandlerSyst(const char *args, size_t len);
static int CommandHandlerPasv(const char *args, size_t len);
static int CommandHandlerNlst(const char *args, size_t len);
static int CommandHandlerPwd(const char *args, size_t len);
static int CommandHandlerCwd(const char *args, size_t len);
static int CommandHandlerRetr(const char *args, size_t len);
static int CommandHandlerSize(const char *args, size_t len);
static int CommandHandlerType(const char *args, size_t len);
static int CommandHandlerHelp(const char *args, size_t len);
static int CommandHandlerQuit(const char *args, size_t len);

static Command_s commands[] = {
        { FTP_COMMAND_USER, STRLEN(FTP_COMMAND_USER), CommandHandlerUser },
        { FTP_COMMAND_SYST, STRLEN(FTP_COMMAND_SYST), CommandHandlerSyst },
        { FTP_COMMAND_PASV, STRLEN(FTP_COMMAND_PASV), CommandHandlerPasv },
        { FTP_COMMAND_NLST, STRLEN(FTP_COMMAND_NLST), CommandHandlerNlst },
        { FTP_COMMAND_PWD, STRLEN(FTP_COMMAND_PWD), CommandHandlerPwd },
        { FTP_COMMAND_CWD, STRLEN(FTP_COMMAND_CWD), CommandHandlerCwd },
        { FTP_COMMAND_RETR, STRLEN(FTP_COMMAND_RETR), CommandHandlerRetr },
        { FTP_COMMAND_SIZE, STRLEN(FTP_COMMAND_SIZE), CommandHandlerSize },
        { FTP_COMMAND_TYPE, STRLEN(FTP_COMMAND_TYPE), CommandHandlerType },
        { FTP_COMMAND_HELP, STRLEN(FTP_COMMAND_HELP), CommandHandlerHelp },
        { FTP_COMMAND_QUIT, STRLEN(FTP_COMMAND_QUIT), CommandHandlerQuit }
};

static int CommandHandlerUser(const char *args, size_t len)
{
    const char user[] = "anonymous";
    size_t lLen = STRLEN(user);
    int retval = -1;

    /* (args != NULL) when len > 0 is guaranteed by caller, len may be 0. */

    if ((len == lLen) && (strncmp(user, args, lLen) == 0)) {
        retval = VSFTPServerSendReply("230 User logged in, proceed.");
    } else {
        retval = VSFTPServerSendReply("530 Login incorrect.");
        (void)VSFTPServerClientDisconnect();
    }

    return retval;
}

static int CommandHandlerSyst(const char *args, size_t len)
{
    /* args and len not used. */
    (void)args;
    (void)len;

    return VSFTPServerSendReply("215 UNIX Type: L8");
}

static int CommandHandlerPasv(const char *args, size_t len)
{
    int retval = -1;
    char ipAddrBuf[INET_ADDRSTRLEN];
    size_t ipAddrLen = 0;
    uint8_t p1 = 0;
    uint8_t p2 = 0;
    uint16_t portNumber = PASV_PORT_NUMBER;

    /* args and len not used. */
    (void)args;
    (void)len;

    /* Close a socket if it is still open (could be happening with unsupported reception of list command). */
    (void)VSFTPServerCloseTransferClientSocket();
    (void)VSFTPServerCloseTransferSocket();

    /* Create a new transfer socket connection. */
    retval = VSFTPServerCreateTransferSocket(portNumber);

    /* Transmit socket to client. */
    if (retval == 0) {
        /* Set P1 and P2. */
        p1 = (uint8_t)(portNumber >> 8U);
        p2 = (uint8_t)(portNumber & 0xFFU);

        retval = VSFTPServerGetServerIP4(ipAddrBuf, sizeof(ipAddrBuf), &ipAddrLen);
        if (retval == 0) {
            for (unsigned long i = 0; i < ipAddrLen; i++) {
                if (ipAddrBuf[i] == '.') {
                    ipAddrBuf[i] = ',';
                }
            }
        }
    }

    if (retval == 0) {
        /* (h1,h2,h3,h4,p1,p2) */
        retval = VSFTPServerSendReply("227 Entering Passive Mode (%s,%d,%d).", ipAddrBuf, p1, p2);
    } else {
        retval = VSFTPServerSendReply("425 Cannot open data connection.");
    }

    return retval;
}

static int CommandHandlerNlst(const char *args, size_t len)
{
    int retval = -1;
    char buf[PATH_LEN_MAX];
    size_t bufLen = 0;
    char realPath[PATH_LEN_MAX];
    size_t realPathLen = 0;
    char cwd[PATH_LEN_MAX];
    size_t cwdLen = 0;
    char serverPath[PATH_LEN_MAX];
    size_t serverPathLen = 0;
    void *d = NULL;
    const char *lpath = 0;
    size_t llen = 0;

    /* (args != NULL) when len > 0 is guaranteed by caller, len may be 0. */

    /* Get cwd. */
    retval = VSFTPServerGetCwd(cwd, sizeof(cwd), &cwdLen);

    if (retval == 0) {
        lpath = cwd;
        llen = cwdLen;

        if (len != 0) {
            /* Get requested dir. */
            retval = VSFTPServerServerPathToRealPath(args, len, realPath, sizeof(realPath), &realPathLen);

            if (retval == 0) {
                retval = VSFTPFilesystemIsDir(realPath, realPathLen);
            }

            /* Make sure the new path is not above the root path. */
            if (retval == 0) {
                retval = VSFTPServerAbsPathIsNotAboveRootPath(realPath, realPathLen);
            }

            if (retval == 0) {
                lpath = realPath;
                llen = realPathLen;
            }
        }
    }

    if (retval == 0) {
        retval = VSFTPServerAcceptTransferClientConnection();
    }

    if (retval == 0) {
        retval = VSFTPServerSendReply("150 Here comes the directory listing.");
    }

    if (retval == 0) {
        /* List dirs and files of given dir. */
        do {
            retval = VSFTPFilesystemListDirPerLine(lpath, llen, buf, sizeof(buf), &bufLen, (len != 0), &d);
            if (retval != 0) {
                break;
            }

            /* Remove the root path from the CWD. */
            if (len != 0) {
                retval = VSFTPServerRealPathToServerPath(buf, sizeof(buf), serverPath, sizeof(serverPath),
                                                         &serverPathLen);
                if (retval == 0) {
                    retval = VSFTPServerSendReplyOwnBufTransfer(serverPath, sizeof(serverPath), serverPathLen);
                }
            } else {
                retval = VSFTPServerSendReplyOwnBufTransfer(buf, sizeof(buf), bufLen);
            }

            if (retval != 0) {
                break;
            }
        } while(d != NULL);
    }

    (void)VSFTPServerCloseTransferClientSocket();
    (void)VSFTPServerCloseTransferSocket();

    if (retval == 0) {
        retval = VSFTPServerSendReply("226 Directory send OK.");
    } else {
        retval = VSFTPServerSendReply("550 Permission Denied.");
    }

    return retval;
}

static int CommandHandlerPwd(const char *args, size_t len)
{
    char cwd[PATH_LEN_MAX];
    char serverPath[PATH_LEN_MAX];
    size_t cwdLen = 0;
    size_t serverPathLen = 0;
    int retval = -1;

    /* args and len not used. */
    (void)args;
    (void)len;

    retval = VSFTPServerGetCwd(cwd, sizeof(cwd), &cwdLen);

    /* Remove the root path from the CWD. */
    if (retval == 0) {
        retval = VSFTPServerRealPathToServerPath(cwd, cwdLen, serverPath, sizeof(serverPath), &serverPathLen);
    }

    if (retval == 0) {
        retval = VSFTPServerSendReply("257 \"%s\"", serverPath);
    } else {
        retval = VSFTPServerSendReply("550 Failed to get directory.");
    }

    return retval;
}

static int CommandHandlerCwd(const char *args, size_t len)
{
    int retval = -1;
    char realPath[PATH_LEN_MAX];
    size_t realPathLen = 0;

    /* (args != NULL) when len > 0 is guaranteed by caller, len may be 0. */

    if (len > 0) {
        retval = VSFTPServerServerPathToRealPath(args, len, realPath, sizeof(realPath), &realPathLen);
    }

    if (retval == 0) {
        retval = VSFTPServerSetCwd(realPath, realPathLen);
    }

    if (retval == 0) {
        retval = VSFTPServerSendReply("250 Directory successfully changed.");
    } else {
        retval = VSFTPServerSendReply("550 Failed to change directory.");
    }

    return retval;
}

static int CommandHandlerRetr(const char *args, size_t len)
{
    int retval = -1;
    char realPath[PATH_LEN_MAX];
    size_t realPathLen = 0;
    bool isBinary = false;
    const char *fileNotFound = "551 File not found.";
    const char *localError = "451 Requested action aborted: Local error in processing.";
    bool isFileError = false;

    /* (args != NULL) when len > 0 is guaranteed by caller, len may be 0. */

    if (len > 0) {
        retval = VSFTPServerServerPathToRealPath(args, len, realPath, sizeof(realPath), &realPathLen);
        if (retval != 0) {
            isFileError = true;
        }
    }

    if (retval == 0) {
        retval = VSFTPFilesystemIsFile(realPath, realPathLen);
        if (retval != 0) {
            isFileError = true;
        }
    }

    if (retval == 0) {
        retval = VSFTPServerAcceptTransferClientConnection();
    }

    if (retval == 0) {
        retval = VSFTPServerGetTransferMode(&isBinary);
    }

    if (retval == 0) {
        if (isBinary == true) {
            retval = VSFTPServerSendReply("150 BINARY mode data connection for %s.", args);
        } else {
            retval = VSFTPServerSendReply("150 ASCII mode data connection for %s.", args);
        }
    }

    if (retval == 0) {
        retval = VSFTPServerSendfileTransfer(realPath, realPathLen);
    }

    (void)VSFTPServerCloseTransferClientSocket();
    (void)VSFTPServerCloseTransferSocket();

    if (retval == 0) {
        retval = VSFTPServerSendReply("226 Transfer Complete.");
    } else {
        retval = VSFTPServerSendReply(isFileError == true ? fileNotFound : localError);
    }

    return retval;
}

static int CommandHandlerSize(const char *args, size_t len)
{
    int retval = -1;
    char realPath[PATH_LEN_MAX];
    size_t realPathLen = 0;
    struct stat filestats;
    const char *fileNotFound = "550 File not found.";
    const char *localError = "451 Requested action aborted: Local error in processing.";
    bool isFileError = false;

    /* (args != NULL) when len > 0 is guaranteed by caller, len may be 0. */

    if (len > 0) {
        retval = 0;
    }

    if (retval == 0) {
        retval = VSFTPServerServerPathToRealPath(args, len, realPath, sizeof(realPath), &realPathLen);
        if (retval != 0) {
            isFileError = true;
        }
    }

    if (retval == 0) {
        retval = VSFTPFilesystemIsFile(realPath, realPathLen);
        if (retval != 0) {
            isFileError = true;
        }
    }

    if (retval == 0) {
        retval = stat(realPath, &filestats);
    }

    if (retval == 0) {
        retval = VSFTPServerSendReply("213 %llu", (unsigned long long int)filestats.st_size);
    } else {
        retval = VSFTPServerSendReply(isFileError == true ? fileNotFound : localError);
    }

    return retval;
}

static int CommandHandlerType(const char *args, size_t len)
{
    int retval = -1;

    if ((len == 1) && ((args[0] == 'I') || (args[0] == 'i'))) {
        retval = VSFTPServerSendReply("200 Switching to Binary mode.");
        if (retval == 0) {
            retval = VSFTPServerSetTransferMode(true);
        }
    } else if ((len == 1) && ((args[0] == 'A') || (args[0] == 'a'))) {
        /* Type A must be always accepted according to RFC, but we do not support it. */
        retval = VSFTPServerSendReply("200 Switching to ASCII mode.");
        if (retval == 0) {
            retval = VSFTPServerSetTransferMode(false);
        }
    } else {
        retval = VSFTPServerSendReply("504 Command not implemented for that parameter.");
    }

    return retval;
}

static int CommandHandlerHelp(const char *args, size_t len)
{
    char buf[HELP_LEN_MAX];
    int written = 0;
    int retval = -1;

    /* args and len not used. */
    (void)args;
    (void)len;

    written = snprintf(buf, sizeof(buf), "214-The following commands are recognized.\r\n");
    if ((written >= 0) && ((size_t)written < sizeof(buf))) {
        retval = 0;
    }

    if (retval == 0) {
        for (unsigned long i = 0; i < DIM(commands); i++) {
            written += snprintf(&buf[written], sizeof(buf) - written, " %s", commands[i].name);
            if ((written < 0) || ((size_t)written >= sizeof(buf))) {
                retval = -1;
                break;
            }
        }
    }

    if (retval == 0) {
        written += snprintf(&buf[written], sizeof(buf) - written, "\r\n214 Help OK.");
        if ((written < 0) || ((size_t)written >= sizeof(buf))) {
            retval = -1;
        }
    }

    if (retval == 0) {
        retval = VSFTPServerSendReplyOwnBuf(buf, sizeof(buf), (size_t)written);
    }

    return retval;
}

static int CommandHandlerQuit(const char *args, size_t len)
{
    /* args and len not used. */
    (void)args;
    (void)len;

    return VSFTPServerSendReply("221 Bye.");
}

int VSFTPCommandsParse(const char *buffer, size_t len)
{
    bool commandFound = false;
    int retval = ENOTSUP;

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
        /* If a command was not found, the return value should be -1. Do not overwrite this with the following call. */
        (void)VSFTPServerSendReply("502 Command not implemented.");
    }

    return retval;
}

