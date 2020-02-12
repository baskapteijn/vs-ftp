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

#include <stddef.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdlib.h>
#include "vsftp_filesystem.h"
#include "config.h"

//first call d should start with a valid pointer to pointer to 0
int VSFTPFilesystemListDirPerFile(const char *path, size_t len, char *buf, size_t size, bool prependDir, DIR **d)
{
    struct dirent *ldir = NULL;
    int written = 0;
    int retval = 0;

    /* We do not need len, opendir will check it. */
    (void)len;

    if (d == NULL) {
        //invalid
        retval = -1;
    } else if (*d == NULL) {
        //first call
        *d = opendir(path);
    } else {
        //continue with d
    }

    if (retval == 0) {
        //continue
        if ((ldir = readdir(*d)) != NULL) {
            if (prependDir == false) {
                written = snprintf(buf, size, "%s\r\n", ldir->d_name);
            } else {
                written = snprintf(buf, size, "%s/%s\r\n", path, ldir->d_name);
            }
            if (written >= size) {
                /* buf too small to contain all directory data. */
                retval = -1;
            }
        }

        if ((retval != 0) || (ldir == NULL)) {
            closedir(*d);
            *d = NULL;
        }
    }

    return retval;
}

int VSFTPFilesystemIsDir(const char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode) == true ? 0 : -1;
}

int VSFTPFilesystemIsRelativeDir(const char *cwd, const size_t len, const char *path, const size_t dirLen,
                                 char *abspath, const size_t size)
{
    int retval = -1;
    char buf[PATH_LEN_MAX];

    (void)snprintf(buf, sizeof(buf), "%s/%s", cwd, path);
    retval = VSFTPFilesystemIsDir(buf);

    if (retval == 0) {
        if (abspath != NULL) {
            retval = VSFTPFilesystemGetAbsPath(buf, sizeof(buf), abspath, size);
        }
    }

    return retval;
}

int VSFTPFilesystemIsFile(const char *file)
{
    struct stat path_stat;
    stat(file, &path_stat);
    return S_ISREG(path_stat.st_mode) == true ? 0 : -1;
}

int VSFTPFilesystemIsRelativeFile(const char *cwd, const size_t len, const char *file, const size_t fileLen,
                                  char *abspath, const size_t size)
{
    int retval = -1;
    char buf[PATH_LEN_MAX];

    (void)snprintf(buf, sizeof(buf), "%s/%s", cwd, file);
    retval = VSFTPFilesystemIsFile(buf);

    if (retval == 0) {
        if (abspath != NULL) {
            retval = VSFTPFilesystemGetAbsPath(buf, sizeof(buf), abspath, size);
        }
    }

    return retval;
}

int VSFTPFilesystemGetAbsPath(const char *path, const size_t len, char *absPath, const size_t size)
{
    int retval = 0;

    if ((path == NULL) || (len == 0) || (absPath == NULL) || (size == 0)) {
        retval = -1;
    }

    if (retval == 0) {
        if (realpath(path, absPath) == 0) {
            retval = -1;
        }
    }

    return retval;
}
