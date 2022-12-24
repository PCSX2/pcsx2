/**
* @file include/demangler/stgrammars/borlandll.h
* @brief Internal LL grammar for demangler.
* @copyright (c) 2017 Avast Software, licensed under the MIT license
*/

#ifndef DEMANGLER_STGRAMMARS_BORLANDLL_H
#define DEMANGLER_STGRAMMARS_BORLANDLL_H

#include "demangler/gparser.h"

namespace demangler {

class cIgram_borlandll {
public:
	static unsigned char terminal_static[256];
	static cGram::llelem_t llst[280][69];
	static cGram::ruleaddr_t ruleaddrs[467];
	static cGram::gelem_t ruleelements[603];
	static cGram::gelem_t root;
	cGram::igram_t getInternalGrammar();
};

} // namespace demangler
#endif
