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

    /* Print to stdio. */
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    sprintf(time_buf, "%d-%02d-%02d_%02d:%02d:%02d",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
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

