/* $Revision$ */
/*
 *                          Copyright 2020                      
 *                         Green Hills Software                      
 *
 *    This program is the property of Green Hills Software, its
 *    contents are proprietary information and no part of it is to be
 *    disclosed to anyone except employees of Green Hills Software,
 *    or as agreed in writing signed by the President of Green Hills
 *    Software.
 *
 */

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "config.h"
#include "io.h"

static FILE *logFp = NULL;

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

