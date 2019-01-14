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
#ifndef CONFIG_H__
#define CONFIG_H__

#define HELP_LEN_MAX        256U
#define PATH_LEN_MAX        256U
#define RESPONSE_LEN_MAX    (256U + 32U) /* Must always be max of previous + some more. */

#define FILE_READ_BUF_SIZE  8192U

#define PASV_PORT_NUMBER    40000U

#endif /* CONFIG_H__ */
