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

#include "stdint.h"

#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

#define FTPLOG(format, ...)      FTPLog(__FILENAME__, __LINE__, format, ##__VA_ARGS__)

extern void FTPLog(const char *file, uint32_t line, const char *format, ...);

#endif /* IO_H__ */

