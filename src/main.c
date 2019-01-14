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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include "vsftp_server.h"
#include "version.h"

#define DECIMAL_STRING_LEN_MAX          10u

volatile sig_atomic_t quit = 0;

/* UINT32_MAX equivalent string. */
const char *DecimalStringValueMax = "4294967295";

static VSFTPData_s vsftpData;

static void Terminate(int signum);
static void ParseDecimal(const char *string, size_t len, uint32_t *number);
static bool IsDecimalChar(char c);
static bool IsDecimal(const char *string, size_t len);
static bool DirectoryExists(const char *path);
static void PrintHelp(void);

static void Terminate(int signum)
{
    quit = 1;
}

/*!
 * \brief Parse a string that represents a Decimal value and returns its value.
 * \param string
 *      The string to parse.
 * \param len
 *      The length of the input string (excluding the string terminator).
 * \param[out] number
 *      The number represented by the numeric string.
 */
static void ParseDecimal(const char *string, size_t len, uint32_t *number)
{
    uint64_t val = 0;
    int64_t i = 0;
    uint64_t value = 0;

    *number = 0;

    val = 1;
    for (i = ((int64_t)len - 1); i >= 0; i--) {
        value = (string[i] - 0x30) * val;
        *number += value;
        val *= 10;
    }
}

/*!
 * \brief Indicate if a character represents a valid Decimal character.
 * \param c
 *      The character to verify.
 * \returns
 *      true if the character represents a valid Decimal character, otherwise false.
 */
static bool IsDecimalChar(char c)
{
    bool isDecimalChar = false;

    if ((c >= '0') && (c <= '9')) {
        isDecimalChar = true;
    }

    return isDecimalChar;
}

/*!
 * \brief Indicate if a string represents a valid Decimal value.
 * \param string
 *      The string to verify.
 * \param len
 *      The length of the input string (excluding the string terminator).
 * \returns
 *      true if the string represents a valid Decimal value, otherwise false.
 */
static bool IsDecimal(const char *string, size_t len)
{
    bool isDecimal = true;
    uint64_t i = 0;

    for (i = 0; i < len; i++) {
        if (IsDecimalChar(string[i]) == false) {
            isDecimal = false;
            break;
        }
    }

    if (isDecimal == true) {
        /* Is the len okay? */
        if (len == 0) {
            return -1;
        } else if (len == DECIMAL_STRING_LEN_MAX) {
            for (i = 0; i < DECIMAL_STRING_LEN_MAX; i++) {
                if ((string[i] - 0x30) > (DecimalStringValueMax[i] - 0x30)) {
                    /* This isn't gonna fit in UINT32_MAX. */
                    isDecimal = false;
                    break;
                } else if ((string[i] - 0x30) == (DecimalStringValueMax[i] - 0x30)) {
                    /* Continue checking the lower digits, it can still become too large. */
                    continue;
                } else {
                    /* No need to continue checking the lower digits, it cannot become too large. */
                    break;
                }
            }
        } else if (len > DECIMAL_STRING_LEN_MAX) {
            isDecimal = false;
        }
        /* else: will not happen because it's protected by the first loop. */
    }

    return isDecimal;
}

/**
 * \brief Function to check whether a directory exists or not.
 * \returns
 *      true if the directory exists, otherwise false.
 */
static bool DirectoryExists(const char *path)
{
    struct stat stats;

    stat(path, &stats);

    /* Check for directory existence */
    if (S_ISDIR(stats.st_mode)) {
        return true;
    }

    return false;
}

/*!
 * \brief Print the help menu to the console.
 */
static void PrintHelp(void)
{
    printf("Version %s\n\n", GetVersionString());

    printf("Usage:\n");
    printf("  vs-ftp <port> <root path>\n");
}

/*!
 * \brief This is the program entry.
 * \details
 *      argv[0]: path to this executable
 *      argv[1]: the port used by the VS-FTP server
 *      argv[2]: the root directory used by the VS-FTP server
 * \param argc
 *      The number of string pointed to by argv (argument count).
 * \param argv
 *      A list of strings (argument vector).
 * \returns
 *      0 in case of successful completion or any other value in case of an error.
 */
int main(int argc, char *argv[])
{
    int retval = 0;
    struct sigaction action;

    /* Check input arguments. */
    if (argc != 3) {
        /* Missing or too many arguments. */
        printf("Invalid number of arguments\n\n");
        PrintHelp();
        return -1;
    }

    if (IsDecimal(argv[1], strlen(argv[1])) != true) {
        /* Invalid port. */
        printf("Invalid port \"%s\"\n\n", argv[1]);
        PrintHelp();
        return -1;
    }

    if (DirectoryExists(argv[2]) != true) {
        /* Invalid directory. */
        printf("Invalid directory \"%s\"\n\n", argv[2]);
        PrintHelp();
        return -1;
    }

    /* Initialize termination on signal. */
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = Terminate;
    sigaction(SIGTERM, &action, NULL);

    /* Initialize VS-FTP data structure. */
    ParseDecimal(argv[1], strlen(argv[1]), &vsftpData.port);
    vsftpData.rootPath = argv[2];

    /* Initialize the VS-FTP Server. */
    retval = VSFTPServerInitialize(&vsftpData);
    if (retval != 0) {
        printf("Server initialization failed with exit code %d\n\n", retval);
        return retval;
    }

    /* Start the VS-FTP Server. */
    retval = VSFTPServerStart();
    if (retval != 0) {
        printf("Server start failed with exit code %d\n\n", retval);
        return retval;
    }

    /* Start handling the VS-FTP Server.
     * This function will return 0 unless unless an exception occurs.
     */
    do {
        retval = VSFTPServerHandler();
        if (retval != 0) {
            printf("Server handler failed with exit code %d\n\n", retval);
            return retval;
        }
    } while (quit == 0);

    /* We have been signalled to quit, stop the VS-FTP Server. */
    retval = VSFTPServerStop();
    if (retval != 0) {
        printf("Server stop failed with exit code %d\n\n", retval);
        return retval;
    }

    return 0;
}
