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
#ifndef VSFTP_SERVER_H__
#define VSFTP_SERVER_H__

#include <stdint.h>
#include <stdbool.h>

extern int VSFTPServerInitialize(const char *rootPath, size_t rootPathLen, const char *ipAddr, size_t ipAddrLen,
                                 uint16_t port);
extern int VSFTPServerStart(void);
extern int VSFTPServerStop(void);
extern int VSFTPServerHandler(void);

extern int VSFTPServerClientDisconnect(void);
extern int VSFTPServerCreateTransferSocket(uint16_t port_num);
extern int VSFTPServerCloseTransferSocket(void);
extern int VSFTPServerAcceptTransferClientConnection(void);
extern int VSFTPServerCloseTransferClientSocket(void);

extern int VSFTPServerSendfileTransfer(const char *pathTofile, size_t len);
extern int VSFTPServerSetTransferMode(bool binary);
extern int VSFTPServerGetTransferMode(bool *binary);

extern int VSFTPServerIsValidIPAddress(char *ipAddress);
extern int VSFTPServerGetServerIP4(char *buf, size_t size, size_t *len);
extern int VSFTPServerAbsPathIsNotAboveRootPath(const char *absPath, size_t absPathLen);
extern int VSFTPServerServerPathToRealPath(const char *serverPath, size_t serverPathLen, char *realPath,
                                           size_t size, size_t *realPathLen);
extern int VSFTPServerRealPathToServerPath(const char *realPath, size_t realPathLen, char *serverPath,
                                           size_t size, size_t *serverPathLen);
extern int VSFTPServerSetCwd(const char *dir, size_t len);
extern int VSFTPServerGetCwd(char *buf, size_t size, size_t *len);

extern int VSFTPServerSendReply(const char *__restrict format, ...);
extern int VSFTPServerSendReplyOwnBuf(char *buf, size_t size, size_t len);
extern int VSFTPServerSendReplyOwnBufTransfer(char *buf, size_t size, size_t len);

#endif /* VSFTP_SERVER_H__ */

