/**
* @file include/demangler/stgrammars/msll.h
* @brief Internal LL grammar for demangler.
* @copyright (c) 2017 Avast Software, licensed under the MIT license
*/

#ifndef DEMANGLER_STGRAMMARS_MSLL_H
#define DEMANGLER_STGRAMMARS_MSLL_H

#include "demangler/gparser.h"

namespace demangler {

class cIgram_msll {
public:
	static unsigned char terminal_static[256];
	static cGram::llelem_t llst[249][68];
	static cGram::ruleaddr_t ruleaddrs[534];
	static cGram::gelem_t ruleelements[796];
	static cGram::gelem_t root;
	cGram::igram_t getInternalGrammar();
};

} // namespace demangler

#endif
