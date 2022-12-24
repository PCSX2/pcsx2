/**
 * @file include/demangler/demglobal.h
 * @brief Global variables in demangler namespace.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#ifndef DEMANGLER_DEMGLOBAL_H
#define DEMANGLER_DEMGLOBAL_H

#include "demangler/igrams.h"

namespace demangler {

extern cGram::igram_t internalGrammarStruct;
extern cIgram_msll* igram_msll;
extern cIgram_gccll* igram_gccll;

} // namespace demangler

#endif
