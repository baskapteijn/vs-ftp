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
#ifndef VSFTP_FILESYSTEM_H__
#define VSFTP_FILESYSTEM_H__

#include <stddef.h>

extern int VSFTPFilesystemListDirPerFile(const char *dir, size_t len, char *buf, size_t size, bool prependDir, DIR **d);
extern int VSFTPFilesystemIsDir(const char *dir);
extern int VSFTPFilesystemIsRelativeDir(const char *cwd, const size_t len, const char *dir, const size_t dirLen,
                                        char *abspath, const size_t size);
extern int VSFTPFilesystemIsFile(const char *file);
extern int VSFTPFilesystemIsRelativeFile(const char *cwd, const size_t len, const char *file, const size_t fileLen,
                                         char *abspath, const size_t size);
extern int VSFTPFilesystemGetAbsPath(const char *path, const size_t len, char *absPath, const size_t size);

#endif /* VSFTP_FILESYSTEM_H__ */
