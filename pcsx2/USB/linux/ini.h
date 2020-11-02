/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef INI_H
#define INI_H


// #ifndef __LINUX__
// #ifdef __linux__
// #define __LINUX__
// #endif /* __linux__ */
// #endif /* No __LINUX__ */

// #define CDVDdefs


// File format:
// [section]
// keyword=value

// file - Name of the INI file
// section - Section within the file
// keyword - Identifier for a value
// value - value to store with a keyword in a section in the file
// buffer - place to retrieve the value of a keyword

// return values: 0 = success, -1 = failure


// #define VERBOSE_FUNCTION_INI

#define INIMAXLEN 255

#if __cplusplus
extern "C" {
#endif
extern int INISaveString(const char* file, const char* section, const char* keyword, const char* value);
extern int INILoadString(const char* file, const char* section, const char* keyword, char* buffer);

extern int INISaveUInt(const char* file, const char* section, const char* keyword, unsigned int value);
extern int INILoadUInt(const char* file, const char* section, const char* keyword, unsigned int* buffer);

// NULL in the keyword below removes the whole section.
extern int INIRemove(const char* file, const char* section, const char* keyword);

#if __cplusplus
}
#endif
#endif /* INI_H */
