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

#ifndef VERSION_H__
#define VERSION_H__

#define VERSION_MAJOR       0u
#define VERSION_MINOR       1u
#define VERSION_PATCH       0u

#define VERSION_STRING_LEN  12 /* mjr.mnr.pat + \0 */

/*!
 * \brief Return the current version.
 * \details
 *      Semantic versioning 2.0.0 as per <https://semver.org/>.
 *      Each part of the version number (major, minor, patch) shall be limited to a value no more
 *      than 255.
 * \returns
 */
static inline const char* GetVersionString(void)
{
    static char version[VERSION_STRING_LEN];

    (void)sprintf(version, "%u.%u.%u", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);

    return version;
}

#endif /* VERSION_H__ */
