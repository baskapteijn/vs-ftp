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

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "config.h"
#include "vsftp_filesystem.h"

typedef struct {
    char cwd[PATH_LEN_MAX];
    size_t cwdLen;
} VSFTPFilesystemData_s;

static VSFTPFilesystemData_s filesystemData;

static int ConcatCwdAndPath(const char *cwd, size_t cwdLen, const char *path, size_t pathLen,
                            char *concatPath, size_t size, size_t *concatPathLen);
static int IsAbsPath(const char *path, size_t pathLen);
static int GetAbsPath(const char *path, size_t pathLen, char *absPath, size_t size, size_t *absPathLen);

static int ConcatCwdAndPath(const char *cwd, const size_t cwdLen, const char *path, const size_t pathLen,
                            char *concatPath, const size_t size, size_t *concatPathLen)
{
    int retval = -1;
    int written = 0;

    if ((cwd != NULL) && (cwdLen > 0) &&
        (path != NULL) && (pathLen > 0) &&
        (concatPath != NULL) && (size > 0) && (concatPathLen != 0)) {
        retval = 0;
    }
    
    if (retval == 0) {
        if ((cwdLen + pathLen) < size) {
            written = snprintf(concatPath, size, "%s/%s", cwd, path);
            if ((written >= 0) && ((size_t)written < size)) {
                *concatPathLen = (size_t)written;
                retval = 0;
            } /* Else: buf too small to contain dir path. */
        }
    }

    return retval;
}

static int IsAbsPath(const char *path, const size_t pathLen)
{
    int retval = -1;

    if ((path != NULL) && (pathLen > 0)) {
        retval = 0;
    }

    if (retval == 0) {
        if (path[0] != '/') {
            retval = -1;
        }
    }

    return retval;
}

static int GetAbsPath(const char *path, const size_t pathLen, char *absPath, const size_t size, size_t *absPathLen)
{
    int retval = -1;
    char *p = NULL;
    char cwd[PATH_LEN_MAX];
    size_t cwdLen = 0;
    size_t pLen = 0;

    if ((path != NULL) && (pathLen > 0) &&
        (absPath != NULL) && (size > 0) && (absPathLen != NULL)) {
        retval = 0;
    }

    if (retval == 0) {
        retval = IsAbsPath(path, pathLen);
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
            retval = VSFTPFilesystemGetCwd(cwd, sizeof(cwd), &cwdLen);
            if (retval == 0) {
                retval = ConcatCwdAndPath(cwd, cwdLen, path, pathLen, absPath, size, absPathLen);
            }

            /* Pass NULL in `resolved_path`. If `realpath` resolves the path it will alloc a buffer (up to PATH_MAX size)
             * and return it's pointer. We have to free it. This is the safest way since we cannot pass the size of our own
             * buffer for `resolved_path`.
             */
            if (retval == 0) {
                p = realpath(absPath, NULL);
                if (p != NULL) {
                    pLen = strnlen(p, PATH_LEN_MAX);
                    if (size > pLen) {
                        (void)strncpy(absPath, p, size);
                        *absPathLen = pLen;
                    } else {
                        retval = -1;
                    }

                    free(p);
                }
            }
        }
    }

    return retval;
}

int VSFTPFilesystemSetCwd(const char *dir, const size_t len)
{
    int retval = -1;

    /* Checks are performed in callee. */
    if (sizeof(filesystemData.cwd) > len) {
        (void)strncpy(filesystemData.cwd, dir, sizeof(filesystemData.cwd));
        filesystemData.cwdLen = len;
        retval = 0;
    }

    return retval;
}

int VSFTPFilesystemGetCwd(char *buf, const size_t size, size_t *len)
{
    int retval = -1;
    int written = 0;

    /* Check if CWD has been initialized and if the buffer is large enough to contain it. */
    if ((buf != NULL) && (size > 0) && (len != NULL) &&
        (filesystemData.cwdLen > 0) && (filesystemData.cwdLen < size)) {
        retval = 0;
    }

    if (retval == 0) {
        written = snprintf(buf, size, filesystemData.cwd, filesystemData.cwdLen);
        if ((written >= 0) && ((size_t)written < size)) {
            *len = (size_t)written;
        } else {
            retval = -1;
        }
    }

    return retval;
}

