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
#ifndef IO_H__
#define IO_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

#define FTPLOG(format, ...)      FTPLog(__FILENAME__, __LINE__, format, ##__VA_ARGS__)

static inline void FTPLog(const char *file, uint32_t line, const char *format, ...)
{
    va_list args;

    /* Print to stdio. */
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    printf("%d-%02d-%02d %02d:%02d:%02d ",
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    printf("%s[%u]: ", file, line);

    va_start(args, format);
    (void)vprintf(format, args);
    va_end(args);

    /* Print to file. */
    //TODO
}

#endif /* IO_H__ */
