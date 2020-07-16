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
#ifndef VSFTP_FILESYSTEM_H__
#define VSFTP_FILESYSTEM_H__

extern int VSFTPFilesystemListDirPerLine(const char *path, size_t pathLen, char *buf, size_t size, size_t *bufLen,
                                         bool prependDir, void **cookie);
extern int VSFTPFilesystemIsDir(const char *dir, size_t dirLen);
extern int VSFTPFilesystemIsFile(const char *file, size_t fileLen);
extern int VSFTPFilesystemGetRealPath(const char *cwd, size_t cwdLen, const char *path, size_t pathLen,
                                       char *realPath, size_t size, size_t *realPathLen);
extern int VSFTPFilesystemOpenFile(const char *absPath, size_t absPathLen, int *fd, size_t *size);
extern int VSFTPFilesystemCloseFile(int fd);

#endif /* VSFTP_FILESYSTEM_H__ */

