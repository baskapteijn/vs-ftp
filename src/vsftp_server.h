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
#ifndef VSFTP_SERVER_H__
#define VSFTP_SERVER_H__

typedef struct {
    uint16_t port;
    const char *rootPath;
    const char *ipAddr;
    size_t ipAddrLen;
    size_t rootPathLen;
} VSFTPConfigData_s;

extern int VSFTPServerInitialize(const VSFTPConfigData_s *vsftpConfigData);
extern int VSFTPServerStart(void);
extern int VSFTPServerStop(void);
extern int VSFTPServerHandler(void);
extern int VSFTPServerClientDisconnect(void);
extern int VSFTPServerCreateTransferSocket(const uint16_t port_num, int *sock);
extern int VSFTPServerCloseTransferSocket(const int sock);
extern int VSFTPServerGetTransferSocket(int *sock);
extern int VSFTPServerAcceptTransferConnection(const int sock, int *con_sock);
extern int VSFTPServerGetCwd(char *buf, const size_t size, size_t *len);
extern int VSFTPServerGetDirAbsPath(const char *dir, const size_t len, char *absPath, const size_t size,
                                    size_t *absPathLen);
extern int VSFTPServerSetCwd(const char *dir, const size_t len);
extern int VSFTPServerGetFileAbsPath(const char *file, const size_t len, char *absFilePath, const size_t size,
                                     size_t *absFilePathLen);
extern int VSFTPServerSendfile(const int sock, const char *pathTofile, const size_t len);
extern int VSFTPServerSetTransferMode(const bool binary);
extern int VSFTPServerGetTransferMode(bool *binary);
extern int VSFTPServerListDirPerFile(const char *dir, size_t len, char *buf, size_t size, size_t *bufLen,
                                     bool prependDir, void **cookie);
extern int VSFTPServerGetServerIP4(char *buf, const size_t size);
extern int VSFTPServerSendReply(const char *__restrict __format, ...);
extern int VSFTPServerSendReplyOwnBuf(char *buf, const size_t size, const size_t len);

#endif /* VSFTP_SERVER_H__ */
