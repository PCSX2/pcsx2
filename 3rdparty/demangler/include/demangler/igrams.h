/**
 * @file include/demangler/igrams.h
 * @brief Internal grammar list.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#ifndef DEMANGLER_IGRAMS_H
#define DEMANGLER_IGRAMS_H

//[igram] add internal grammar headers here
#include "demangler/stgrammars/borlandll.h"
#include "demangler/stgrammars/gccll.h"
#include "demangler/stgrammars/msll.h"

namespace demangler {

bool initIgram(std::string gname, cGram* gParser);

void deleteIgrams(cGram* gParser);

} // namespace demangler

#endif
