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

#include "vsftp_server.h"
#include "vsftp_commands.h"
#include "vsftp_filesystem.h"

typedef enum {
    VSFTP_STATE_UNINITIALIZED = 0,
    VSFTP_STATE_INITIALIZED = 1,
    VSFTP_STATE_STARTED = 2,
} VSFTPStates_e;

/*!
 * \brief Maintains the server state.
 * \details
 *      The server state flow is as follows:
 *
 *      VSFTPServerInitialize() called
 *        VSFTP_STATE_UNINITIALIZED -> VSFTP_STATE_INITIALIZED
 *      VSFTPServerStart() called
 *        VSFTP_STATE_INITIALIZED -> VSFTP_STATE_STARTED
 *      VSFTPServerStop() called
 *        VSFTP_STATE_STARTED -> VSFTP_STATE_INITIALIZED
 *
 *      This means that initialization can only be performed once, and after initialization the
 *      server can be started and stopped as many times as required.
 */
static VSFTPStates_e state = VSFTP_STATE_UNINITIALIZED;

/*!
 * \brief Initialize the VS-FTP server.
 * \pre Can only be called when the server state is VSFTP_STATE_UNINITIALIZED.
 * \details
 *      Initializes the Server with data coming in through the vsftpData structure.
 * \param vsftpData
 *      A pointer to the VS-FTP configuration data.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
int VSFTPServerInitialize(VSFTPData_s *vsftpData)
{
    if (state != VSFTP_STATE_UNINITIALIZED) {
        return -1;
    }

    /* TODO: implement. */
    return -1;
}

/*!
 * \brief Start the VS-FTP server.
 * \pre Can only be called when the server state is VSFTP_STATE_INITIALIZED.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
int VSFTPServerStart(void)
{
    if (state != VSFTP_STATE_INITIALIZED) {
        return -1;
    }

    /* TODO: implement. */
    return -1;
}

/*!
 * \brief Stop the VS-FTP server.
 * \pre Can only be called when the server state is VSFTP_STATE_STARTED.
 * \details
 *      Stop serving and clean-up to restore the initialized state.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
int VSFTPServerStop(void)
{
    if (state != VSFTP_STATE_STARTED) {
        return -1;
    }

    /* TODO: implement. */
    return -1;
}

/*!
 * \brief Handle the next server iteration.
 * \pre Can only be called when the server state is VSFTP_STATE_STARTED.
 * \details
 *      - Check for a (newly) received command.
 *
 *      And optionally:
 *
 *      - Handle a received command.
 *      - Continue a transmission part.
 *
 *      This function shall return swiftly as to allow the user to do tasks in between.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
int VSFTPServerHandler(void)
{
    if (state != VSFTP_STATE_STARTED) {
        return -1;
    }

    /* TODO: implement. */
    return -1;
}