int VSFTPFilesystemListDirPerLine(const char *path, size_t pathLen, char *buf, size_t size, size_t *bufLen,
                                  bool prependDir, void **cookie)
{
    struct dirent *ldir = NULL;
    int written = 0;
    int retval = -1;
    DIR *d = NULL;

    if ((path != NULL) && (pathLen > 0) && (buf != NULL) && (size > 0) && (bufLen != NULL) && (cookie != NULL)) {
        retval = 0;
    }

    if (retval == 0) {
        if (*cookie == NULL) {
            //first call
            d = opendir(path);
            if (d == NULL) {
                retval = -1;
            }
        } else {
            //continue with d
            d = *cookie;
        }
    }

    if (retval == 0) {
        //continue
        if ((ldir = readdir(d)) != NULL) {
            if (prependDir == false) {
                written = snprintf(buf, size, "%s", ldir->d_name);
            } else {
                written = snprintf(buf, size, "%s/%s", path, ldir->d_name);
            }

            if ((written >= 0) && ((size_t)written < size)) {
                *bufLen = (size_t)written;
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

int VSFTPFilesystemIsDir(const char *dir, const size_t dirLen)
{
    int retval = -1;
    struct stat path_stat;

    if ((dir != NULL) && (dirLen > 0)) {
        retval = 0;
    }

    if (retval == 0) {
        stat(dir, &path_stat);

        if (S_ISDIR(path_stat.st_mode) == 0) {
            retval = -1;
        }
    }

    return retval;
}

int VSFTPFilesystemIsFile(const char *file, const size_t fileLen)
{
    int retval = -1;
    struct stat path_stat;

    if ((file != NULL) && (fileLen > 0)) {
        retval = 0;
    }

    if (retval == 0) {
        stat(file, &path_stat);

        if (S_ISREG(path_stat.st_mode) == 0) {
            retval = -1;
        }
    }

    return retval;
}

int VSFTPFilesystemGetDirAbsPath(const char *dir, const size_t len, char *absPath, const size_t size,
                             size_t *absPathLen)
{
    int retval = -1;

    if ((dir != NULL) && (len > 0) && (absPath != NULL) && (size > 0)) {
        retval = 0;
    }

    if (retval == 0) {
        retval = GetAbsPath(dir, len, absPath, size, absPathLen);
    }

    if (retval == 0) {
        retval = VSFTPFilesystemIsDir(absPath, *absPathLen);
    }

    return retval;
}

int VSFTPFilesystemGetFileAbsPath(const char *file, const size_t len, char *absFilePath, const size_t size,
                                  size_t *absFilePathLen)
{
    int retval = -1;

    /* Checks are performed in callee. */

    retval = GetAbsPath(file, len, absFilePath, size, absFilePathLen);
    if (retval == 0) {
        retval = VSFTPFilesystemIsFile(absFilePath, *absFilePathLen);
    }

    return retval;
}

int VSFTPFilesystemOpenFile(const char *absPath, const size_t absPathLen, int *fd, size_t *size)
{
    int retval = -1;
    struct stat stat_buf;

    if ((absPath != NULL) && (absPathLen > 0) && (fd != NULL) && (size != NULL)) {
        retval = 0;
    }

    if (retval == 0) {
        retval = access(absPath, R_OK);
    }

    if (retval == 0) {
        *fd = open(absPath, O_RDONLY);
        if (*fd == -1) {
            retval = -1;
        } else {
            retval = fstat(*fd, &stat_buf);
        }
    }

    if (retval == 0) {
        *size = (size_t)stat_buf.st_size;
    }

    return retval;
}

int VSFTPFilesystemCloseFile(const int fd)
{
    /* Checks are performed in callee. */

    return close(fd);
}
