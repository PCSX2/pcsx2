/**
 * @file src/demangler/igrams.cpp
 * @brief Internal grammar list.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#include <cstdlib>

#include "demangler/demglobal.h"
#include "demangler/igrams.h"

using namespace std;

namespace demangler {

/**
 * @brief Classes containing internal grammar.
 */
cIgram_msll* igram_msll = nullptr; //Microsoft Visual C++
cIgram_gccll* igram_gccll = nullptr; //GCC
cIgram_borlandll* igram_borlandll = nullptr; //Borland

//[igram] add pointers to internal grammars here

/**
 * @brief Function which allocates an internal grammar class and sets the internal grammar structure.
 * @param gname Grammar name. The particular internal grammar is selected using this name.
 * @param gParser Pointer to a cGram to send pointers to newly allocated grammar to.
 * @return Was the initialisation successful?
 * @retval false Grammar with the specified name was not found.
 */
bool initIgram(string gname, cGram* gParser) {
	bool retvalue = false;

	//Microsoft Visual C++ (msll)
	if (gname == "ms") {
		igram_msll = new cIgram_msll;
		gParser->internalGrammarStruct = igram_msll->getInternalGrammar();
		return true;
	}
	//GCC (gccll)
	else if (gname == "gcc") {
		igram_gccll = new cIgram_gccll;
		gParser->internalGrammarStruct = igram_gccll->getInternalGrammar();
		return true;
	}
	//Borland (borlandll)
	else if (gname == "borland") {
		igram_borlandll = new cIgram_borlandll;
		gParser->internalGrammarStruct = igram_borlandll->getInternalGrammar();
		return true;
	}

	//[igram] add allocation of internal grammars here

	return retvalue;
}

/**
 * @brief Function which deallocates all used internal grammar classes.
 * @param gParser Pointer to a cGram to clean internal grammars from.
 */
void deleteIgrams(cGram* gParser) {

	//first, delete the dynamically allocated internal llst if there is any
	if (gParser->internalGrammarStruct.llst != nullptr) {
		free(gParser->internalGrammarStruct.llst);
		gParser->internalGrammarStruct.llst = nullptr;
	}

	//dealocate all internal grammars here...
	if (igram_msll != nullptr) {
		delete igram_msll;
		igram_msll = nullptr;
	}

	if (igram_gccll != nullptr) {
		delete igram_gccll;
		igram_gccll = nullptr;
	}

	if (igram_borlandll != nullptr) {
		delete igram_borlandll;
		igram_borlandll = nullptr;
	}

	//[igram] add deallocation of internal grammars here

}

} // namespace demangler
