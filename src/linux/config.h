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
#ifndef CONFIG_H__
#define CONFIG_H__

#define HELP_LEN_MAX        256U
#define PATH_LEN_MAX        256U
#define REQUEST_LEN_MAX     (256U + 32U) /* Must always be max of HELP/PATH + some more. */
#define RESPONSE_LEN_MAX    (256U + 32U) /* Must always be max of HELP/PATH + some more. */

#define FILE_READ_BUF_SIZE  8192U

#define PASV_PORT_NUMBER    40000U

#define LOG_FILE_PATH       ".."

#endif /* CONFIG_H__ */
