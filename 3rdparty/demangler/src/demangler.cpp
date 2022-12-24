/**
 * @file src/demangler/demangler.cpp
 * @brief Demangler library.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */


#include <iostream>
#include <string>

#include "demangler/demangler.h"

namespace demangler {

/**
 * @brief Constructor of CDemangler class.
 * @param gname Grammar name. If internal grammar is used, internal grammar of this name must exist.
 * If external grammar is used file with this name must exist and it used as external grammar.
 * @param i Use internal grammar? Default setting is true. If set to false, external grammar is used.
 */
CDemangler::CDemangler(std::string gname, bool i):
		pGram(new cGram()),
		pName(nullptr),
		errState(cGram::ERROR_OK) {
	//if gname is empty, pGram will not be initialized (may be used for creating new internal grammars)
	if (!gname.empty()) {
		errState = pGram->initialize(gname, i);
		compiler = gname;
	}
}

std::unique_ptr<CDemangler> CDemangler::createGcc(bool i)
{
	return std::make_unique<CDemangler>("gcc", i);
}

std::unique_ptr<CDemangler> CDemangler::createMs(bool i)
{
	return std::make_unique<CDemangler>("ms", i);
}

std::unique_ptr<CDemangler> CDemangler::createBorland(bool i)
{
	return std::make_unique<CDemangler>("borland", i);
}

/**
 * @brief Destructor of CDemangler class.
 */
CDemangler::~CDemangler() {
	delete pGram;
}

/**
 * @brief Check if the error state of demangler is ok. Returns false if an error has ocurred during the last action.
 * @return Boolean value determining whether everything is ok.
 */
bool CDemangler::isOk() {
	if (errState == cGram::ERROR_OK) {
		return true;
	}
	else {
		return false;
	}
}

/**
 * @brief Returns string describing the last error.
 */
std::string CDemangler::printError() {
	if (pGram != nullptr) {
		return pGram->errString;
	}
	else {
		return "No grammar class allocated. Cannot get error state.";
	}
}

/**
 * @brief Reset error state.
 */
void CDemangler::resetError() {
	if (pGram != nullptr) {
		pGram->resetError();
		errState = cGram::ERROR_OK;
	}
}

/**
 * @brief Function which converts external grammar into internal grammar.
 * After using this function the demangler object must not be used for demangling.S
 * errState may be set if an error occurs.
 * @param inputfilename The name of the file which contains grammar rules.
 * @param outputname The name of the output grammar.
 */
void CDemangler::createGrammar(std::string inputfilename, std::string outputname) {
	errState = pGram->generateIgrammar(inputfilename, outputname);
}

/**
 * @brief Demangle the input string and return the demangled name class. errState may be set if an error occurs.
 * @param inputName The name to be demangled.
 * @return Pointer to a cName object containing all info anout the demangled name.
 */
cName *CDemangler::demangleToClass(std::string inputName) {
	return pGram->perform(inputName,&errState);
}

/**
 * @brief Demangle the input string and return the demangled name as a string. errState may be set if an error occurs.
 * @param inputName The name to be demangled.
 * @return String containing the declaration of the demangled name.
 */
std::string CDemangler::demangleToString(std::string inputName) {
	std::string retvalue;
	resetError();
	pName = pGram->perform(inputName,&errState);
	retvalue = pName->printall(compiler);
	delete pName;
	return retvalue;
}

/**
 * @brief Set substitution analysis manually to enabled or disabled.
 * @param x Boolean value. True means enable, false means disable.
 */
void CDemangler::setSubAnalyze(bool x) {
	pGram->setSubAnalyze(x);
}

} // namespace demangler
