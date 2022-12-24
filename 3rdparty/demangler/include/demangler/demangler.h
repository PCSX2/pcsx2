/**
 * @file include/demangler/demangler.h
 * @brief Demangler library.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#ifndef DEMANGLER_DEMANGLERL_H
#define DEMANGLER_DEMANGLERL_H

#include <memory>
#include <string>

#include "demangler/gparser.h"

namespace demangler {

/**
 * The grammar parser class - the core of the demangler.
 */
class CDemangler {
	cGram *pGram;
	cName *pName;
	std::string compiler = "gcc";
	cGram::errcode errState; /// error state; 0 = everyting is ok

public:
	CDemangler(std::string gname, bool i = true);
	static std::unique_ptr<CDemangler> createGcc(bool i = true);
	static std::unique_ptr<CDemangler> createMs(bool i = true);
	static std::unique_ptr<CDemangler> createBorland(bool i = true);
	virtual ~CDemangler();

	bool isOk();
	std::string printError();
	void resetError();

	void createGrammar(std::string inputfilename, std::string outputname);
	cName *demangleToClass(std::string inputName);
	std::string demangleToString(std::string inputName);
	void setSubAnalyze(bool x);
};

} // namespace demangler

#endif
