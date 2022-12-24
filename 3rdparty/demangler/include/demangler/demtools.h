/**
 * @file include/demangler/demtools.h
 * @brief Tools and extra functions for demangler.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#ifndef DEMANGLER_DEMTOOLS_H
#define DEMANGLER_DEMTOOLS_H

#include <string>

namespace demangler {

/**
 * @brief Structure for date and time.
 */
struct sdate_t {
	unsigned int y = 0;
	unsigned int m = 0;
	unsigned int d = 0;
	unsigned int h = 0;
	unsigned int min = 0;
	unsigned int s = 0;
};

bool fileExists(const std::string &filename);

void initSdate_t(sdate_t &x);

sdate_t genTimeStruct();

void xreplace(std::string &source, const std::string &tobereplaced, const std::string &replacement);

} // namespace demangler

#endif
