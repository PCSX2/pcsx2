/**
* @file include/demangler/stgrammars/gccll.h
* @brief Internal LL grammar for demangler.
* @copyright (c) 2017 Avast Software, licensed under the MIT license
*/

#ifndef DEMANGLER_STGRAMMARS_GCCLL_H
#define DEMANGLER_STGRAMMARS_GCCLL_H

#include "demangler/gparser.h"

namespace demangler {

class cIgram_gccll {
public:
	static unsigned char terminal_static[256];
	static cGram::llelem_t llst[254][64];
	static cGram::ruleaddr_t ruleaddrs[423];
	static cGram::gelem_t ruleelements[445];
	static cGram::gelem_t root;
	cGram::igram_t getInternalGrammar();
};

} // namespace demangler
#endif
