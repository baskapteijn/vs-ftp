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

/*!
 * \brief Handle the next server iteration.
 * \details
 *      - Check for a (newly) received command
 *
 *      And optionally:
 *
 *      - Handle a received command
 *      - Continue a transmission part
 *
 *      This function shall return swiftly as to allow the user to do tasks in between.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
int VSFTPServerHandle(void)
{
    /* TODO: implement. */
    return -1;
}

/*!
 * \brief Initialize the VS-FTP server.
 * \details
 *      Initializes the Server with data coming in through the vsftpData structure.
 * \param vsftpData
 *      A pointer to the VS-FTP configuration data.
 * \returns 0 in case of successful completion or any other value in case of an error.
 */
int VSFTPServerInitialize(VSFTPData_s *vsftpData)
{
    /* TODO: implement. */
    return -1;
}
