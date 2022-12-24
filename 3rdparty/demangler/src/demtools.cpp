/**
 * @file src/demangler/demtools.cpp
 * @brief Tools and extra functions for demangler.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#include <ctime>
#include <fstream>
#include <sstream>
#include <string>

#include "demangler/demtools.h"

using namespace std;

namespace demangler {

/**
 * @brief Function which finds out whether a file exists.
 * @param filename Name of the file to be checked.
 * @return Boolean value determining whether the file exists or not.
 */
bool fileExists(const std::string &filename) {
	ifstream ifile(filename);
	return ifile.is_open();
}

/**
 * @brief Initializes a sdate_t to default values.
 * @param x sdate_t to be initialized..
 */
void initSdate_t(sdate_t &x) {
	x.y = 0;
	x.m = 0;
	x.d = 0;
	x.h = 0;
	x.min = 0;
	x.s = 0;
}

/**
 * @brief Get stuct with current date and time.
 * @return Struct with current date and time.
 */
sdate_t genTimeStruct() {
	sdate_t retvalue;
	initSdate_t(retvalue);
	time_t t = time(nullptr); // get time now
	struct tm * now = localtime( & t );
	//year
	retvalue.y = now->tm_year + 1900;
	//month
	retvalue.m = (now->tm_mon + 1);
	//day
	retvalue.d = now->tm_mday;
	//hour
	retvalue.h = now->tm_hour;
	//minute
	retvalue.min = now->tm_min;
	//second
	retvalue.s = now->tm_sec;
	return retvalue;
}

/**
 * @brief Replaces strings "tobereplaced" in source with "replacement".
 * @param source Source string.
 * @param tobereplaced Substring which will be searched for and all of its instances will be replaced.
 * @param replacement The replacement string.
 */
void xreplace(string &source, const string &tobereplaced, const string &replacement) {
	std::size_t lastfound = 0;
	if (tobereplaced != "") {
		while (source.find(tobereplaced,lastfound) != source.npos) {
			lastfound = source.find(tobereplaced,lastfound);
			source.replace(source.find(tobereplaced,lastfound),tobereplaced.length(),replacement);
			lastfound += replacement.length();
		}
	}
}

} // namespace demangler
