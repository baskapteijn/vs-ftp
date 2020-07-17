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

static int ConcatCwdAndPath(const char *cwd, size_t cwdLen, const char *path, size_t pathLen,
                            char *concatPath, size_t size, size_t *concatPathLen);
static int IsAbsPath(const char *path);

/*!
 * \brief Concatenate 'cwd' and 'path'.
 * \param cwd
 *      The current working directory.
 * \param cwdLen
 *      The length of 'cwd'.
 * \param path
 *      The (relative) path to concatenate.
 * \param pathLen
 *      The length of 'path'.
 * \param[out] concatPath
 *      A pointer to the storage location for the concatenated path.
 * \param size
 *      The size of 'concatPath'.
 * \param[out] concatPathLen
 *      A pointer to the storage location for the length of 'concatPath'.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
static int ConcatCwdAndPath(const char *cwd, const size_t cwdLen, const char *path, const size_t pathLen,
                            char *concatPath, const size_t size, size_t *concatPathLen)
{
    int retval = -1;
    int written = 0;

    /* Argument checks are performed by the caller. */

    if ((cwdLen + pathLen) < size) {
        written = snprintf(concatPath, size, "%s/%s", cwd, path);
        if ((written >= 0) && ((size_t)written < size)) {
            *concatPathLen = (size_t)written;
            retval = 0;
        } /* Else: buf too small to contain dir path. */
    }

    return retval;
}

/*!
 * \brief Check if the given path is an absolute path.
 * \param path
 *      The path to check.
 * \returns 0 in case it is an absolute path or -1 in case it isn't.
 */
static int IsAbsPath(const char *path)
{
    int retval = -1;

    /* Argument checks are performed by the caller. */

    if (path[0] == '/') {
        retval = 0;
    }

    return retval;
}

/*!
 * \brief Get the files and directories of a directory.
 * \details
 *      Each file/directory is returned as a single string.
 *      This function can be used to iterate through the directory until all files/directories have been passed.
 * \param path
 *      The path to the directory to list.
 * \param pathLen
 *      The length of 'path'.
 * \param[out] buf
 *      A pointer to the storage location for a file/directory listing.
 * \param size
 *      The size of 'buf'.
 * \param[out] bufLen
 *      A pointer to the storage locatoin for the length of 'buf'.
 * \param prependDir
 *      A boolean indicating if the returned file/directory listing should be prepended with 'path'.
 * \param[in,out] cookie
 *      A pointer to a pointer to a storage location indicating where in the directory listing we were.
 * \returns 0 in case of successful (partial) completion or any other value in case of an error.
 */
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
            /* Initial call. */
            d = opendir(path);
            if (d == NULL) {
                retval = -1;
            }
        } else {
            /* Continue with d. */
            d = *cookie;
        }
    }

    if (retval == 0) {
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

/*!
 * \brief Check if the given path is a path to a directory.
 * \param dir
 *      The path to check.
 * \param dirLen
 *      The length of 'dir'.
 * \returns 0 in case it is a directory or -1 in case it isn't.
 */
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

/*!
 * \brief Check if the given path is a path to a file.
 * \param file
 *      The path to check.
 * \param fileLen
 *      The length of 'file'.
 * \returns 0 in case it is a file or -1 in case it isn't.
 */
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

/*!
 * \brief Get the real path.
 * \details
 *      If 'path' is absolute, it will be converted to a real path (symbolic links will be resolved).
 *      If 'path' is relative, it will try to make an absolute path from 'cwd' and 'path'.
 * \param cwd
 *      The current working directory.
 * \param cwdLen
 *      The length of 'cwd'.
 * \param path
 *      The path to get the real path from.
 * \param pathLen
 *      The length of 'path'.
 * \param[out] realPath
 *      A pointer to the storage location for the real path.
 * \param size
 *      The size of 'realPath'.
 * \param[out] realPathLen
 *      The length of 'realPath'.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
int VSFTPFilesystemGetRealPath(const char *cwd, const size_t cwdLen, const char *path, const size_t pathLen,
                               char *realPath, const size_t size, size_t *realPathLen)
{
    int retval = -1;
    char *p = NULL;
    char absPath[PATH_LEN_MAX];
    size_t absPathLen = 0;
    size_t pLen = 0;
    const char *lpath = 0;

    /* cwd and cwdLen are allowed to be 0, only check them if a call to ConcatCwdAndPath() is required. */
    if ((path != NULL) && (pathLen > 0) &&
        (realPath != NULL) && (size > 0) && (realPathLen != NULL)) {
        retval = 0;
    }

    if (retval == 0) {
        retval = IsAbsPath(path);
        if (retval == 0) {
            /* It's absolute. */
            lpath = path;
        } else {
            if ((cwd != NULL) && (cwdLen > 0)) {
                /* It's relative. */
                retval = ConcatCwdAndPath(cwd, cwdLen, path, pathLen, absPath, sizeof(absPath), &absPathLen);
                if (retval == 0) {
                    lpath = absPath;
                }
            }
        }

        /* Pass NULL in `resolved_path`. If `realpath` resolves the path it will alloc a buffer (up to PATH_MAX size)
         * and return it's pointer. We have to free it. This is the safest way since we cannot pass the size of our own
         * buffer for `resolved_path`.
         */
        if (retval == 0) {
            retval = -1;

            p = realpath(lpath, NULL);
            if (p != NULL) {
                pLen = strnlen(p, PATH_LEN_MAX);
                if (size > pLen) {
                    (void)strncpy(realPath, p, size);
                    *realPathLen = pLen;
                    retval = 0;
                }

                free(p);
            }
        }
    }

    return retval;
}

/*!
 * \brief Open a file.
 * \param absPath
 *      The absolute path to the file, including the filename.
 *      This does not have to be a real path, symbolic links are automatically dereferenced.
 * \param absPathLen
 *      The length of 'absPath'.
 * \param[out] fd
 *      A pointer to the storage location for the file descriptor.
 * \param size
 *      The size of the opened file.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
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

/*!
 * \brief Close a file.
 * \param fd
 *      The file descriptor.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
int VSFTPFilesystemCloseFile(const int fd)
{
    /* Checks are performed in callee. */

    return close(fd);
}

