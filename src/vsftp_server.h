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

#include <stdint.h>

typedef struct {
    uint32_t port;
    const char *rootPath;
} VSFTPData_s;

extern int VSFTPServerHandle(void);
extern int VSFTPServerInitialize(VSFTPData_s *vsftpData);

#endif /* VSFTP_SERVER_H__ */
