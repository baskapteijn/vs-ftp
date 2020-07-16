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

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "config.h"
#include "io.h"

static FILE *logFp = NULL;

/*!
 * \brief Logging function for the FTP server.
 * \details
 *      Prints any given output prefixed by a date/time stamp and the filename/line of origin.
 *      Output is printed to stdout and a file in LOG_FILE_PATH.
 * \param file
 *      The filename of origin.
 * \param line
 *      The line of origin.
 * \param format
 *      The format of the output.
 * \param ...
 *      Optional arguments of the output.
 */
void FTPLog(const char *file, uint32_t line, const char *format, ...)
{
    char time_buf[100];
    va_list args;
    struct tm now;
    const time_t now_seconds = time(NULL);

    /* Print to stdio. */
    gmtime_r(&now_seconds, &now);
    sprintf(time_buf, "%d-%02d-%02d_%02d:%02d:%02d",
            now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec);
    printf("%s %s[%u]: ", time_buf, file, line);

    va_start(args, format);
    (void)vprintf(format, args);
    va_end(args);

    /* Print to file. */
    if (logFp == NULL) {
        /* Create filename based on date/time started if it's not yet created. */
        char buf[100];
        sprintf(buf, "%s/%s.log", LOG_FILE_PATH, time_buf);

        logFp = fopen(buf, "a");
    }
    fprintf(logFp, "%s %s[%u]: ", time_buf, file, line);
    va_start(args, format);
    (void)vfprintf(logFp, format, args);
    va_end(args);
    /* Flush immediately so we can tail it. */
    fflush(logFp);
}

