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
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "config.h"
#include "vsftp_server.h"
#include "vsftp_filesystem.h"

static int ConcatCwdAndPath(const char *cwd, const size_t cwdLen, const char *path, const size_t pathLen,
                            char *concatPath, const size_t size, size_t *concatPathLen);

static int ConcatCwdAndPath(const char *cwd, const size_t cwdLen, const char *path, const size_t pathLen,
                            char *concatPath, const size_t size, size_t *concatPathLen)
{
    int retval = -1;
    int written = 0;

    written = snprintf(concatPath, size, "%s/%s", cwd, path);
    if ((written >= 0) && (written < size)) {
        *concatPathLen = written;
        retval = 0;
    } /* Else: buf too small to contain dir path. */

    return retval;
}

//first call d should start with a valid pointer to pointer to 0
/* Does (!) modify out variables while it could still fail. */
int VSFTPFilesystemListDirPerFile(const char *path, size_t pathLen, char *buf, size_t size, size_t *bufLen,
                                  bool prependDir, void **cookie)
{
    struct dirent *ldir = NULL;
    int written = 0;
    int retval = -1;
    DIR *d = NULL;

    /* We do not need len, opendir will check it. */

    if (cookie == NULL) {
        //invalid
        retval = -1;
    } else if (*cookie == NULL) {
        //first call
        d = opendir(path);
        if (d != NULL) {
            retval = 0;
        }
    } else {
        //continue with d
        d = *cookie;
        retval = 0;
    }

    if (retval == 0) {
        //continue
        if ((ldir = readdir(d)) != NULL) {
            if (prependDir == false) {
                written = snprintf(buf, size, "%s\r\n", ldir->d_name);
            } else {
                written = snprintf(buf, size, "%s/%s\r\n", path, ldir->d_name);
            }

            if ((written >= 0) && (written < size)) {
                *bufLen = written;
            } else {
                /* buf too small to contain directory data. */
                retval = -1;
            }
        }

        if ((retval == 0) && (ldir == NULL)) {
            closedir(d);
            d = NULL;
        }
    }

    if ((retval != 0) && (d != NULL)) {
        closedir(d);
        d = NULL;
    }

    if (retval == 0) {
        *cookie = d;
    }

    return retval;
}

int VSFTPFilesystemIsDir(const char *path, const size_t pathLen)
{
    int retval = -1;
    struct stat path_stat;
    stat(path, &path_stat);

    if (S_ISDIR(path_stat.st_mode) != 0) {
        retval = 0;
    }

    return retval;
}

int VSFTPFilesystemIsFile(const char *file, const size_t len)
{
    int retval = -1;
    struct stat path_stat;
    stat(file, &path_stat);

    if (S_ISREG(path_stat.st_mode) != 0) {
        retval = 0;
    }

    return retval;
}

int VSFTPFilesystemIsAbsPath(const char *path, const size_t pathLen)
{
    int retval = -1;

    if ((pathLen > 0) && (path[0] == '/')) {
        retval = 0;
    }

    return retval;
}

/* Does not modify absPath or absPathLen until all is valid. */
int VSFTPFilesystemGetAbsPath(const char *path, const size_t pathLen,
                              char *absPath, const size_t size, size_t *absPathLen)
{
    int retval = -1;
    char *p = NULL;
    char cwd[PATH_LEN_MAX];
    char lAbsPath[PATH_LEN_MAX];
    size_t cwdLen = 0;
    size_t lAbsPathLen = 0;

    if ((path != NULL) && (pathLen > 0) &&
        (absPath != NULL) && (size >= 0) && (absPathLen != NULL)) {
        retval = 0;
    }

    if (retval == 0) {
        retval = VSFTPFilesystemIsAbsPath(path, pathLen);
        if (retval == 0) {
            /* It's absolute. */
            p = realpath(path, NULL);
            if (p == NULL) {
                retval = -1;
            }

            if (retval == 0) {
                (void)strncpy(absPath, p, size);
                *absPathLen = strnlen(p, PATH_LEN_MAX);
                free(p);
                retval = 0;
            }
        } else {
            /* It's relative. */
            retval = VSFTPServerGetCwd(cwd, sizeof(cwd), &cwdLen);
            if (retval == 0) {
                retval = ConcatCwdAndPath(cwd, cwdLen, path, pathLen, lAbsPath, sizeof(lAbsPath), &lAbsPathLen);
            }

            /* Pass NULL in `resolved_path`. If `realpath` resolves the path it will alloc a buffer (up to PATH_MAX size)
             * and return it's pointer. We have to free it. This is the safest way since we cannot pass the size of our own
             * buffer for `resolved_path`.
             */
            if (retval == 0) {
                p = realpath(lAbsPath, NULL);
                if (p != NULL) {
                    (void)strncpy(absPath, p, size);
                    free(p);
                    *absPathLen = lAbsPathLen;
                    retval = 0;
                }
            }
        }
    }

    return retval;
}

/* Does not modify fd or size until all is valid. */
int VSFTPFilesystemOpenFile(const char *absPath, const size_t pathLen, int *fd, size_t *size)
{
    int lfd = -1;
    int retval = -1;
    struct stat stat_buf;

    retval = access(absPath, R_OK);

    if (retval == 0) {
        lfd = open(absPath, O_RDONLY);
        if (lfd == -1) {
            retval = -1;
        } else {
            *fd = lfd;
            retval = fstat(lfd, &stat_buf);
        }
    }

    if (retval == 0) {
        *size = (size_t)stat_buf.st_size;
    }

    return retval;
}

int VSFTPFilesystemCloseFile(const int fd)
{
    return close(fd);
}

