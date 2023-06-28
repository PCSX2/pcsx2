/**
 * @file src/demangler/gparser.cpp
 * @brief Parser of LL grammar.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

#include "demangler/demglobal.h"
#include "demangler/demtools.h"
#include "demangler/gparser.h"
#include "demangler/igrams.h"

// #define DEMANGLER_SUBDBG
// #define DEMANGLER_IGRAM

#define BSUBMAX 40 //max size of parameter substitution vector

using namespace std;

namespace demangler {

/**
 * @brief Global array of semantic action names. Used when building internal LL table from external grammar.
 */
const char *cGram::semactname[] = {
	"SA_NULL",
	"SA_SETNAMEF",
	"SA_SETNAMETF",
	"SA_SETNAMEO",
	"SA_SETNAMEC",
	"SA_SETNAMED",
	"SA_SETNAMEX",
	"SA_SETNAMEVT",
	"SA_SETOPXX",
	"SA_SETOPNW",
	"SA_SETOPNA",
	"SA_SETOPDL",
	"SA_SETOPDA",
	"SA_SETOPPS",
	"SA_SETOPNG",
	"SA_SETOPAD",
	"SA_SETOPDE",
	"SA_SETOPCO",
	"SA_SETOPPL",
	"SA_SETOPMI",
	"SA_SETOPML",
	"SA_SETOPDV",
	"SA_SETOPRM",
	"SA_SETOPAN",
	"SA_SETOPOR",
	"SA_SETOPEO",
	"SA_SETOPASS",
	"SA_SETOPPLL",
	"SA_SETOPMII",
	"SA_SETOPMLL",
	"SA_SETOPDVV",
	"SA_SETOPRMM",
	"SA_SETOPANN",
	"SA_SETOPORR",
	"SA_SETOPEOO",
	"SA_SETOPLS",
	"SA_SETOPRS",
	"SA_SETOPLSS",
	"SA_SETOPRSS",
	"SA_SETOPEQ",
	"SA_SETOPNE",
	"SA_SETOPLT",
	"SA_SETOPGT",
	"SA_SETOPLE",
	"SA_SETOPGE",
	"SA_SETOPNT",
	"SA_SETOPAA",
	"SA_SETOPOO",
	"SA_SETOPPP",
	"SA_SETOPMM",
	"SA_SETOPCM",
	"SA_SETOPPM",
	"SA_SETOPPT",
	"SA_SETOPCL",
	"SA_SETOPIX",
	"SA_SETOPQU",
	"SA_SETOPST",
	"SA_SETOPSZ",
	"SA_SETOPAT",
	"SA_SETOPAZ",
	"SA_SETOPCV",
	"SA_SETTYPEV",
	"SA_SETTYPEW",
	"SA_SETTYPEB",
	"SA_SETTYPEC",
	"SA_SETTYPEA",
	"SA_SETTYPEH",
	"SA_SETTYPES",
	"SA_SETTYPET",
	"SA_SETTYPEI",
	"SA_SETTYPEJ",
	"SA_SETTYPEL",
	"SA_SETTYPEM",
	"SA_SETTYPEX",
	"SA_SETTYPEY",
	"SA_SETTYPEN",
	"SA_SETTYPEO",
	"SA_SETTYPEF",
	"SA_SETTYPED",
	"SA_SETTYPEE",
	"SA_SETTYPEG",
	"SA_SETTYPEZ",
	"SA_SETCONST",
	"SA_SETRESTRICT",
	"SA_SETVOLATILE",
	"SA_SETPTR",
	"SA_SETREF",
	"SA_SETRVAL",
	"SA_SETCPAIR",
	"SA_SETIM",
	"SA_SUBSTD", //std::
	"SA_SUBALC", //std::allocator
	"SA_SUBSTR", //std::basic_string
	"SA_SUBSTRS", //std::basic_string<char",std::char_traits<char>",std::allocator<char>>
	"SA_SUBISTR", //std::basic_istream<char",  std::char_traits<char>>
	"SA_SUBOSTR", //std::basic_ostream<char",  std::char_traits<char>>
	"SA_SUBIOSTR", //std::basic_iostream<char",  std::char_traits<char>>
	"SA_LOADID", //load an unqualified name into the qualified vector of names
	"SA_LOADSUB", //load a substitution
	"SA_LOADTSUB", //load a template sub
	"SA_LOADARR", //load an array dimension
	"SA_STOREPAR", //store current parameter to vector of parameters
	"SA_STORETEMPARG", //store current parameter to current vector of template arguments
	"SA_STORETEMPLATE", //store the whole template into the last name element of last name vector
	"SA_BEGINTEMPL", //begin a template
	"SA_SKIPTEMPL", //skip a template
	"SA_PAR2F", //store current vector of parameters into the function
	"SA_PAR2RET", //store current parameter to the return value
	"SA_PAR2SPEC", //store current parameter to the special value
	"SA_UNQ2F", //future identifiers are added to the function name
	"SA_UNQ2P", //function identifiers are added to parameter name
	"SA_SSNEST", //nested sub
	"SA_STUNQ", //unqualified std:: sub
	"SA_SSNO", //other sub derived from <name>
	"SA_TYPE2EXPR", //builtin type is converted to primary expression
	"SA_EXPRVAL", //load expression value
	"SA_BEGINPEXPR", //begin a primary expression
	"SA_STOREPEXPR", //end a primary expression
	"SA_COPYTERM", //copy the terminal on the input into current_name in substitution analyzer
	"SA_ADDCHARTONAME",
	"SA_STORENAME",
	"SA_REVERSENAME",
	"SA_SETPRIVATE",
	"SA_SETPUBLIC",
	"SA_SETPROTECTED",
	"SA_SETFCDECL",
	"SA_SETFPASCAL",
	"SA_SETFFORTRAN",
	"SA_SETFTHISCALL",
	"SA_SETFSTDCALL",
	"SA_SETFFASTCALL",
	"SA_SETFINTERRUPT",
	"SA_SETUNION",
	"SA_SETSTRUCT",
	"SA_SETCLASS",
	"SA_SETENUM",
	"SA_SETSTATIC",
	"SA_SETVIRTUAL",
	"SA_STCLCONST",
	"SA_STCLVOL",
	"SA_STCLFAR",
	"SA_STCLHUGE",
	"SA_SAVENAMESUB",
	"SA_LOADNAMESUB",
	"SA_MSTEMPLPSUB",
	"SA_SETNAMER0",
	"SA_SETNAMER1",
	"SA_SETNAMER2",
	"SA_SETNAMER3",
	"SA_SETNAMER4",
	"SA_SETNAME_A",
	"SA_SETNAME_B",
	"SA_SETNAME_C",
	"SA_SETNAME_D",
	"SA_SETNAME_E",
	"SA_SETNAME_F",
	"SA_SETNAME_G",
	"SA_SETNAME_H",
	"SA_SETNAME_I",
	"SA_SETNAME_J",
	"SA_SETNAME_K",
	"SA_SETNAME_L",
	"SA_SETNAME_M",
	"SA_SETNAME_N",
	"SA_SETNAME_O",
	"SA_SETNAME_P",
	"SA_SETNAME_Q",
	"SA_SETNAME_R",
	"SA_SETNAME_S",
	"SA_SETNAME_T",
	"SA_SETNAME_U",
	"SA_SETNAME_V",
	"SA_SETNAME_W",
	"SA_SETNAME_X",
	"SA_SETNAME_Y",
	"SA_SETNAME_Z",
	"SA_TEMPL2TFTPL",
	"SA_BEGINBSUB",
	"SA_LOADBSUB",
	"SA_ADDMCONST",
	"SA_ADDMVOL",
	"SA_ADDMFAR",
	"SA_ADDMHUGE",
	"SA_LOADMSNUM",
	"SA_NUMTORTTIBCD",
	"SA_NUMTOTYPE",
	"SA_BORLANDNORMALIZEPARNAME", //normalize the last current_name element by replacing "@" with "::". Usef dor Borland demangler
	"SA_BORLANDID", //insert separator "|" after a number of characters specified by following characters (numbers). Borland only
	"SA_LOADBORLANDSUB", //load borland sub (based on current char - numbering 1 - 9, a - z)
	"SA_BORLANDARR", //load an array dimension (Borland only)
	"SA_END"
};

/**
 * @brief Constructor of cGram class.
 */
cGram::cGram() {
	internalGrammarStruct.llst = nullptr; //nullify the llst
	SubAnalyzeEnabled = false;
	errValid = false;
	errString = "Everything OK";
	lex_position = 0;
}

/**
 * @brief Destructor of cGram class.
 */
cGram::~cGram() {

	//delete the used internal grammar, if any way used
	deleteIgrams(this);

	//deallocate parsed external grammar
	if (!internalGrammar) {
		for(vector<rule_t>::iterator i=rules.begin(); i != rules.end(); ++i) {
			free(i->left.nt);
				//for every element on the right side
				for(vector<gelem_t>::iterator j=i->right.begin(); j != i->right.end(); ++j) {
					if (j->type == GE_NONTERM) {
						free(j->nt);
					}
				} //for every element on the right
		}
	} //if (!internalGrammar)
}

/**
 * @brief Initialize the grammar in cGram class.
 * @param gname Grammar name. If internal grammar is used, internal grammar of this name must exist.
 * If external grammar is used file with this name must exist and it used as external grammar.
 * @param i Use internal grammar? Default setting is true. If set to false, external grammar is used.
 */
cGram::errcode cGram::initialize(string gname, bool i) {
	errcode retvalue = ERROR_OK;
	internalGrammar = i;
	compiler = gname;
	//initialize internal grammar
	if (internalGrammar) {

		//this is only for gcc, the special substitution analysis
		if (gname == "gcc") {
			SubAnalyzeEnabled = true;
		}

		//initialize the internal grammar
		if (!initIgram(gname,this)) {
			errValid = false;
			errString = "Failed to load internal grammar " + gname + ".";
			return ERROR_GRAM;
		}
		//successfully initialised internal grammar
		else {
			errValid = true;
		}
	}

	//initialize an external grammar
	else {
		//parse external grammar file
		if ((retvalue = parse(gname)) == ERROR_OK) {
			errValid = true;
		}
	}
	return retvalue;
}

/**
 * @brief Set substitution analysis manually to enabled or disabled.
 * @param x Boolean value. True means enable, false means disable.
 */
void cGram::setSubAnalyze(bool x) {
	SubAnalyzeEnabled = x;
}

/**
 * @brief Constructor of cName class. Initializes return and special type.
 */
cName::cName() {
	return_type.type = TT_UNKNOWN;
	special_type.type = TT_UNKNOWN;
	member_function_access = MFM_NULL;
	function_call = FCC_NULL;
	is_static = false;
	is_virtual = false;
	tf_tpl = nullptr;
}

/**
 * @brief Initializer of the static const array of final states of the input file parsing FSM. Terminated by S_NULL.
 */
const cGram::fsmstate cGram::fsm_final[] = {S_START,S_IGNORE,S_RIGHT,S_T,S_NULL};

/**
 * @brief Definition of the EOF terminal element.
 */
const cGram::gelem_t cGram::T_EOF = {GE_TERM, const_cast<char *>(""), 0, '\0'};

/**
 * @brief Initializer of error message array.
 */
const char *cGram::errmsg[] = {
	"Everything is ok. However, if you can see this error message something has gone terribly wrong.",
	"File error,",
	"Grammar input invalid.",
	"Syntax error.",
	"Unknown error."
};

/**
 * @brief An envelop function to load a file into the "source" string of cGram class.
 * @param filename Name of the file to be loaded.
 * @return An error code.
 * @retval ERROR_FILE File loading failed.
 */
cGram::errcode cGram::loadfile(const string filename) {
	errcode retvalue = ERROR_OK;
	source = new fstream(filename,fstream::in);
	return retvalue;
}

/**
 * @brief An envelop function to retrieve one byte from the input source.
 * @return One byte of the input file.
 */
char cGram::getc() {
	char c;
	source->get(c);
	return c;
}

/**
 * @brief Bool function which tests whether end of source file has been reached.
 * @return "Has the pointer reached end of source file?"
 */
bool cGram::eof() {
	return (source->peek() == EOF);
}

/**
 * @brief Bool function which tests whether end of line in the source file has been reached.
 * @return "Has the pointer reached end of line?"
 */
bool cGram::lf() {
	return (static_cast<char>(source->peek()) == '\n' || static_cast<char>(source->peek()) == '\r');
}

/**
 * @brief Function which tests whether a state is a final state of FSM.
 * @param s The input state.
 * @return "Is s a final state of the FSM?"
 */
bool cGram::is_final(fsmstate s) {
	bool retvalue = false;
	//for every state in array check whether it equals input parameter s
	for (int i=0; fsm_final[i] != S_NULL; ++i) {
		if (s == fsm_final[i]) {
			return true;
		}
	}
	return retvalue;
}

/**
 * @brief The FSM which parses the input file into a vector of grammar rules.
 * @param filename Name of the file from which the grammar should be read.
 * @return An errorcode.
 * @retval ERROR_FILE An error ocurred when trying to load the file.
 * @retval ERROR_FSM Error caused by invalid data in the input file.
 */
cGram::errcode cGram::getgrammar(const string filename) {
	errcode retvalue = ERROR_OK;
	fsmstate state = S_START;
	fsmstate nextstate = S_START;
	string current_nt;
	/*
	 * Variables for internal grammar generator
	 */
	std::size_t current_offset = 0;
	newIG_ruleaddrs = "";
	newIG_ruleelements = "\t";
	/*
	 * End of block "Variables for internal grammar generator"
	 */

	//load the file
	if ((retvalue = loadfile(filename)) != ERROR_OK) {return retvalue;}

	rule_t current_rule;
	gelem_t current_gelem;

	current_gelem.nt = nullptr;

	//rule numbering starts at 1
	unsigned int rulenum = 1;


	while (retvalue == ERROR_OK) {
		//if reached end of line or file, check whether current state is final
		if (eof() || lf()) {
			//current state is not final, end with error
			if (!is_final(nextstate)) {
				errString = "cGram::parse: Unexpected end of line/file.";
				retvalue = ERROR_FSM;
				break;
			}
			//state is final, save current rule
			else {
				//when generating new internal grammar
				if (createIGrammar != "") {
					//add new nonterminal into nonterminal list
					if (isnt(nonterminals,current_rule.left.nt) == 0) {
						nonterminals.push_back(current_rule.left.nt);
					}
					//assign the ntst number (starting from 0)
					current_rule.left.ntst = isnt(nonterminals,current_rule.left.nt)-1;
					//for every non-terminal on the right side of the rule, do the same
					for(vector<gelem_t>::iterator j=current_rule.right.begin(); j != current_rule.right.end(); ++j) {
						if (j->type == GE_NONTERM) {
							if (isnt(nonterminals,j->nt) == 0) {
								nonterminals.push_back(j->nt);
							}
							j->ntst = isnt(nonterminals,j->nt)-1;
						}
					}

					//create the rules table
					if (rulenum != 1 && !current_rule.right.empty()) {
						newIG_ruleelements += string("") + "," + "\n\t";
					}
					//for every element on the right side of the rule
					for(vector<gelem_t>::iterator j=current_rule.right.begin(); j != current_rule.right.end(); ++j) {
						if (j!=current_rule.right.begin()) {
							newIG_ruleelements += ",";
						}
						newIG_ruleelements += "{";
						if (j->type == GE_NONTERM) {
							newIG_ruleelements += "cGram::GE_NONTERM,const_cast<char *>(\"";
							newIG_ruleelements += j->nt;
							newIG_ruleelements += string("") + "\")," + std::to_string(j->ntst) + ",\'\\0\'";
						}
						else if (j->type == GE_TERM) {
							newIG_ruleelements += string("") + "cGram::GE_TERM,const_cast<char *>(\"\"),0,\'" + j->t + "\'";
						}
						newIG_ruleelements += "}";
					}

					//rules offset table
					if (rulenum != 1) {
						newIG_ruleaddrs += ",";
					}
					if ((rulenum - 1) % 10 == 0) {
						newIG_ruleaddrs += "\n\t";
					}
					newIG_ruleaddrs += string("") + "{" + std::to_string(current_offset) + "," + std::to_string(static_cast<unsigned int>(current_rule.right.size())) + "}";
					current_offset += current_rule.right.size();

				}

				current_rule.n=rulenum++;
				rules.push_back(current_rule);
				current_rule.left.nt = nullptr;
				current_rule.right.clear();
				nextstate = S_START;
			}
		}
		//if FSM reached end of file, exit the main cycle
		if (eof() || (retvalue != ERROR_OK)) {
			break;
		}
		char c = getc();
		state=nextstate;
		switch(state) {
			//beginning of a line
			case S_START:
				//ignore whitespaces
				if (isspace(c)) {
					nextstate = S_START;
				}
				else {
					switch(c) {
						//left non-terminal begins
						case '<':
							current_gelem.type = GE_NONTERM;
							nextstate = S_NT_LEFT;
							break;
						//start of a comment - ignore rest of the line
						case '#':
							nextstate = S_IGNORE;
							break;
						//anything else = error
						default:
							nextstate = S_ERROR;
							break;
					}
				}
				break;
			//left non-terminal
			case S_NT_LEFT:
				switch(c) {
					//end the non.terminal and save it to the rule struct
					case '>':
						current_gelem.nt = static_cast<char *>(malloc(sizeof(char)*(current_nt.length()+1)));
						if (current_gelem.nt == nullptr) {
							errString = "cGram::getgrammar: Memory allocation error.";
							retvalue = ERROR_MEM;
							break;
						}
						strcpy(current_gelem.nt,current_nt.c_str());
						current_rule.left = current_gelem;
						current_nt.clear();
						current_gelem.nt = nullptr;
						nextstate = S_OP1;
						break;
					//anything else is part of the non-terminal's name
					default:
						current_nt += c;
						nextstate = S_NT_LEFT;
						break;
				}
				break;
			//first symbol of "::=" here
			case S_OP1:
				if (isspace(c)) {
					nextstate = S_OP1;
				}
				else {
					switch(c) {
						case ':':
							nextstate = S_OP2;
							break;
						default:
							nextstate = S_ERROR;
							break;
					}
				}
				break;
			//second symbol of "::=" here
			case S_OP2:
				switch(c) {
					case ':':
						nextstate = S_OP3;
						break;
					default:
						nextstate = S_ERROR;
						break;
				}
				break;
			//third symbol of "::=" here
			case S_OP3:
				switch(c) {
					case '=':
						nextstate = S_RIGHT;
						break;
					default:
						nextstate = S_ERROR;
						break;
				}
				break;
			//right side of the rule - expect a terminal, quoted terminal, non-terminal or nothing
			case S_RIGHT:
				//ignore whitespaces
				if (isspace(c)) {
					nextstate = S_RIGHT;
				}
				else {
					switch(c) {
						//start of a non-terminal
						case '<':
							current_gelem.type = GE_NONTERM;
							nextstate = S_NT_RIGHT;
							break;
						//start of a quoted terminal
						case '"':
							nextstate = S_QT;
							break;
						//start of a comment - ignore rest of the line
						case '#':
							nextstate = S_IGNORE;
							break;
						//anything else is a one-byte terminal - save it to the right side of the rule
						default:
							current_gelem.type = GE_TERM;
							current_gelem.t=c;
							current_rule.right.push_back(current_gelem);
							nextstate = S_T;
							break;
					}
				}
				break;
			//non-terminal on the right side of the rule
			case S_NT_RIGHT:
				switch(c) {
					//end of non-terminal - save it
					case '>':
						current_gelem.nt = static_cast<char *>(malloc(sizeof(char)*(current_nt.length()+1)));
						if (current_gelem.nt == nullptr) {
							errString = "cGram::getgrammar: Memory allocation error.";
							retvalue = ERROR_MEM;
							break;
						}
						strcpy(current_gelem.nt,current_nt.c_str());

						current_rule.right.push_back(current_gelem);
						current_gelem.nt = nullptr;
						current_nt.clear();
						nextstate = S_RIGHT;
						break;
					//anything else is part of the non-terminal's name
					default:
						current_nt += c;
						nextstate = S_NT_RIGHT;
						break;
				}
				break;
			//terminal
			case S_T:
				//ignore whitespaces
				if (isspace(c)) {
					nextstate = S_RIGHT;
				}
				else {
					switch(c) {
						//a non-terminal starts
						case '<':
							current_gelem.type = GE_NONTERM;
							nextstate = S_NT_RIGHT;
							break;
						//start of a quoted terminal
						case '"':
							nextstate = S_QT;
							break;
						//anything else is another one-byte terminal - save it
						default:
							current_gelem.type = GE_TERM;
							current_gelem.t=c;
							current_rule.right.push_back(current_gelem);
							nextstate = S_T;
							break;
					}
				}
				break;
			//quoted terminal
			case S_QT:
				switch(c) {
					//end of quoted non-terminal
					case '"':
						nextstate = S_RIGHT;
						break;
					//an escape sequence
					case '\\':
						nextstate = S_QT_ESC;
						break;
					//anything else is a one-byte terminal - save it
					default:
						current_gelem.type = GE_TERM;
						current_gelem.t=c;
						current_rule.right.push_back(current_gelem);
						nextstate = S_QT;
						break;
				}
				break;
			//escape sequence
			case S_QT_ESC:
				//anything here is a one byte terminal - save it
				current_gelem.type = GE_TERM;
				current_gelem.t=c;
				current_rule.right.push_back(current_gelem);
				nextstate = S_QT;
				break;
			//ignore everything until end of line
			case S_IGNORE:
				nextstate = S_IGNORE;
				break;
			//error ocurred because of invalid data in input file
			case S_ERROR:
				errString = "cGram::parse: Syntax error in line " + std::to_string(rulenum);
				break;
			default:
				break;
		} //switch()state
	} //while

	//generate new internal grammar
	if (createIGrammar != "") {
		newIG_ruleaddrs += "\n";
		newIG_ruleelements_x = current_offset;
		newIG_ruleaddrs_x = rules.size();
	}

	return retvalue;
}

/**
 * @brief Function which generates the "Empty" set based on the parsed grammar rules.
 */
void cGram::genempty() {
	bool right_empty = true;
	bool change = false;
	empty.clear();
	//initialisation phase
	for(vector<rule_t>::iterator i=rules.begin(); i != rules.end(); ++i) {
		//if there is nothing on the right side, add left nonterminal to "empty"
		if (i->right.empty()) {
			empty[i->left.nt] = true;
		}
		else {
			empty[i->left.nt] = false;
		}
	}

	//do while there is still anything to change in "empty"
	do {
		change = false;
		//for every rule
		for(vector<rule_t>::iterator i=rules.begin(); i != rules.end(); ++i) {
			//if left non-terminal still is not true in "empty"
			if (!empty[i->left.nt]) {
				right_empty = true;
				//for every element on the right side of the rule check whether it is in "empty"
				for(vector<gelem_t>::iterator j=i->right.begin(); j != i->right.end(); ++j) {
					//terminal - can not be added to empty
					if (j->type == GE_TERM) {
						right_empty = false;
						break;
					}
					//non-terminal which is not in "empty"
					else if (!empty[j->nt]) {
						right_empty = false;
						break;
					}
				} //for every element on the right
				//if the right side of the rule could be emptied, add the left non-terminal to empty
				if (right_empty) {
					empty[i->left.nt] = true;
					change = true;
				}
			} //if left not in "empty"
		} //for every rule
	} while(change);

}

/**
 * @brief Function which generates the "First" set based on the parsed grammar rules and the "Empty" set.
 */
void cGram::genfirst() {
	bool change = false;
	first.clear();
	//do while there is still something to change
	do {
		change = false;
		//for every rule
		for(vector<rule_t>::iterator i=rules.begin(); i != rules.end(); ++i) {
				//for every element on the right side of the rule get its "First" set
				for(vector<gelem_t>::iterator j=i->right.begin(); j != i->right.end(); ++j) {
					//terminal - add it to the first of the left NT and stop parsing this rule
					if (j->type == GE_TERM) {
						if (first[i->left.nt].insert(*j).second) {
							change = true;
						}
						break;
					}
					//non-terminal
					else {
						if (j->nt != i->left.nt) {
							//add the contents of its "first" to the "first" of the left non-terminal
							if (copyset(first[i->left.nt],first[j->nt])) {
								change = true;
							}
						}
						//if the nonterminal is not in "empty", stop parsing this rule
						if (!empty[j->nt]) {
							break;
						}
					}
				} //for every element on the right
		} //for every rule

	} while(change);
}

/**
 * @brief Function which generates the "Empty" set for a sequence of grammar elements.
 * @param src Reference to a vector of grammar elements.
 * @return "May the sequence of grammar elements be emptied?"
 */
bool cGram::getempty(vector<gelem_t> & src) {
	bool retvalue = true;
	//empty sequence is empty :)
	if (src.empty()) {
		return true;
	}
	//for every element in the sequence...
	for(vector<gelem_t>::iterator i=src.begin(); (i != src.end()) && (retvalue == true); ++i) {
		//if there is a terminal, the sequence can not be emptied
		if (i->type == GE_TERM) {
			retvalue = false;
		}
		//for non-terminals, get the info about emptiability from the "Empty" set
		else {
			retvalue = empty[i->nt];
		}
	}
	return retvalue;
}

/**
 * @brief Function which generates the "First" set for a sequence of grammar elements.
 * @param src Reference to a vector of grammar elements.
 * @return A std::set of terminals that belong to the "First" set of the grammar element sequence.
 */
set<cGram::gelem_t,cGram::comparegelem_c> cGram::getfirst(vector<gelem_t> & src) {
	set<gelem_t,comparegelem_c> retvalue;
	//for every element of the sequence
	for(vector<gelem_t>::iterator i=src.begin(); i != src.end(); ++i) {
		//terminal - just insert into output set and end
		if (i->type == GE_TERM) {
			retvalue.insert(*i);
			break;
		}
		//nonterminal
		else {
			//copy the "First" set of the nonterminal into the output set
			copyset(retvalue,first[i->nt]);
			//if the non.terminal can not be emptied, end
			if (!empty[i->nt]) {
				break;
			}
		}
	}
	return retvalue;
}

/**
 * @brief Function which creates union of two sets by copying elements of the second set into the first set.
 * @param dst Reference to the destination std::set, into which elements will be copied.
 * @param src Referrence to the source std::set.
 * @return "Have any new elements been added to the destination set?"
 *
 */
bool cGram::copyset(set<cGram::gelem_t,cGram::comparegelem_c> & dst, set<cGram::gelem_t,cGram::comparegelem_c> & src) {
	bool retvalue = false;
	//for every element in the source set
	for(set<gelem_t,comparegelem_c>::iterator i = src.begin(); i != src.end(); ++i) {
		//insert it into the destination set and if it wasn't already present in the destination set...
		if (dst.insert(*i).second) {
			//set return value to true
			retvalue = true;
		}
	}
	return retvalue;
}

/**
 * @brief Function which makes a dynamically allocated copy of an existing template.
 * @param src The existing template (vector of types) to be copied.
 * @return A pointer to the copy of the source template.
 * @retval nullptr The source pointer was also nullptr or an error occurred.
 */
void * cGram::copynametpl(void * src) {
	void * retvalue = nullptr;
	bool all_ok = true;
	cName::type_t temp_type;
	void * temp_expr = nullptr;

	//if there is no source to copy from, quit
	if (src == nullptr) {
		return retvalue;
	}
	//allocate the destination vector
	retvalue = static_cast<void *>(new vector<cName::type_t>);
	if (retvalue == nullptr) {
		return retvalue;
	}
	//iterate through all types of the source vector
	for (vector<cName::type_t>::iterator i=(*(static_cast<vector<cName::type_t>*>(src))).begin();
										i != (*(static_cast<vector<cName::type_t>*>(src))).end(); ++i) {
		//dynamically allocate a copy of p-expression
		if (i->type == cName::TT_PEXPR) {
			temp_expr = malloc(sizeof(bool));
			if (temp_expr == nullptr) {
				all_ok = false;
				break;
			}
			temp_expr = static_cast<bool *>(i->value);
			temp_type = *i;
			temp_type.value = temp_expr;
			temp_expr = nullptr;
		}
		//if there is a template in the named type, recursively call this function
		else if (i->type == cName::TT_NAME) {
			temp_type = *i;
			for (vector<cName::name_t>::iterator j=temp_type.n.begin(); j != temp_type.n.end(); ++j) {
				if (j->tpl != nullptr) {
					j->tpl = copynametpl(j->tpl);
					//if there was an error during allocation, clean up
					if (j->tpl == nullptr) {
						all_ok = false;
						--j;
						do {
							if (j->tpl != nullptr) {
								delete static_cast<vector<cName::type_t>*>(j->tpl);
							}
						} while (j != temp_type.n.begin());
						break;
					}
				}
			}
			if (!all_ok) {
				break;
			}
		}
		//any other type
		else {
			temp_type = *i;
		}
		//push the whole type struct into destination vector
		(*(static_cast<vector<cName::type_t>*>(retvalue))).push_back(temp_type);
	}

	if (!all_ok) {
		delete static_cast<vector<cName::type_t>*>(retvalue);
		retvalue = nullptr;
	}
	return retvalue;
}

/**
 * @brief Function which generates the "Follow" set based on the parsed grammar rules, the "Empty" set and the "First" set.
 */
void cGram::genfollow() {
	bool change = false;
	follow.clear();
	//Y = the rest of the right element sequence
	vector<gelem_t> y;
	vector<gelem_t>::iterator k;
	//"First" of Y
	set<gelem_t,comparegelem_c> first_y;
	//set Follow of the root non-terminal to EOF
	follow[(*rules.begin()).left.nt].insert(T_EOF);
	do {
		change = false;
		//for every rule
		for(vector<rule_t>::iterator i=rules.begin(); i != rules.end(); ++i) {
			//for every element (J) on the right side...
			for(vector<gelem_t>::iterator j=i->right.begin(); j != i->right.end(); ++j) {
				//there is no point in getting Follow for terminals
				if (j->type == GE_NONTERM) {
					y.clear();
					k=j;
					//load Y from the rest of the right side of the rule
					for(++k; k != i->right.end(); ++k) {
						y.push_back(*k);
					}
					//get "First" for Y
					first_y = getfirst(y);
					//insert first(Y) into follow(J)
					if (copyset(follow[j->nt],first_y)) {
						change = true;
					}
					//get "Empty" for Y and if true, copy Follow of the left non-terminal to follow(Y)
					if (getempty(y) && copyset(follow[j->nt],follow[i->left.nt])) {
						change = true;
					}
				} //if non-terminal
			} //for every element on the right
		} //for every rule
	} while(change);
}

/**
 * @brief Function which generates the "Predict" set based on the parsed grammar rules,"Empty" set,"First" set and "Follow" set.
 */
void cGram::genpredict() {
	predict.clear();
	set<gelem_t,comparegelem_c> y;
	//for every rule
	for(vector<rule_t>::iterator i=rules.begin(); i != rules.end(); ++i) {
		y.clear();
		//get first for the element sequence on the right side of the rule and insert into Predict for this rule
		y = getfirst(i->right);
		copyset(predict[i->n],y);
		//if right side can be emptied, copy Follow of the left non-terminal into this rule's Predict
		if (getempty(i->right)) {
			copyset(predict[i->n],follow[i->left.nt]);
		}
	}
}

/**
 * @brief Function which generates the LL table from the "Predict" set.
 */
cGram::errcode cGram::genll() {
	errcode retvalue = ERROR_OK;
	ll.clear();
	pair<unsigned int,semact> current_pair;
	current_pair.second = SA_NULL;
	//for every rule
	for(vector<rule_t>::iterator i=rules.begin(); i != rules.end(); ++i) {
		//for every element in predict
		for(set<gelem_t,comparegelem_c>::iterator j = predict[i->n].begin(); j != predict[i->n].end(); ++j) {
			//if there already is a rule number on this position, the resulting table wouldn't be LL
			if ((ll[i->left.nt].find(j->t) != ll[i->left.nt].end())) {
				errString = string("genll: Duplicate entry at Nonterminal=") + i->left.nt + ", Terminal=" + j->t + ", NewRule=" + std::to_string(i->n) + ", OldRule=" + std::to_string(ll[i->left.nt][j->t].first);
				return ERROR_LL;
			}
			//insert the rule number into the table - line adressed by left non-terminal
			//column adressed by the current terminal in Predict
			current_pair.first = i->n;
			ll[i->left.nt][j->t] = current_pair;
		}
	}
	return retvalue;
}

unsigned int cGram::isnt(vector<string> & v_nonterminals, string nonterminal) {
	unsigned int retvalue = 0;
	unsigned int num = 0;
	for(vector<string>::iterator i=v_nonterminals.begin(); i != v_nonterminals.end(); ++i) {
		++num;
		if ((*i) == nonterminal) {
			retvalue = num;
			break;
		}
	}
	return retvalue;
}

unsigned int cGram::ist(vector<unsigned char> & v_terminals, unsigned char terminal) {
	unsigned int retvalue = 0;
	unsigned int num = 0;
	for(vector<unsigned char>::iterator i=v_terminals.begin(); i != v_terminals.end(); ++i) {
		++num;
		if ((*i) == terminal) {
			retvalue = num;
			break;
		}
	}
	return retvalue;
}

/**
 * @brief Function which generates the static code for an internal LL table.
 */
cGram::errcode cGram::genconstll() {
	errcode retvalue = ERROR_OK;
	ll.clear();
	pair<unsigned int,semact> current_pair;
	current_pair.second = SA_NULL;
	//for every rule
	for(vector<rule_t>::iterator i=rules.begin(); i != rules.end(); ++i) {
		//for every element in predict
		for(set<gelem_t,comparegelem_c>::iterator j = predict[i->n].begin(); j != predict[i->n].end(); ++j) {
			//if there already is a rule number on this position, the resulting table wouldn't be LL
			if (ll[i->left.nt].find(j->t) != ll[i->left.nt].end()) {
				errString = string("genconstll: Duplicate entry at Nonterminal=") + i->left.nt + ", Terminal=" + j->t + ", NewRule=" + std::to_string(i->n) + ", OldRule=" + std::to_string(ll[i->left.nt][j->t].first);
				return ERROR_LL;
			}
			if (ist(terminals,j->t) == 0) {
				terminals.push_back(j->t);
			}
			//insert the rule number into the table - line adressed by left non-terminal
			//column adressed by the current terminal in Predict
			current_pair.first = i->n;
			ll[i->left.nt][j->t] = current_pair;
		}
	}

	newIG_terminal_static_x = 256;
	newIG_terminal_static = "";
	//generate the terminal table - reduces the size of static LL table
	for (int i = 0; i < 256; ++i) {
		newIG_terminal_static += '\t';
		newIG_terminal_static += std::to_string(ist(terminals,static_cast<unsigned char>(i)));
		if (i != 255) {
			newIG_terminal_static += ",";
		}
		newIG_terminal_static += string("") + " // " + std::to_string(i);

		newIG_terminal_static += "\n";
	}

	//static LL table
	newIG_llst_x = nonterminals.size();
	newIG_llst_y = terminals.size()+1;
	newIG_llst = "";

	this->genllsem();
	//for every non-terminal (row)
	for (vector<string>::iterator i=nonterminals.begin(); i != nonterminals.end(); ++i) {
		if (i!=nonterminals.begin())
			newIG_llst += ",\n";
		newIG_llst += "\t{\n";
		//for every terminal (column)
		unsigned int llelCnt = 0;
		for (vector<unsigned char>::iterator j=terminals.begin(); j != terminals.end(); ++j) {
			if (j==terminals.begin()) {
				newIG_llst += "\t\t{0, cGram::SA_NULL}, ";
			}
			else {
				newIG_llst += ", ";
				if ((llelCnt + 1) % 4 == 0) {
					newIG_llst += "\n\t\t";
				}
			}
			newIG_llst += "{";
			newIG_llst += string("") + std::to_string(ll[*i][*j].first) + ", " + "cGram::" + semactname[ll[*i][*j].second];
			newIG_llst += "}";
			++llelCnt;
		}
		newIG_llst += "\n\t}";
	}
	return retvalue;
}

/**
 * @brief: Function which assigns semantic action for each cell of the LL table.
 */
void cGram::genllsem() {
	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-addchartoname"].begin(); i != ll["sem-addchartoname"].end(); ++i) {
		i->second.second = SA_ADDCHARTONAME;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-storename"].begin(); i != ll["sem-storename"].end(); ++i) {
		i->second.second = SA_STORENAME;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-loadid"].begin(); i != ll["sem-loadid"].end(); ++i) {
		i->second.second = SA_LOADID;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-loadarr"].begin(); i != ll["sem-loadarr"].end(); ++i) {
		i->second.second = SA_LOADARR;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-ssno"].begin(); i != ll["sem-ssno"].end(); ++i) {
		i->second.second = SA_SSNO;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-ssnest"].begin(); i != ll["sem-ssnest"].end(); ++i) {
		i->second.second = SA_SSNEST;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-stunq"].begin(); i != ll["sem-stunq"].end(); ++i) {
		i->second.second = SA_STUNQ;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-loadtsub"].begin(); i != ll["sem-loadtsub"].end(); ++i) {
		i->second.second = SA_LOADTSUB;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-type2expr"].begin(); i != ll["sem-type2expr"].end(); ++i) {
		i->second.second = SA_TYPE2EXPR;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-exprval"].begin(); i != ll["sem-exprval"].end(); ++i) {
		i->second.second = SA_EXPRVAL;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-beginexpr"].begin(); i != ll["sem-beginexpr"].end(); ++i) {
		i->second.second = SA_BEGINPEXPR;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-storeexpr"].begin(); i != ll["sem-storeexpr"].end(); ++i) {
		i->second.second = SA_STOREPEXPR;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-copyexpr"].begin(); i != ll["sem-copyexpr"].end(); ++i) {
		i->second.second = SA_COPYTERM;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-storepar"].begin(); i != ll["sem-storepar"].end(); ++i) {
		i->second.second = SA_STOREPAR;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-begintempl"].begin(); i != ll["sem-begintempl"].end(); ++i) {
		i->second.second = SA_BEGINTEMPL;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-skiptempl"].begin(); i != ll["sem-skiptempl"].end(); ++i) {
		i->second.second = SA_SKIPTEMPL;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-storetemparg"].begin(); i != ll["sem-storetemparg"].end(); ++i) {
		i->second.second = SA_STORETEMPARG;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-storetemplate"].begin(); i != ll["sem-storetemplate"].end(); ++i) {
		i->second.second = SA_STORETEMPLATE;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setnametf"].begin(); i != ll["sem-setnametf"].end(); ++i) {
		i->second.second = SA_SETNAMETF;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-par2f"].begin(); i != ll["sem-par2f"].end(); ++i) {
		i->second.second = SA_PAR2F;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-unq2f"].begin(); i != ll["sem-unq2f"].end(); ++i) {
		i->second.second = SA_UNQ2F;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-unq2p"].begin(); i != ll["sem-unq2p"].end(); ++i) {
		i->second.second = SA_UNQ2P;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setnamex"].begin(); i != ll["sem-setnamex"].end(); ++i) {
		i->second.second = SA_SETNAMEX;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setnameo"].begin(); i != ll["sem-setnameo"].end(); ++i) {
		i->second.second = SA_SETNAMEO;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-par2ret"].begin(); i != ll["sem-par2ret"].end(); ++i) {
		i->second.second = SA_PAR2RET;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-par2spec"].begin(); i != ll["sem-par2spec"].end(); ++i) {
		i->second.second = SA_PAR2SPEC;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settypev"].begin(); i != ll["sem-settypev"].end(); ++i) {
		i->second.second = SA_SETTYPEV;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settypew"].begin(); i != ll["sem-settypew"].end(); ++i) {
		i->second.second = SA_SETTYPEW;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settypeb"].begin(); i != ll["sem-settypeb"].end(); ++i) {
		i->second.second = SA_SETTYPEB;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settypec"].begin(); i != ll["sem-settypec"].end(); ++i) {
		i->second.second = SA_SETTYPEC;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settypea"].begin(); i != ll["sem-settypea"].end(); ++i) {
		i->second.second = SA_SETTYPEA;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settypeh"].begin(); i != ll["sem-settypeh"].end(); ++i) {
		i->second.second = SA_SETTYPEH;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settypes"].begin(); i != ll["sem-settypes"].end(); ++i) {
		i->second.second = SA_SETTYPES;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settypet"].begin(); i != ll["sem-settypet"].end(); ++i) {
		i->second.second = SA_SETTYPET;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settypei"].begin(); i != ll["sem-settypei"].end(); ++i) {
		i->second.second = SA_SETTYPEI;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settypej"].begin(); i != ll["sem-settypej"].end(); ++i) {
		i->second.second = SA_SETTYPEJ;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settypel"].begin(); i != ll["sem-settypel"].end(); ++i) {
		i->second.second = SA_SETTYPEL;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settypem"].begin(); i != ll["sem-settypem"].end(); ++i) {
		i->second.second = SA_SETTYPEM;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settypex"].begin(); i != ll["sem-settypex"].end(); ++i) {
		i->second.second = SA_SETTYPEX;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settypey"].begin(); i != ll["sem-settypey"].end(); ++i) {
		i->second.second = SA_SETTYPEY;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settypen"].begin(); i != ll["sem-settypen"].end(); ++i) {
		i->second.second = SA_SETTYPEN;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settypeo"].begin(); i != ll["sem-settypeo"].end(); ++i) {
		i->second.second = SA_SETTYPEO;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settypef"].begin(); i != ll["sem-settypef"].end(); ++i) {
		i->second.second = SA_SETTYPEF;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settyped"].begin(); i != ll["sem-settyped"].end(); ++i) {
		i->second.second = SA_SETTYPED;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settypee"].begin(); i != ll["sem-settypee"].end(); ++i) {
		i->second.second = SA_SETTYPEE;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settypeg"].begin(); i != ll["sem-settypeg"].end(); ++i) {
		i->second.second = SA_SETTYPEG;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-settypez"].begin(); i != ll["sem-settypez"].end(); ++i) {
		i->second.second = SA_SETTYPEZ;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setrestrict"].begin(); i != ll["sem-setrestrict"].end(); ++i) {
		i->second.second = SA_SETRESTRICT;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setvolatile"].begin(); i != ll["sem-setvolatile"].end(); ++i) {
		i->second.second = SA_SETVOLATILE;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setconst"].begin(); i != ll["sem-setconst"].end(); ++i) {
		i->second.second = SA_SETCONST;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setptr"].begin(); i != ll["sem-setptr"].end(); ++i) {
		i->second.second = SA_SETPTR;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setref"].begin(); i != ll["sem-setref"].end(); ++i) {
		i->second.second = SA_SETREF;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setrval"].begin(); i != ll["sem-setrval"].end(); ++i) {
		i->second.second = SA_SETRVAL;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setcpair"].begin(); i != ll["sem-setcpair"].end(); ++i) {
		i->second.second = SA_SETCPAIR;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setim"].begin(); i != ll["sem-setim"].end(); ++i) {
		i->second.second = SA_SETIM;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-substd"].begin(); i != ll["sem-substd"].end(); ++i) {
		i->second.second = SA_SUBSTD;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-subalc"].begin(); i != ll["sem-subalc"].end(); ++i) {
		i->second.second = SA_SUBALC;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-substr"].begin(); i != ll["sem-substr"].end(); ++i) {
		i->second.second = SA_SUBSTR;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-substrs"].begin(); i != ll["sem-substrs"].end(); ++i) {
		i->second.second = SA_SUBSTRS;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-subistr"].begin(); i != ll["sem-subistr"].end(); ++i) {
		i->second.second = SA_SUBISTR;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-subostr"].begin(); i != ll["sem-subostr"].end(); ++i) {
		i->second.second = SA_SUBOSTR;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-subiostr"].begin(); i != ll["sem-subiostr"].end(); ++i) {
		i->second.second = SA_SUBIOSTR;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-loadsub"].begin(); i != ll["sem-loadsub"].end(); ++i) {
		i->second.second = SA_LOADSUB;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setnamec"].begin(); i != ll["sem-setnamec"].end(); ++i) {
		i->second.second = SA_SETNAMEC;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setnamed"].begin(); i != ll["sem-setnamed"].end(); ++i) {
		i->second.second = SA_SETNAMED;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopnw"].begin(); i != ll["sem-setopnw"].end(); ++i) {
		i->second.second = SA_SETOPNW;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopna"].begin(); i != ll["sem-setopna"].end(); ++i) {
		i->second.second = SA_SETOPNA;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopdl"].begin(); i != ll["sem-setopdl"].end(); ++i) {
		i->second.second = SA_SETOPDL;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopda"].begin(); i != ll["sem-setopda"].end(); ++i) {
		i->second.second = SA_SETOPDA;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopps"].begin(); i != ll["sem-setopps"].end(); ++i) {
		i->second.second = SA_SETOPPS;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopng"].begin(); i != ll["sem-setopng"].end(); ++i) {
		i->second.second = SA_SETOPNG;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopad"].begin(); i != ll["sem-setopad"].end(); ++i) {
		i->second.second = SA_SETOPAD;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopde"].begin(); i != ll["sem-setopde"].end(); ++i) {
		i->second.second = SA_SETOPDE;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopco"].begin(); i != ll["sem-setopco"].end(); ++i) {
		i->second.second = SA_SETOPCO;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setoppl"].begin(); i != ll["sem-setoppl"].end(); ++i) {
		i->second.second = SA_SETOPPL;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopmi"].begin(); i != ll["sem-setopmi"].end(); ++i) {
		i->second.second = SA_SETOPMI;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopml"].begin(); i != ll["sem-setopml"].end(); ++i) {
		i->second.second = SA_SETOPML;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopdv"].begin(); i != ll["sem-setopdv"].end(); ++i) {
		i->second.second = SA_SETOPDV;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setoprm"].begin(); i != ll["sem-setoprm"].end(); ++i) {
		i->second.second = SA_SETOPRM;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopan"].begin(); i != ll["sem-setopan"].end(); ++i) {
		i->second.second = SA_SETOPAN;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopor"].begin(); i != ll["sem-setopor"].end(); ++i) {
		i->second.second = SA_SETOPOR;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopeo"].begin(); i != ll["sem-setopeo"].end(); ++i) {
		i->second.second = SA_SETOPEO;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopass"].begin(); i != ll["sem-setopass"].end(); ++i) {
		i->second.second = SA_SETOPASS;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setoppll"].begin(); i != ll["sem-setoppll"].end(); ++i) {
		i->second.second = SA_SETOPPLL;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopmii"].begin(); i != ll["sem-setopmii"].end(); ++i) {
		i->second.second = SA_SETOPMII;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopmll"].begin(); i != ll["sem-setopmll"].end(); ++i) {
		i->second.second = SA_SETOPMLL;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopdvv"].begin(); i != ll["sem-setopdvv"].end(); ++i) {
		i->second.second = SA_SETOPDVV;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setoprmm"].begin(); i != ll["sem-setoprmm"].end(); ++i) {
		i->second.second = SA_SETOPRMM;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopann"].begin(); i != ll["sem-setopann"].end(); ++i) {
		i->second.second = SA_SETOPANN;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setoporr"].begin(); i != ll["sem-setoporr"].end(); ++i) {
		i->second.second = SA_SETOPORR;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopeoo"].begin(); i != ll["sem-setopeoo"].end(); ++i) {
		i->second.second = SA_SETOPEOO;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopls"].begin(); i != ll["sem-setopls"].end(); ++i) {
		i->second.second = SA_SETOPLS;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setoprs"].begin(); i != ll["sem-setoprs"].end(); ++i) {
		i->second.second = SA_SETOPRS;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setoplss"].begin(); i != ll["sem-setoplss"].end(); ++i) {
		i->second.second = SA_SETOPLSS;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setoprss"].begin(); i != ll["sem-setoprss"].end(); ++i) {
		i->second.second = SA_SETOPRSS;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopeq"].begin(); i != ll["sem-setopeq"].end(); ++i) {
		i->second.second = SA_SETOPEQ;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopne"].begin(); i != ll["sem-setopne"].end(); ++i) {
		i->second.second = SA_SETOPNE;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setoplt"].begin(); i != ll["sem-setoplt"].end(); ++i) {
		i->second.second = SA_SETOPLT;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopgt"].begin(); i != ll["sem-setopgt"].end(); ++i) {
		i->second.second = SA_SETOPGT;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setople"].begin(); i != ll["sem-setople"].end(); ++i) {
		i->second.second = SA_SETOPLE;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopge"].begin(); i != ll["sem-setopge"].end(); ++i) {
		i->second.second = SA_SETOPGE;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopnt"].begin(); i != ll["sem-setopnt"].end(); ++i) {
		i->second.second = SA_SETOPNT;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopaa"].begin(); i != ll["sem-setopaa"].end(); ++i) {
		i->second.second = SA_SETOPAA;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopoo"].begin(); i != ll["sem-setopoo"].end(); ++i) {
		i->second.second = SA_SETOPOO;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setoppp"].begin(); i != ll["sem-setoppp"].end(); ++i) {
		i->second.second = SA_SETOPPP;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopmm"].begin(); i != ll["sem-setopmm"].end(); ++i) {
		i->second.second = SA_SETOPMM;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopcm"].begin(); i != ll["sem-setopcm"].end(); ++i) {
		i->second.second = SA_SETOPCM;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setoppm"].begin(); i != ll["sem-setoppm"].end(); ++i) {
		i->second.second = SA_SETOPPM;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setoppt"].begin(); i != ll["sem-setoppt"].end(); ++i) {
		i->second.second = SA_SETOPPT;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopcl"].begin(); i != ll["sem-setopcl"].end(); ++i) {
		i->second.second = SA_SETOPCL;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopix"].begin(); i != ll["sem-setopix"].end(); ++i) {
		i->second.second = SA_SETOPIX;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopqu"].begin(); i != ll["sem-setopqu"].end(); ++i) {
		i->second.second = SA_SETOPQU;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopst"].begin(); i != ll["sem-setopst"].end(); ++i) {
		i->second.second = SA_SETOPST;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopsz"].begin(); i != ll["sem-setopsz"].end(); ++i) {
		i->second.second = SA_SETOPSZ;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopat"].begin(); i != ll["sem-setopat"].end(); ++i) {
		i->second.second = SA_SETOPAT;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopaz"].begin(); i != ll["sem-setopaz"].end(); ++i) {
		i->second.second = SA_SETOPAZ;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopcv"].begin(); i != ll["sem-setopcv"].end(); ++i) {
		i->second.second = SA_SETOPCV;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setopxx"].begin(); i != ll["sem-setopxx"].end(); ++i) {
		i->second.second = SA_SETOPXX;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setnamef"].begin(); i != ll["sem-setnamef"].end(); ++i) {
		i->second.second = SA_SETNAMEF;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setnamevt"].begin(); i != ll["sem-setnamevt"].end(); ++i) {
		i->second.second = SA_SETNAMEVT;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-reversename"].begin(); i != ll["sem-reversename"].end(); ++i) {
		i->second.second = SA_REVERSENAME;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setprivate"].begin(); i != ll["sem-setprivate"].end(); ++i) {
		i->second.second = SA_SETPRIVATE;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setpublic"].begin(); i != ll["sem-setpublic"].end(); ++i) {
		i->second.second = SA_SETPUBLIC;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setprotected"].begin(); i != ll["sem-setprotected"].end(); ++i) {
		i->second.second = SA_SETPROTECTED;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setfcdecl"].begin(); i != ll["sem-setfcdecl"].end(); ++i) {
		i->second.second = SA_SETFCDECL;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setfpascal"].begin(); i != ll["sem-setfpascal"].end(); ++i) {
		i->second.second = SA_SETFPASCAL;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setffortran"].begin(); i != ll["sem-setffortran"].end(); ++i) {
		i->second.second = SA_SETFFORTRAN;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setfthiscall"].begin(); i != ll["sem-setfthiscall"].end(); ++i) {
		i->second.second = SA_SETFTHISCALL;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setfstdcall"].begin(); i != ll["sem-setfstdcall"].end(); ++i) {
		i->second.second = SA_SETFSTDCALL;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setffastcall"].begin(); i != ll["sem-setffastcall"].end(); ++i) {
		i->second.second = SA_SETFFASTCALL;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setfinterrupt"].begin(); i != ll["sem-setfinterrupt"].end(); ++i) {
		i->second.second = SA_SETFINTERRUPT;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setunion"].begin(); i != ll["sem-setunion"].end(); ++i) {
		i->second.second = SA_SETUNION;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setstruct"].begin(); i != ll["sem-setstruct"].end(); ++i) {
		i->second.second = SA_SETSTRUCT;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setclass"].begin(); i != ll["sem-setclass"].end(); ++i) {
		i->second.second = SA_SETCLASS;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setenum"].begin(); i != ll["sem-setenum"].end(); ++i) {
		i->second.second = SA_SETENUM;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setstatic"].begin(); i != ll["sem-setstatic"].end(); ++i) {
		i->second.second = SA_SETSTATIC;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setvirtual"].begin(); i != ll["sem-setvirtual"].end(); ++i) {
		i->second.second = SA_SETVIRTUAL;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-stclconst"].begin(); i != ll["sem-stclconst"].end(); ++i) {
		i->second.second = SA_STCLCONST;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-stclvol"].begin(); i != ll["sem-stclvol"].end(); ++i) {
		i->second.second = SA_STCLVOL;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-stclfar"].begin(); i != ll["sem-stclfar"].end(); ++i) {
		i->second.second = SA_STCLFAR;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-stclhuge"].begin(); i != ll["sem-stclhuge"].end(); ++i) {
		i->second.second = SA_STCLHUGE;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-savenamesub"].begin(); i != ll["sem-savenamesub"].end(); ++i) {
		i->second.second = SA_SAVENAMESUB;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-loadnamesub"].begin(); i != ll["sem-loadnamesub"].end(); ++i) {
		i->second.second = SA_LOADNAMESUB;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-mstemplsub"].begin(); i != ll["sem-mstemplsub"].end(); ++i) {
		i->second.second = SA_MSTEMPLPSUB;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setnamer0"].begin(); i != ll["sem-setnamer0"].end(); ++i) {
		i->second.second = SA_SETNAMER0;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setnamer1"].begin(); i != ll["sem-setnamer1"].end(); ++i) {
		i->second.second = SA_SETNAMER1;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setnamer2"].begin(); i != ll["sem-setnamer2"].end(); ++i) {
		i->second.second = SA_SETNAMER2;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setnamer3"].begin(); i != ll["sem-setnamer3"].end(); ++i) {
		i->second.second = SA_SETNAMER3;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setnamer4"].begin(); i != ll["sem-setnamer4"].end(); ++i) {
		i->second.second = SA_SETNAMER4;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_a"].begin(); i != ll["sem-setname_a"].end(); ++i) {
		i->second.second = SA_SETNAME_A;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_b"].begin(); i != ll["sem-setname_b"].end(); ++i) {
		i->second.second = SA_SETNAME_B;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_c"].begin(); i != ll["sem-setname_c"].end(); ++i) {
		i->second.second = SA_SETNAME_C;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_d"].begin(); i != ll["sem-setname_d"].end(); ++i) {
		i->second.second = SA_SETNAME_D;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_e"].begin(); i != ll["sem-setname_e"].end(); ++i) {
		i->second.second = SA_SETNAME_E;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_f"].begin(); i != ll["sem-setname_f"].end(); ++i) {
		i->second.second = SA_SETNAME_F;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_g"].begin(); i != ll["sem-setname_g"].end(); ++i) {
		i->second.second = SA_SETNAME_G;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_h"].begin(); i != ll["sem-setname_h"].end(); ++i) {
		i->second.second = SA_SETNAME_H;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_i"].begin(); i != ll["sem-setname_i"].end(); ++i) {
		i->second.second = SA_SETNAME_I;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_j"].begin(); i != ll["sem-setname_j"].end(); ++i) {
		i->second.second = SA_SETNAME_J;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_k"].begin(); i != ll["sem-setname_k"].end(); ++i) {
		i->second.second = SA_SETNAME_K;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_l"].begin(); i != ll["sem-setname_l"].end(); ++i) {
		i->second.second = SA_SETNAME_L;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_m"].begin(); i != ll["sem-setname_m"].end(); ++i) {
		i->second.second = SA_SETNAME_M;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_n"].begin(); i != ll["sem-setname_n"].end(); ++i) {
		i->second.second = SA_SETNAME_N;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_o"].begin(); i != ll["sem-setname_o"].end(); ++i) {
		i->second.second = SA_SETNAME_O;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_p"].begin(); i != ll["sem-setname_p"].end(); ++i) {
		i->second.second = SA_SETNAME_P;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_q"].begin(); i != ll["sem-setname_q"].end(); ++i) {
		i->second.second = SA_SETNAME_Q;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_r"].begin(); i != ll["sem-setname_r"].end(); ++i) {
		i->second.second = SA_SETNAME_R;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_s"].begin(); i != ll["sem-setname_s"].end(); ++i) {
		i->second.second = SA_SETNAME_S;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_t"].begin(); i != ll["sem-setname_t"].end(); ++i) {
		i->second.second = SA_SETNAME_T;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_u"].begin(); i != ll["sem-setname_u"].end(); ++i) {
		i->second.second = SA_SETNAME_U;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_v"].begin(); i != ll["sem-setname_v"].end(); ++i) {
		i->second.second = SA_SETNAME_V;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_w"].begin(); i != ll["sem-setname_w"].end(); ++i) {
		i->second.second = SA_SETNAME_W;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_x"].begin(); i != ll["sem-setname_x"].end(); ++i) {
		i->second.second = SA_SETNAME_X;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_y"].begin(); i != ll["sem-setname_y"].end(); ++i) {
		i->second.second = SA_SETNAME_Y;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-setname_z"].begin(); i != ll["sem-setname_z"].end(); ++i) {
		i->second.second = SA_SETNAME_Z;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-templ2tftpl"].begin(); i != ll["sem-templ2tftpl"].end(); ++i) {
		i->second.second = SA_TEMPL2TFTPL;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-beginbsub"].begin(); i != ll["sem-beginbsub"].end(); ++i) {
		i->second.second = SA_BEGINBSUB;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-loadbsub"].begin(); i != ll["sem-loadbsub"].end(); ++i) {
		i->second.second = SA_LOADBSUB;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-addmconst"].begin(); i != ll["sem-addmconst"].end(); ++i) {
		i->second.second = SA_ADDMCONST;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-addmvol"].begin(); i != ll["sem-addmvol"].end(); ++i) {
		i->second.second = SA_ADDMVOL;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-addmfar"].begin(); i != ll["sem-addmfar"].end(); ++i) {
		i->second.second = SA_ADDMFAR;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-addmhuge"].begin(); i != ll["sem-addmhuge"].end(); ++i) {
		i->second.second = SA_ADDMHUGE;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-loadmsnum"].begin(); i != ll["sem-loadmsnum"].end(); ++i) {
		i->second.second = SA_LOADMSNUM;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-numtorttibcd"].begin(); i != ll["sem-numtorttibcd"].end(); ++i) {
		i->second.second = SA_NUMTORTTIBCD;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-numtotype"].begin(); i != ll["sem-numtotype"].end(); ++i) {
		i->second.second = SA_NUMTOTYPE;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-normalizeparname"].begin(); i != ll["sem-normalizeparname"].end(); ++i) {
		i->second.second = SA_BORLANDNORMALIZEPARNAME;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-borlandid"].begin(); i != ll["sem-borlandid"].end(); ++i) {
		i->second.second = SA_BORLANDID;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-loadborlandsub"].begin(); i != ll["sem-loadborlandsub"].end(); ++i) {
		i->second.second = SA_LOADBORLANDSUB;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-borlandarr"].begin(); i != ll["sem-borlandarr"].end(); ++i) {
		i->second.second = SA_BORLANDARR;
	}

	for(map<char,pair<unsigned int, semact>>::iterator i=ll["sem-end"].begin(); i != ll["sem-end"].end(); ++i) {
		i->second.second = SA_END;
	}

}

/**
 * @brief Function which converts external grammar into internal grammar. No analysis may be done after using this function.
 * @param inputfilename The name of the file which contains grammar rules.
 * @param outputname The name of the output grammar.
 * @return Error code.
 */
cGram::errcode cGram::generateIgrammar(const string inputfilename, const string outputname) {
	errcode retvalue = ERROR_OK;

	//if the output files already exist, end with error. overwriting is not allowed
	if (fileExists("stgrammars/"+outputname+"ll.cpp")) {
		errString = string("") + "Igrammar file " + "stgrammars/"+outputname+"ll.cpp" + " already exists.";
		return ERROR_FILE;
	}
	if (fileExists("stgrammars/"+outputname+"ll.h")) {
		errString = string("") + "Igrammar file " + "stgrammars/"+outputname+"ll.h" + " already exists.";
		return ERROR_FILE;
	}

	//get current date/time
	sdate_t createDate = genTimeStruct();

	//set the variable that triggers igrammar construction during LL table creation (in getgrammar() and genconstll())
	createIGrammar = outputname;

	//get the rules for external grammar
	//sets newIG_ruleaddrs and newIG_ruleelements
	if ((retvalue = getgrammar(inputfilename)) != ERROR_OK) {return retvalue;}
	delete source;

	//standard steps to achieve LL table generation
	genempty();
	genfirst();
	genfollow();
	genpredict();

	//generate LL table into strings. sets newIG_terminal_static and newIG_llst
	if ((retvalue = genconstll()) != ERROR_OK) {return retvalue;}

	//generate newIG_root from left nt of first rule
	newIG_root = string("") + "cGram::GE_NONTERM,const_cast<char *>(\"" + string(rules[0].left.nt) + "\"),0,\'\\0\'";

	//output filestreams for the new .cpp and .h files
	ofstream ofsIgCpp;
	ofstream ofsIgH;
	//open .cpp for writing
	ofsIgCpp.open(("stgrammars/"+outputname+"ll.cpp").c_str());
	if (!ofsIgCpp.is_open()) {
		errString = string("") + "Could not open " + "stgrammars/"+outputname+"ll.cpp" + " for writing.";
		return ERROR_FILE;
	}
	//open .h for writing
	ofsIgH.open(("stgrammars/"+outputname+"ll.h").c_str());
	if (!ofsIgH.is_open()) {
		errString = string("") + "Could not open " + "stgrammars/"+outputname+"ll.h" + " for writing.";
		return ERROR_FILE;
	}

	stringstream date;
	date << createDate.y << "-" << setfill('0') << setw(2) << createDate.m << "-" << createDate.d;

	/*
	 * Generate .cpp header
	 */
	ofsIgCpp << "/**\n";
	ofsIgCpp << "* @file   demangler/stgrammars/" << outputname << "ll.cpp\n";
	ofsIgCpp << "* @author Internal Grammar Generator\n";
	ofsIgCpp << "* @brief  Internal LL grammar for demangler.\n";
	ofsIgCpp << "* @date   " << createDate.y << "-" << setfill('0') << setw(2) << createDate.m << "-" << createDate.d << "\n";
	ofsIgCpp << "* @copyright (c) 2017 Avast Software, licensed under the MIT license\n";
	ofsIgCpp << "*/\n";

	/*
	 * Generate .h header
	 */
	ofsIgH << "/**\n";
	ofsIgH << "* @file   demangler/stgrammars/" << outputname << "ll.h\n";
	ofsIgH << "* @author Internal Grammar Generator\n";
	ofsIgH << "* @brief  Internal LL grammar for demangler.\n";
	ofsIgH << "* @date   " << createDate.y << "-" << setfill('0') << setw(2) << createDate.m << "-" << createDate.d << "\n";
	ofsIgH << "* @copyright (c) 2017 Avast Software, licensed under the MIT license\n";
	ofsIgH << "*/\n";

	/*
	 * Generate .h content
	 */
	//generate <outputname>LL_H_
	string hDef = outputname;
	transform(hDef.begin(), hDef.end(), hDef.begin(), ::toupper);
	hDef += "LL_H_";

	//.h body
	ofsIgH << "\n";
	ofsIgH << "#ifndef " << hDef << "\n";
	ofsIgH << "#define " << hDef << "\n";
	ofsIgH << "\n";
	ofsIgH << "#include \"demangler/gparser.h\"\n";
	ofsIgH << "\n";
	ofsIgH << "namespace demangler\n";
	ofsIgH << "{\n";
	ofsIgH << "\n";
	ofsIgH << "class cIgram_" << outputname << "ll {\n";
	ofsIgH << "public:\n";
	ofsIgH << "\tstatic unsigned char terminal_static[" << newIG_terminal_static_x << "];\n";
	ofsIgH << "\tstatic cGram::llelem_t llst[" << newIG_llst_x << "][" << newIG_llst_y << "];\n";
	ofsIgH << "\tstatic cGram::ruleaddr_t ruleaddrs[" << newIG_ruleaddrs_x << "];\n";
	ofsIgH << "\tstatic cGram::gelem_t ruleelements[" << newIG_ruleelements_x << "];\n";
	ofsIgH << "\tstatic cGram::gelem_t root;\n";
	ofsIgH << "\tcGram::igram_t getInternalGrammar();\n";
	ofsIgH << "};\n";
	ofsIgH << "\n";
	ofsIgH << "} /* namespace demangler */\n";
	ofsIgH << "#endif /* " << hDef << " */\n";

	/*
	 * Generate .cpp content
	 */
	ofsIgCpp << "\n";
	ofsIgCpp << "#include <stdlib.h>\n";
	ofsIgCpp << "#include \"" << outputname << "ll.h\"\n";
	ofsIgCpp << "\n";
	ofsIgCpp << "namespace demangler\n";
	ofsIgCpp << "{\n";
	ofsIgCpp << "\n";
	ofsIgCpp << "/**\n";
	ofsIgCpp << " * @brief Static version of the root element.\n";
	ofsIgCpp << " */\n";
	ofsIgCpp << "cGram::gelem_t cIgram_" << outputname << "ll::root = {" << newIG_root << "};\n";
	ofsIgCpp << "\n";
	ofsIgCpp << "/**\n";
	ofsIgCpp << " * @brief Static table of used terminals. Used to reduce size of static LL table.\n";
	ofsIgCpp << " */\n";
	ofsIgCpp << "unsigned char cIgram_" << outputname << "ll::terminal_static[" << newIG_terminal_static_x << "] = {\n";
	ofsIgCpp << newIG_terminal_static << "};\n";
	ofsIgCpp << "\n";
	ofsIgCpp << "/**\n";
	ofsIgCpp << " * @brief Static adress table for grammar elements in rules. First value is offset from the start of ruleelements array, second value is the number of elements in the current rule.\n";
	ofsIgCpp << " */\n";
	ofsIgCpp << "cGram::ruleaddr_t cIgram_" << outputname << "ll::ruleaddrs[" << newIG_ruleaddrs_x << "] = {";
	ofsIgCpp << newIG_ruleaddrs;
	ofsIgCpp << "};\n";
	ofsIgCpp << "\n";
	ofsIgCpp << "/**\n";
	ofsIgCpp << " * @brief Static table of grammar elements in all rules.\n";
	ofsIgCpp << " */\n";
	ofsIgCpp << "cGram::gelem_t cIgram_" << outputname << "ll::ruleelements[" << newIG_ruleelements_x << "] = {\n";
	ofsIgCpp << newIG_ruleelements;
	ofsIgCpp << "\n};\n";
	ofsIgCpp << "\n";
	ofsIgCpp << "/**\n";
	ofsIgCpp << " * @brief Static LL table\n";
	ofsIgCpp << " */\n";
	ofsIgCpp << "cGram::llelem_t cIgram_" << outputname << "ll::llst[" << newIG_llst_x << "][" << newIG_llst_y << "] = {\n";
	ofsIgCpp << newIG_llst;
	ofsIgCpp << "\n};\n";
	ofsIgCpp << "\n";
	ofsIgCpp << "/**\n";
	ofsIgCpp << " * @brief Export internal grammar data for use in demangler.\n";
	ofsIgCpp << " * @return igram_t structure containing the internal grammar data.\n";
	ofsIgCpp << " */\n";
	ofsIgCpp << "cGram::igram_t cIgram_" << outputname << "ll::getInternalGrammar() {\n";
	ofsIgCpp << "\tcGram::igram_t retvalue = {\n";
	ofsIgCpp << "\t\t" << newIG_terminal_static_x << ", //terminal_static_x\n";
	ofsIgCpp << "\t\t" << newIG_ruleaddrs_x << ", //ruleaddrs_x\n";
	ofsIgCpp << "\t\t" << newIG_ruleelements_x << ", //ruleelements_x\n";
	ofsIgCpp << "\t\t" << newIG_llst_x << ", //llst_x\n";
	ofsIgCpp << "\t\t" << newIG_llst_y << ", //llst_y\n";
	ofsIgCpp << "\t\troot,\n";
	ofsIgCpp << "\t\tterminal_static,\n";
	ofsIgCpp << "\t\truleaddrs,\n";
	ofsIgCpp << "\t\truleelements,\n";
	ofsIgCpp << "\t\tNULL\n";
	ofsIgCpp << "\t};\n";
	ofsIgCpp << "\t\n";
	ofsIgCpp << "\t//dynamic allocation of the llst\n";
	ofsIgCpp << "\tretvalue.llst = static_cast<cGram::llelem_t**>(malloc(sizeof(cGram::llelem_t*) * retvalue.llst_x));\n";
	ofsIgCpp << "\tfor (unsigned int i=0; i<retvalue.llst_x; i++) {\n";
	ofsIgCpp << "\t\tretvalue.llst[i] = llst[i];\n";
	ofsIgCpp << "\t}\n";
	ofsIgCpp << "\t\n";
	ofsIgCpp << "\treturn retvalue;\n";
	ofsIgCpp << "}\n";
	ofsIgCpp << "\n";
	ofsIgCpp << "} /* namespace demangler */\n";

	//clean up the output file streams
	ofsIgCpp.close();
	ofsIgH.close();
	return retvalue;
}

/**
 * @brief Function which calls all the generating functions to transform input grammar file into an LL table.
 * @param filename The name of the file which contains grammar rules
 * @return Error code.
 */
cGram::errcode cGram::parse(const string filename) {
	errcode retvalue = ERROR_OK;

	//get the rules for external grammar
	if ((retvalue = getgrammar(filename)) != ERROR_OK) {return retvalue;}
	delete source;

	genempty();
	genfirst();
	genfollow();
	genpredict();
	retvalue = genll();
	if (retvalue != ERROR_OK) {
		return retvalue;
	}
	genllsem();

	return retvalue;
}

/**
 * @brief Function which creates template parameter sequence for SA_SUBSTRS.
 * @param pName Reference to any object of cName class, of which the type_t clear function is used.
 * @return Void type pointer to template of SA_SUBSTRS (vector of parameters).
 */
void *cGram::getbstpl(cName & pName) {
	vector<cName::type_t> return_vector;
	vector<cName::type_t> temp_type_vector;
	vector<cName::name_t> temp_name_vector;
	cName::name_t temp_name;
	cName::type_t temp_type;
	pName.type_t_clear(temp_type);

	//first parameter "char" is inserted into final vector and also into temp vector
	temp_type.type = cName::TT_BUILTIN;
	temp_type.b = cName::T_CHAR;
	return_vector.push_back(temp_type);
	temp_type_vector.push_back(temp_type);
	pName.type_t_clear(temp_type);

	//second parameter
	temp_name_vector.clear();
	temp_name.tpl = nullptr;
	temp_name.un = "std";
	temp_name_vector.push_back(temp_name);
	temp_name.un = "char_traits";
	temp_name.tpl = static_cast<void *>(new vector<cName::type_t>(temp_type_vector));
	temp_name_vector.push_back(temp_name);
	temp_type.type = cName::TT_NAME;
	temp_type.n = temp_name_vector;
	return_vector.push_back(temp_type);
	pName.type_t_clear(temp_type);

	//third parameter
	temp_name_vector.clear();
	temp_name.tpl = nullptr;
	temp_name.un = "std";
	temp_name_vector.push_back(temp_name);
	temp_name.un = "allocator";
	temp_name.tpl = static_cast<void *>(new vector<cName::type_t>(temp_type_vector));
	temp_name_vector.push_back(temp_name);
	temp_type.type = cName::TT_NAME;
	temp_type.n = temp_name_vector;
	return_vector.push_back(temp_type);
	pName.type_t_clear(temp_type);

	return static_cast<void *>(new vector<cName::type_t>(return_vector));
}

/**
 * @brief Function which creates template parameter sequence for basic stream template.
 * @param pName Reference to any object of cName class, of which the type_t clear function is used.
 * @return Void type pointer the template (vector of parameters).
 */
void *cGram::getstrtpl(cName & pName) {
	vector<cName::type_t> return_vector;
	vector<cName::type_t> temp_type_vector;
	vector<cName::name_t> temp_name_vector;
	cName::name_t temp_name;
	cName::type_t temp_type;
	pName.type_t_clear(temp_type);

	//first parameter "char" is inserted into final vector and also into temp vector
	temp_type.type = cName::TT_BUILTIN;
	temp_type.b = cName::T_CHAR;
	return_vector.push_back(temp_type);
	temp_type_vector.push_back(temp_type);
	pName.type_t_clear(temp_type);

	//second parameter
	temp_name_vector.clear();
	temp_name.tpl = nullptr;
	temp_name.un = "std";
	temp_name_vector.push_back(temp_name);
	temp_name.un = "char_traits";
	temp_name.tpl = static_cast<void *>(new vector<cName::type_t>(temp_type_vector));
	temp_name_vector.push_back(temp_name);
	temp_type.type = cName::TT_NAME;
	temp_type.n = temp_name_vector;
	return_vector.push_back(temp_type);
	pName.type_t_clear(temp_type);

	return static_cast<void *>(new vector<cName::type_t>(return_vector));
}

/**
 * @brief Function which determines whether a substitution already exists in given substitution vector.
 * @param candidate Candidate substitution string.
 * @param vec Vector of existing substitutions.
 * @return "Does 'candidate' already exist in 'vec'?"
 */
bool cGram::issub(string candidate,vector<string> & vec) {
	bool retvalue = false;
	if (candidate == "") {
		return true;
	}
	//TODO hacking for built-in substitutions
	if (candidate == "3std" || candidate == "3std9allocator" || candidate == "3std6string") {
		return true;
	}


	for(vector<string>::iterator i=vec.begin(); i != vec.end(); ++i) {
		if (candidate == (*i)) {
			retvalue = true;
			break;
		}
	}
	return retvalue;
}

/**
 * @brief Function which prints existing substitutions.
 * @param vec Vector of substitutions to be printed.
 */
void cGram::showsubs(vector<string> & vec) {
	unsigned int count = 0;
	for(vector<string>::iterator i=vec.begin(); i != vec.end(); ++i) {
		cout << "S";
		if (count != 0) {
			cout << count-1;
		}
		cout << "_: " << (*i) << endl;
		++count;
	}
}

/**
 * @brief Function which converts a Base36 number to long int.
 * @param x String containing the Base36 number (0-9, A-Z). Unknown symbols are ignored.
 * @return Long int converted from the Base36 number.
 * @retval -1 The Base36 string was empty.
 */
long int cGram::b36toint(string x) {
	long int retvalue = 0;
	//return -1 if string is empty
	if (x.empty()) {
		return -1;
	}
	//cycle through the string
	for (size_t i=0; i<x.length();++i) {
		retvalue *= 36;
		//0-9
		if (x[i] >= '0' && x[i] <= '9') {
			retvalue += x[i] - '0';
		}
		//A-Z
		else if (x[i] >= 'A' && x[i] <= 'Z') {
			retvalue += 10 + x[i] - 'A';
		}
		//ignore incorrect symbols
	}
	return retvalue;
}

cGram::llelem_t cGram::getllpair(string nt, unsigned int ntst, unsigned char t) {
	llelem_t retvalue;

	//internal grammar
	if (internalGrammar) {
		retvalue = internalGrammarStruct.llst[ntst][internalGrammarStruct.terminal_static[t]];
	}
	//external grammar
	else {
		retvalue.n = ll[nt][t].first;
		retvalue.s = ll[nt][t].second;
	}
	return retvalue;
}

/**
 * @brief The pre-analyzer which expands substitutions.
 * @param input The mangled name.
 * @param err Pointer to error code.
 * @return Input string with expanded substitutions.
 */
string cGram::subanalyze(const string input, cGram::errcode *err) {
	string retvalue;

	string current_part;
	bool last_rule = false;
	stack<gelem_t> elemstack;
	size_t position = 0;
	bool builtin = false;
	bool pexpr = false;
	subtype current_st = ST_NULL;
	unsigned int current_recursion = 0;
	bool fname = true;
	bool subbing = false; //if true, all other subs will be ignored
	bool subchange = true;
	unsigned int sub_recursion = 0; //used as temporary to determine in which level the sub is expanded
	string current_sub_id;
	string current_sub;
	string current_array;
	string current_templ_sub;
	string current_name;
	string current_unq_name;

	string current_param;
	string current_param_modifiers;
	string current_params;

	string current_retvalue;
	string current_input;

	stack<string> name_stack;
	stack<string> paramm_stack;
	stack<string> params_stack;
	stack<vector<string>> arrays_stack;
	vector<string> template_subs;
	vector<string> substitutions;

	vector<string> arrays;

	//grammar element from the top of stack
	gelem_t current_element;
	//vector of rules used during the analysis
	vector<unsigned int> used_rules;
	//currently used rule. contains the semantic action as well
	llelem_t current_rule;

	//id length used for loading IDs
	unsigned int current_id_length = 0;

	//current symbol of the analyzed string.
	char current_char = input[position];

	current_input = input;
	//the loop for multiple subanalyze passes
	while (subchange) {
		current_part.clear();
		last_rule = false;
		while (!elemstack.empty()) {elemstack.pop();}
		position = 0;
		builtin = false;
		fname = true;
		current_st = ST_NULL;
		current_recursion = 0;
		subbing = false;
		pexpr = false;
		sub_recursion = 0;
		current_id_length = 0;
		current_sub_id.clear();
		current_sub.clear();
		current_array.clear();
		current_name.clear();
		current_unq_name.clear();
		current_param.clear();
		current_param_modifiers.clear();
		current_param_modifiers.clear();
		current_params.clear();
		current_templ_sub.clear();
		template_subs.clear();
		while (!name_stack.empty()) {name_stack.pop();}
		while (!paramm_stack.empty()) {paramm_stack.pop();}
		while (!params_stack.empty()) {params_stack.pop();}
		while (!arrays_stack.empty()) {arrays_stack.pop();}
		arrays.clear();
		used_rules.clear();
		subchange = false;

		//insert the root NT into the stack
		if (internalGrammar) {
			elemstack.push(internalGrammarStruct.root);
		}
		else {
			elemstack.push(rules[0].left);
		}

		current_char = input[position];

		//the main loop of one pass
		while (!last_rule && *err == ERROR_OK) {
			if (elemstack.empty()) {
					errString = "cGram::subanalyze: Syntax error: elemstack empty ";
					*err = ERROR_SYN;
					break;
			}
			//load the current character of the input string
			if (position == current_input.length()) {
				current_char = '\0';
			}
			else {
				current_char = current_input[position];
			}
			current_element = elemstack.top();
			elemstack.pop();

			//top of stack is a non-terminal
			if (current_element.type == GE_NONTERM) {
				//load the rule number for current NT and T and check for syntax error
				if ((current_rule = getllpair(current_element.nt, current_element.ntst, current_char)).n == 0) {
					errString = string("cGram::subanalyze: Syntax error: No rule for NT ") + current_element.nt + " and T " + current_char + ".";
					*err = ERROR_SYN;
					break;
				}
				//add the currently used rule to the vector of used rules
				else {
					used_rules.push_back(current_rule.n);
				}

				//internal grammar
				if (internalGrammar) {
					for(unsigned int i=internalGrammarStruct.ruleaddrs[current_rule.n-1].size; i>0; i--) {
						elemstack.push(internalGrammarStruct.ruleelements[internalGrammarStruct.ruleaddrs[current_rule.n-1].offset + (i-1)]);
					}
				}
				//external grammar
				else {
					//push right side of the used rule into the stack
					for(vector<gelem_t>::reverse_iterator i = rules[current_rule.n-1].right.rbegin(); i != rules[current_rule.n-1].right.rend(); ++i) {
						elemstack.push(*i);
					}
				}


				//semantic action
				switch (current_rule.s) {
					//do nothing
					case SA_NULL:
						break;

					case SA_LOADARR:
						//remove the number terminal and "_" from element stack
						elemstack.pop();
						elemstack.pop();
						current_id_length = 0;
						current_array.clear();
						current_array += 'A';

						while (current_input[position] != '_') {
							if (position == current_input.length()) {
								errString = "cGram::subanalyze: Syntax error: Unexpected end of array.";
								*err = ERROR_SYN;
								break;
							}
							if (!isdigit(current_input[position])) {
								errString = string("") + "cGram::subanalyze: Syntax error: Unknown array symbol " + current_input[position] + ".";
								*err = ERROR_SYN;
								break;
							}
							current_id_length *= 10;
							current_id_length += current_input[position] - '0';
							current_array += current_input[position];
							current_part += current_input[position];
							++position;
						}
						if (*err != ERROR_OK) {
							break;
						}
						//_
						current_array += current_input[position];
						current_part += current_input[position];
						++position;

						if (!subbing) {
							arrays.push_back(current_array);
							current_params += current_array;
						}

						break;

					case SA_SETNAMEC:
					case SA_SETNAMED:
					case SA_SETNAMEO:
						//save previous names as sub
						if (!subbing && !issub(current_name,substitutions)) {
							substitutions.push_back(current_name);
						}
						break;

					case SA_LOADID:
						//remove the number terminal from element stack
						elemstack.pop();
						current_id_length = 0;

						//save previous names as sub
						if (!subbing && !issub(current_name,substitutions)) {
							substitutions.push_back(current_name);
						}

						//load the length of ID
						while (isdigit(current_input[position])) {
							if (position == current_input.length()) {
								errString = "cGram::subanalyze: Syntax error: Unexpected end of identifier length.";
								*err = ERROR_SYN;
								break;
							}
							current_unq_name += current_input[position];
							current_part += current_input[position];
							current_id_length *= 10;
							current_id_length += (current_input[position++] - '0');
						}
						if (*err != ERROR_OK) {
							break;
						}

						//load the ID
						for (unsigned int i = 0; i < current_id_length; ++i) {
							if (position == current_input.length()) {
								errString = string("") + "cGram::subanalyze: Syntax error: Unexpected end of identifier " + current_unq_name + ".";
								*err = ERROR_SYN;
								break;
							}
							current_part += current_input[position];
							current_unq_name += current_input[position++];
						}
						if (*err != ERROR_OK) {
							break;
						}

						current_name += current_unq_name;
						current_unq_name.clear();

						//synchronize current_char to current_input[position]
						if (position == current_input.length()) {
							current_char = '\0';
						}
						else {
							current_char = current_input[position];
						}
						break;

					//last rule - successfully end
					case SA_END:
						current_retvalue = current_part;
						last_rule = true;
						break;

					case SA_UNQ2F:
						fname = false;
						current_name.clear();
						current_param_modifiers.clear();
						current_params.clear();
						break;

					case SA_STOREPAR:
						if (!builtin) {
							//save sub
							if (!subbing && !issub(current_name,substitutions)) {
								substitutions.push_back(current_name);
							}
							current_params += current_param_modifiers;
							current_params += "N";
							current_params += current_name;
							current_params += "E";
							current_name = "N"+current_name+"E";
						}
						while (!subbing && !current_param_modifiers.empty()) {
							current_name = current_param_modifiers[current_param_modifiers.length()-1]+current_name;
							if (!current_param_modifiers.empty() && !issub('\0'+current_name,substitutions)) {
								substitutions.push_back('\0'+current_name);
							}
							current_param_modifiers = current_param_modifiers.substr(0,current_param_modifiers.length()-1);
						}

						while(!subbing && !arrays.empty()) {
							if (!issub('\0'+arrays.back()+current_name,substitutions)) {
								substitutions.push_back('\0'+arrays.back()+current_name);
							}
							current_name = arrays.back()+current_name;
							arrays.pop_back();
						}

						current_param_modifiers.clear();
						current_name.clear();
						builtin = false;


						break;

					case SA_SETOPNW:
					case SA_SETOPNA:
					case SA_SETOPDL:
					case SA_SETOPDA:
					case SA_SETOPPS:
					case SA_SETOPNG:
					case SA_SETOPAD:
					case SA_SETOPDE:
					case SA_SETOPCO:
					case SA_SETOPPL:
					case SA_SETOPMI:
					case SA_SETOPML:
					case SA_SETOPDV:
					case SA_SETOPRM:
					case SA_SETOPAN:
					case SA_SETOPOR:
					case SA_SETOPEO:
					case SA_SETOPASS:
					case SA_SETOPPLL:
					case SA_SETOPMII:
					case SA_SETOPMLL:
					case SA_SETOPDVV:
					case SA_SETOPRMM:
					case SA_SETOPANN:
					case SA_SETOPORR:
					case SA_SETOPEOO:
					case SA_SETOPLS:
					case SA_SETOPRS:
					case SA_SETOPLSS:
					case SA_SETOPRSS:
					case SA_SETOPEQ:
					case SA_SETOPNE:
					case SA_SETOPLT:
					case SA_SETOPGT:
					case SA_SETOPLE:
					case SA_SETOPGE:
					case SA_SETOPNT:
					case SA_SETOPAA:
					case SA_SETOPOO:
					case SA_SETOPPP:
					case SA_SETOPMM:
					case SA_SETOPCM:
					case SA_SETOPPM:
					case SA_SETOPPT:
					case SA_SETOPCL:
					case SA_SETOPIX:
					case SA_SETOPQU:
					case SA_SETOPST:
					case SA_SETOPSZ:
					case SA_SETOPAT:
					case SA_SETOPAZ:
					case SA_SETOPCV:
						//save previous names as sub
						if (!subbing && !issub(current_name,substitutions)) {
							substitutions.push_back(current_name);
						}
						current_name += current_part.substr(current_part.length()-1);
						current_name += current_char;
						template_subs.clear();
						break;

					case SA_SETTYPEV:
					case SA_SETTYPEW:
					case SA_SETTYPEB:
					case SA_SETTYPEC:
					case SA_SETTYPEA:
					case SA_SETTYPEH:
					case SA_SETTYPES:
					case SA_SETTYPET:
					case SA_SETTYPEI:
					case SA_SETTYPEJ:
					case SA_SETTYPEL:
					case SA_SETTYPEM:
					case SA_SETTYPEX:
					case SA_SETTYPEY:
					case SA_SETTYPEN:
					case SA_SETTYPEO:
					case SA_SETTYPEF:
					case SA_SETTYPED:
					case SA_SETTYPEE:
					case SA_SETTYPEG:
					case SA_SETTYPEZ:
						if (!pexpr) {
							current_params += current_param_modifiers;
							current_params += current_char;
						}
						current_name += current_char;
						builtin = true;
						break;
					case SA_BEGINTEMPL:
						++current_recursion;
						//clear current template substitutions
						if (current_recursion == 1 && fname) {
							template_subs.clear();
						}
						//save sub
						if (!subbing && !issub(current_name,substitutions)) {
							substitutions.push_back(current_name);
						}
						name_stack.push(current_name);
						current_name.clear();
						paramm_stack.push(current_param_modifiers);
						current_param_modifiers.clear();
						params_stack.push(current_params);
						current_params.clear();
						arrays_stack.push(arrays);
						arrays.clear();
						break;

					case SA_STORETEMPARG:
						if (!builtin) {
							//save sub
							if (!subbing && !issub(current_name,substitutions)) {
								substitutions.push_back(current_name);
							}
							current_params += current_param_modifiers;
							current_params += "N";
							current_params += current_name;
							current_params += "E";
							current_name = "N"+current_name+"E";
						}

						if (current_recursion == 1 && fname) {
							template_subs.push_back(current_param_modifiers+current_name);
						}

						while (!subbing && !current_param_modifiers.empty()) {
							current_name = current_param_modifiers[current_param_modifiers.length()-1]+current_name;
							if (!current_param_modifiers.empty() && !issub('\0'+current_name,substitutions)) {
								substitutions.push_back('\0'+current_name);
							}
							current_param_modifiers = current_param_modifiers.substr(0,current_param_modifiers.length()-1);
						}

						while(!subbing && !arrays.empty()) {
							if (!issub('\0'+arrays.back()+current_name,substitutions)) {
								substitutions.push_back('\0'+arrays.back()+current_name);
							}
							current_name = arrays.back()+current_name;
							arrays.pop_back();
						}

						current_param_modifiers.clear();
						current_name.clear();
						builtin = false;
						pexpr = false;
						break;

					case SA_TEMPL2TFTPL:
					case SA_STORETEMPLATE:
						if (!builtin) {
							//save sub
							if (!subbing && !issub(current_name,substitutions)) {
								substitutions.push_back(current_name);
							}
							current_params += current_param_modifiers;
							current_params += "N";
							current_params += current_name;
							current_params += "E";
							current_name = "N"+current_name+"E";
						}

						if (current_recursion == 1 && fname) {
							template_subs.push_back(current_param_modifiers+current_name);
						}

						while (!subbing && !current_param_modifiers.empty()) {
							current_name = current_param_modifiers[current_param_modifiers.length()-1]+current_name;
							if (!current_param_modifiers.empty() && !issub('\0'+current_name,substitutions)) {
								substitutions.push_back('\0'+current_name);
							}
							current_param_modifiers = current_param_modifiers.substr(0,current_param_modifiers.length()-1);
						}

						while(!subbing && !arrays.empty()) {
							if (!issub('\0'+arrays.back()+current_name,substitutions)) {
								substitutions.push_back('\0'+arrays.back()+current_name);
							}
							current_name = arrays.back()+current_name;
							arrays.pop_back();
						}

						current_param_modifiers.clear();
						current_name.clear();
						pexpr = false;
						builtin = false;

						current_name = name_stack.top();
						name_stack.pop();
						current_name += "I";
						current_name += current_params;
						current_name += "E";

						current_param_modifiers = paramm_stack.top();
						paramm_stack.pop();
						arrays = arrays_stack.top();
						arrays_stack.pop();
						current_params = params_stack.top();
						params_stack.pop();

						current_recursion--;
						if (subbing && (sub_recursion == current_recursion)) {
							switch(current_st) {
								case ST_STUNQ:
									current_part += "E";
									break;
								case ST_SSNO:
									current_part = "N" + current_part + "E";
									break;
								default:
									break;
							}
							subbing = false;
							current_st = ST_NULL;
							current_sub.clear();
							current_retvalue = current_retvalue + current_part + current_input.substr(position);
							last_rule = true;
							subchange = true;
						}

						break;

					case SA_SKIPTEMPL:
						if (subbing && (sub_recursion == current_recursion)) {
							switch(current_st) {
								case ST_STUNQ:
									current_part += "E";
									break;
								case ST_SSNO:
									if (current_sub[0] == '\0') {
										current_part = current_sub.substr(1);
									}
									else {
										current_part = "N" + current_part + "E";
									}
									break;
								default:
									break;
							}
							subbing = false;
							current_st = ST_NULL;
							current_sub.clear();
							current_retvalue = current_retvalue + current_part + current_input.substr(position);
							last_rule = true;
							subchange = true;
						}
						break;

					case SA_PAR2F:
						if (!builtin) {
							//save sub
							if (!subbing && !issub(current_name,substitutions)) {
								substitutions.push_back(current_name);
							}
							current_params += current_param_modifiers;
							current_params += "N";
							current_params += current_name;
							current_params += "E";
							current_name = "N"+current_name+"E";
						}
						while (!subbing && !current_param_modifiers.empty()) {
							current_name = current_param_modifiers[current_param_modifiers.length()-1]+current_name;
							if (!current_param_modifiers.empty() && !issub('\0'+current_name,substitutions)) {
								substitutions.push_back('\0'+current_name);
							}
							current_param_modifiers = current_param_modifiers.substr(0,current_param_modifiers.length()-1);
						}

						while(!subbing && !arrays.empty()) {
							if (!issub('\0'+arrays.back()+current_name,substitutions)) {
								substitutions.push_back('\0'+arrays.back()+current_name);
							}
							current_name = arrays.back()+current_name;
							arrays.pop_back();
						}

						current_param_modifiers.clear();
						current_name.clear();
						builtin = false;
						break;

					//parameter modifiers
					case SA_SETCONST:
					case SA_SETRESTRICT:
					case SA_SETVOLATILE:
					case SA_SETPTR:
					case SA_SETREF:
					case SA_SETRVAL:
					case SA_SETCPAIR:
					case SA_SETIM:
						current_param_modifiers += current_char;
						break;

					case SA_STUNQ:
						if (!subbing) {
							current_st = ST_STUNQ;
						}
						break;
					case SA_SSNEST:
						if (!subbing) {
							current_st = ST_SSNEST;
						}
						break;
					case SA_SSNO:
						if (!subbing) {
							current_st = ST_SSNO;
						}
						break;

					case SA_LOADTSUB:
						current_id_length = 0;
						while (!(elemstack.top().type == GE_TERM && elemstack.top().t == '_')) {
							elemstack.pop();
						}
						elemstack.pop();

						if (current_input[position] == '_') {
							current_templ_sub = template_subs[0];
						}
						else {
							while (current_input[position] != '_') {
								if (position == current_input.length()) {
									errString = "cGram::subanalyze: Syntax error: Unexpected end of template substitution.";
									*err = ERROR_SYN;
									break;
								}
								if (!isdigit(current_input[position])) {
									errString = string("") + "cGram::subanalyze: Syntax error: Unknown template sub character " + current_input[position] + ".";
									*err = ERROR_SYN;
									break;
								}

								if (subbing) {
									current_part += current_input[position];
								}
								current_id_length *= 10;
								current_id_length += current_input[position] - '0';
								++position;
							}
							if (*err != ERROR_OK) {
								break;
							}
							current_templ_sub = template_subs[current_id_length+1];
						}
						if (subbing) {
							current_part += '_';
						}
						++position;
						if (!subbing) {
							substitutions.push_back('\0'+current_templ_sub);
							current_retvalue = current_part.substr(0,current_part.length()-1) + current_templ_sub + current_input.substr(position);
							last_rule = true;
							subchange = true;
						}

						break;

					case SA_LOADSUB:
						current_sub_id.clear();
						while (!(elemstack.top().type == GE_TERM && elemstack.top().t == '_')) {
							elemstack.pop();
						}
						elemstack.pop();

						if (!subbing) {
							current_retvalue = current_part.substr(0,current_part.length()-1);
							current_part.clear();
						}

						while (current_input[position] != '_') {
							if (position == current_input.length()) {
								errString = "cGram::subanalyze: Syntax error: Unexpected end of substitution.";
								*err = ERROR_SYN;
								break;
							}
							if (!((current_input[position] >= '0' && current_input[position] <= '9') || (current_input[position] >= 'A' && current_input[position] <= 'Z'))) {
								errString = string("") + "cGram::subanalyze: Syntax error: Unknown sub ID symbol " + current_input[position] + ".";
								*err = ERROR_SYN;
								break;
							}

							if (subbing) {
								current_part += current_input[position++];
							}
							else {
								current_sub_id += current_input[position++];
							}
						}
						if (*err != ERROR_OK) {
							break;
						}

						if (subbing) {
							current_part += '_';
						}
						++position;

						if (!subbing) {
							subbing = true;
							sub_recursion = current_recursion;
#ifdef DEMANGLER_SUBDBG
							cout << "Iteration sub candidates:" << endl;
							showsubs(substitutions);
#endif

							//Fix of the bug #914
							unsigned tempPos = b36toint(current_sub_id);
							if ((tempPos+1) >= substitutions.size()) {
								errString = string("") + "cGram::subanalyze: Syntax error: Non-existent substitution " + current_sub_id + ".";
								*err = ERROR_SYN;
								break;
							}

							current_sub = substitutions[tempPos+1];
							switch(current_st) {
								case ST_STUNQ:
									current_part = "N" + current_sub;
									break;
								case ST_SSNO:
									current_part = current_sub;
									break;
								default:
									current_part = current_sub;
									subbing = false;
									current_st = ST_NULL;
									current_sub.clear();
									current_retvalue = current_retvalue + current_part + current_input.substr(position);
									last_rule = true;
									subchange = true;
									break;
							}
						}

						//synchronize current_char to current_input[position]
						if (position == current_input.length()) {
							current_char = '\0';
						}
						else {
							current_char = current_input[position];
						}
						break;

					case SA_SUBSTD:
						if (!subbing) {
							elemstack.pop();
							++position;
							subbing = true;
							current_retvalue = current_part.substr(0,current_part.length()-1);
							current_part.clear();
							sub_recursion = current_recursion;

							current_sub = "3std";
							switch(current_st) {
								case ST_STUNQ:
									current_part = "N" + current_sub;
									break;
								case ST_SSNO:
									current_part = current_sub;
									break;
								default:
									current_part = current_sub;
									subbing = false;
									current_st = ST_NULL;
									current_sub.clear();
									current_retvalue = current_retvalue + current_part + current_input.substr(position);
									last_rule = true;
									subchange = true;
									break;
							}
						}
						break;

					case SA_SUBALC:
						if (!subbing) {
							elemstack.pop();
							++position;
							subbing = true;
							current_retvalue = current_part.substr(0,current_part.length()-1);
							current_part.clear();
							sub_recursion = current_recursion;

							current_sub = "3std9allocator";
							switch(current_st) {
								case ST_STUNQ:
									current_part = "N" + current_sub;
									break;
								case ST_SSNO:
									current_part = current_sub;
									break;
								default:
									current_part = current_sub;
									subbing = false;
									current_st = ST_NULL;
									current_sub.clear();
									current_retvalue = current_retvalue + current_part + current_input.substr(position);
									last_rule = true;
									subchange = true;
									break;
							}
						}
						break;
					case SA_SUBSTR:
						if (!subbing) {
							//get rid of the second Substitution character
							++position;
							elemstack.pop();
							subbing = true;
							current_retvalue = current_part.substr(0,current_part.length()-1);
							current_part.clear();
							sub_recursion = current_recursion;

							current_sub = "3std6string";
							switch(current_st) {
								case ST_STUNQ:
									current_part = "N" + current_sub;
									break;
								case ST_SSNO:
									current_part = current_sub;
									break;
								default:
									current_part = current_sub;
									subbing = false;
									current_st = ST_NULL;
									current_sub.clear();
									current_retvalue = current_retvalue + current_part + current_input.substr(position);
									last_rule = true;
									subchange = true;
									break;
							}
						}
						break;
					case SA_SUBSTRS:
						if (!subbing) {
							elemstack.pop();
							++position;
							subbing = true;
							current_retvalue = current_part.substr(0,current_part.length()-1);
							current_part.clear();
							sub_recursion = current_recursion;

							if (current_input.length() >= position && (current_input[position] == 'C' || current_input[position] == 'D')) {
								current_sub = "3std12basic_stringIcN3std11char_traitsIcEEN3std9allocatorIcEEE";
							}
							else {
								current_sub = "3std6string";
							}
							switch(current_st) {
								case ST_STUNQ:
									current_part = "N" + current_sub;
									break;
								case ST_SSNO:
									current_part = current_sub;
									break;
								default:
									current_part = current_sub;
									subbing = false;
									current_st = ST_NULL;
									current_sub.clear();
									current_retvalue = current_retvalue + current_part + current_input.substr(position);
									last_rule = true;
									subchange = true;
									break;
							}
						}
						break;
					case SA_SUBISTR:
						if (!subbing) {
							elemstack.pop();
							++position;
							subbing = true;
							current_retvalue = current_part.substr(0,current_part.length()-1);
							current_part.clear();
							sub_recursion = current_recursion;

							current_sub = "3std7istream";
							switch(current_st) {
								case ST_STUNQ:
									current_part = "N" + current_sub;
									break;
								case ST_SSNO:
									current_part = current_sub;
									break;
								default:
									current_part = current_sub;
									subbing = false;
									current_st = ST_NULL;
									current_sub.clear();
									current_retvalue = current_retvalue + current_part + current_input.substr(position);
									last_rule = true;
									subchange = true;
									break;
							}
						}
						break;
					case SA_SUBOSTR:
						if (!subbing) {
							elemstack.pop();
							++position;
							subbing = true;
							current_retvalue = current_part.substr(0,current_part.length()-1);
							current_part.clear();
							sub_recursion = current_recursion;

							current_sub = "3std7ostream";
							switch(current_st) {
								case ST_STUNQ:
									current_part = "N" + current_sub;
									break;
								case ST_SSNO:
									current_part = current_sub;
									break;
								default:
									current_part = current_sub;
									subbing = false;
									current_st = ST_NULL;
									current_sub.clear();
									current_retvalue = current_retvalue + current_part + current_input.substr(position);
									last_rule = true;
									subchange = true;
									break;
							}
						}
						break;
					case SA_SUBIOSTR:
						if (!subbing) {
							elemstack.pop();
							++position;
							subbing = true;
							current_retvalue = current_part.substr(0,current_part.length()-1);
							current_part.clear();
							sub_recursion = current_recursion;

							current_sub = "3std8iostream";
							switch(current_st) {
								case ST_STUNQ:
									current_part = "N" + current_sub;
									break;
								case ST_SSNO:
									current_part = current_sub;
									break;
								default:
									current_part = current_sub;
									subbing = false;
									current_st = ST_NULL;
									current_sub.clear();
									current_retvalue = current_retvalue + current_part + current_input.substr(position);
									last_rule = true;
									subchange = true;
									break;
							}
						}
						break;
					case SA_BEGINPEXPR:
						current_name = 'L';
						pexpr = true;
						break;

					case SA_EXPRVAL:
						current_name += current_char;
						break;

					case SA_STOREPEXPR:
						current_name += 'E';
						current_params += current_name;
						current_name.clear();
						current_param_modifiers.clear();
						pexpr = false;
						break;



					//else do nothing
					default:
						break;
				}
			}
			//top of stack is a terminal
			else {
				if (current_element.t == current_char) {
					//just move to next char
					current_part += current_char;
					++position;
				}
				else {
					errString = string("") + "cGram::subanalyze: Syntax error: Unexpected terminal " + current_char + ". Expected was " + current_element.t + ".";
					*err = ERROR_SYN;
					break;
				}
			}
		} //while
#ifdef DEMANGLER_SUBDBG
		cout << current_retvalue << endl;
#endif
		current_input = current_retvalue;
	}
	retvalue = current_retvalue;






#ifdef DEMANGLER_SUBDBG
	showsubs(substitutions);
#endif
	return retvalue;
}

/**
 * @brief The main syntactical and semantical analyzer.
 * @param input The mangled name to be demangled.
 * @param pName Reference to an existing object of cName class into which the demangled name will be stored.
 * @return Error code. Anything else than ERROR_OK means an error has happened.
 */
cGram::errcode cGram::analyze(string input, cName & pName) {
	errcode retvalue = ERROR_OK;
	bool last_rule = false;
	bool rettype = false;
	bool btypesub = false;
	stack<gelem_t> elemstack;
	size_t position = 0;
	bool tempbool = false; //a bool variable for temporary use

	//grammar element from the top of stack
	gelem_t current_element;
	//vector of rules used during the analysis
	vector<unsigned int> used_rules;
	//currently used rule. contains the semantic action as well
	llelem_t current_rule;
	//currently built parameter
	cName::type_t current_param;
	pName.type_t_clear(current_param);
	//current vector of parameters
	vector<cName::type_t> current_param_vector;
	vector<cName::name_t> name_substitution_vector;
	vector<cName::type_t> type_substitution_vector;

	//stacks to store current state when entering a new level of recursion
	stack<cName::type_t> param_stack;
	stack<vector<cName::type_t>> param_vector_stack;
	stack<vector<cName::name_t>> name_vector_stack;
	stack<vector<cName::name_t>> name_substitution_stack;
	stack<vector<cName::type_t>> type_substitution_stack;
	stack<bool> btypesub_stack;


	//current unqualified name
	cName::name_t current_unq_name;
	current_unq_name.tpl = nullptr;
	current_unq_name.op = false;
	//current qualified name
	vector<cName::name_t> current_name;

	//id length used for loading IDs
	unsigned int current_id_length = 0;

	//integer used for adressing type substitutions (in Borland)
	unsigned int bsubtemp = 0;

	long int current_number = 0;

	//default name type is function
	pName.setnametype(cName::NT_FUNCTION);

	//current symbol of the analyzed string.
	char current_char = input[position];

	//insert the root NT into the stack
	if (internalGrammar) {
		elemstack.push(internalGrammarStruct.root);
	}
	else {
		elemstack.push(rules[0].left);
	}

	//the main loop
	while (!last_rule && retvalue == ERROR_OK) {
		if (elemstack.empty()) {
				errString = "cGram::analyze: Syntax error: elemstack empty ";
				retvalue = ERROR_SYN;
				break;
		}

		//load the current character of the input string
		if (position == input.length()) {
			current_char = '\0';
		}
		else {
			current_char = input[position];
		}
		current_element = elemstack.top();
		elemstack.pop();
		//top of stack is a non-terminal
		if (current_element.type == GE_NONTERM) {
			//load the rule number for current NT and T and check for syntax error
			if ((current_rule = getllpair(current_element.nt, current_element.ntst, current_char)).n == 0) {
				errString = string("") + "cGram::analyze: Syntax error: No rule for NT " + current_element.nt + " and T " + current_char + ".";
				retvalue = ERROR_SYN;
				break;
			}
			//add the currently used rule to the vector of used rules
			else {
				used_rules.push_back(current_rule.n);
			}

			//internal grammar
			if (internalGrammar) {
				for(unsigned int i=internalGrammarStruct.ruleaddrs[current_rule.n-1].size; i>0; i--) {
					elemstack.push(internalGrammarStruct.ruleelements[internalGrammarStruct.ruleaddrs[current_rule.n-1].offset + (i-1)]);
				}
			}
			//external grammar
			else {
				//push right side of the used rule into the stack
				for(vector<gelem_t>::reverse_iterator i = rules[current_rule.n-1].right.rbegin(); i != rules[current_rule.n-1].right.rend(); ++i) {
					elemstack.push(*i);
				}
			}

			//temporary name_t variable for storing template pointer into last element of current_name
			cName::name_t tempname;
			//semantic action
			switch (current_rule.s) {
				//do nothing
				case SA_NULL:
					break;

				//type of name modifiers
				//set name type to constructor
				case SA_SETNAMEC:
					pName.setnametype(cName::NT_CONSTRUCTOR);
					break;
				//set name type to destructor
				case SA_SETNAMED:
					pName.setnametype(cName::NT_DESTRUCTOR);
					break;
				//set name type to data
				case SA_SETNAMEX:
					pName.setnametype(cName::NT_DATA);
					break;
				//set name type to operator
				case SA_SETNAMEO:
					pName.setnametype(cName::NT_OPERATOR);
					current_unq_name.tpl = nullptr;
					current_unq_name.un = "operator";
					current_unq_name.op = true;
					break;

				//built-in types
				case SA_SETTYPEV:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_VOID;
					break;
				case SA_SETTYPEW:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_WCHAR;
					break;
				case SA_SETTYPEB:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_BOOL;
					break;
				case SA_SETTYPEC:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_CHAR;
					break;
				case SA_SETTYPEA:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_SCHAR;
					break;
				case SA_SETTYPEH:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_UCHAR;
					break;
				case SA_SETTYPES:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_SHORT;
					break;
				case SA_SETTYPET:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_USHORT;
					break;
				case SA_SETTYPEI:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_INT;
					break;
				case SA_SETTYPEJ:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_UINT;
					break;
				case SA_SETTYPEL:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_LONG;
					break;
				case SA_SETTYPEM:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_ULONG;
					break;
				case SA_SETTYPEX:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_LONGLONG;
					break;
				case SA_SETTYPEY:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_ULONGLONG;
					break;
				case SA_SETTYPEN:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_INT128;
					break;
				case SA_SETTYPEO:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_UINT128;
					break;
				case SA_SETTYPEF:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_FLOAT;
					break;
				case SA_SETTYPED:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_DOUBLE;
					break;
				case SA_SETTYPEE:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_LONGDOUBLE;
					break;
				case SA_SETTYPEG:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_FLOAT128;
					break;
				case SA_SETTYPEZ:
					current_param.type = cName::TT_BUILTIN;
					current_param.n.clear();
					current_param.b = cName::T_ELLIPSIS;
					break;



				//set operator type
				case SA_SETOPNW:
					pName.setop(cName::OT_NEW);
					current_unq_name.un += " new";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPNA:
					pName.setop(cName::OT_NEWARR);
					current_unq_name.un += " new[]";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPDL:
					pName.setop(cName::OT_DEL);
					current_unq_name.un += " delete";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPDA:
					pName.setop(cName::OT_DELARR);
					current_unq_name.un += " delete[]";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPPS:
					pName.setop(cName::OT_UPLUS);
					current_unq_name.un += "+";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPNG:
					pName.setop(cName::OT_UMINUS);
					current_unq_name.un += "-";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPAD:
					pName.setop(cName::OT_UAND);
					current_unq_name.un += "&";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPDE:
					pName.setop(cName::OT_UAST);
					current_unq_name.un += "*";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPCO:
					pName.setop(cName::OT_TILDA);
					current_unq_name.un += "~";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPPL:
					pName.setop(cName::OT_PLUS);
					current_unq_name.un += "+";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPMI:
					pName.setop(cName::OT_MINUS);
					current_unq_name.un += "-";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPML:
					pName.setop(cName::OT_AST);
					current_unq_name.un += "*";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPDV:
					pName.setop(cName::OT_DIV);
					current_unq_name.un += "/";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPRM:
					pName.setop(cName::OT_MOD);
					current_unq_name.un += "%";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPAN:
					pName.setop(cName::OT_AND);
					current_unq_name.un += "&";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPOR:
					pName.setop(cName::OT_OR);
					current_unq_name.un += "|";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPEO:
					pName.setop(cName::OT_EXP);
					current_unq_name.un += "^";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPASS:
					pName.setop(cName::OT_ASSIGN);
					current_unq_name.un += "=";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPPLL:
					pName.setop(cName::OT_PLUSASS);
					current_unq_name.un += "+=";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPMII:
					pName.setop(cName::OT_MINUSASS);
					current_unq_name.un += "-=";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPMLL:
					pName.setop(cName::OT_ASTASS);
					current_unq_name.un += "*=";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPDVV:
					pName.setop(cName::OT_DIVASS);
					current_unq_name.un += "/=";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPRMM:
					pName.setop(cName::OT_MODASS);
					current_unq_name.un += "%=";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPANN:
					pName.setop(cName::OT_ANDASS);
					current_unq_name.un += "&=";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPORR:
					pName.setop(cName::OT_ORASS);
					current_unq_name.un += "|=";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPEOO:
					pName.setop(cName::OT_EXPASS);
					current_unq_name.un += "^=";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPLS:
					pName.setop(cName::OT_LSHIFT);
					current_unq_name.un += "<<";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPRS:
					pName.setop(cName::OT_RSHIFT);
					current_unq_name.un += ">>";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPLSS:
					pName.setop(cName::OT_LSHIFTASS);
					current_unq_name.un += "<<=";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPRSS:
					pName.setop(cName::OT_RSHIFTASS);
					current_unq_name.un += ">>=";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPEQ:
					pName.setop(cName::OT_EQ);
					current_unq_name.un += "==";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPNE:
					pName.setop(cName::OT_NEQ);
					current_unq_name.un += "!=";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPLT:
					pName.setop(cName::OT_LT);
					current_unq_name.un += "<";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPGT:
					pName.setop(cName::OT_GT);
					current_unq_name.un += ">";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPLE:
					pName.setop(cName::OT_LE);
					current_unq_name.un += "<=";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPGE:
					pName.setop(cName::OT_GE);
					current_unq_name.un += ">=";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPNT:
					pName.setop(cName::OT_NOT);
					current_unq_name.un += "!";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPAA:
					pName.setop(cName::OT_ANDAND);
					current_unq_name.un += "&&";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPOO:
					pName.setop(cName::OT_OROR);
					current_unq_name.un += "||";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPPP:
					pName.setop(cName::OT_PLUSPLUS);
					current_unq_name.un += "++";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPMM:
					pName.setop(cName::OT_MINUSMINUS);
					current_unq_name.un += "--";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPCM:
					pName.setop(cName::OT_COMMA);
					current_unq_name.un += ",";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPPM:
					pName.setop(cName::OT_PTAST);
					current_unq_name.un += "->*";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPPT:
					pName.setop(cName::OT_PT);
					current_unq_name.un += "->";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPCL:
					pName.setop(cName::OT_BRACKETS);
					current_unq_name.un += "()";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPIX:
					pName.setop(cName::OT_ARR);
					current_unq_name.un += "[]";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPQU:
					pName.setop(cName::OT_QUESTION);
					current_unq_name.un += "?";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPST:
					pName.setop(cName::OT_SIZEOFT);
					current_unq_name.un += " sizeof";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPSZ:
					pName.setop(cName::OT_SIZEOFE);
					current_unq_name.un += " sizeof";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPAT:
					pName.setop(cName::OT_ALIGNOFT);
					current_unq_name.un += " alignof";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPAZ:
					pName.setop(cName::OT_ALIGNOFE);
					current_unq_name.un += " alignof";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;
				case SA_SETOPCV:
					pName.setop(cName::OT_CAST);
					current_unq_name.un += " ";
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.op = false;
					break;

				//parameter modifiers
				case SA_SETCONST:
					current_param.modifiers = "K"+current_param.modifiers;
					break;
				case SA_SETRESTRICT:
					current_param.modifiers = "r"+current_param.modifiers;
					break;
				case SA_SETVOLATILE:
					current_param.modifiers = "V"+current_param.modifiers;
					break;
				case SA_SETPTR:
					current_param.modifiers = "P"+current_param.modifiers;
					break;
				case SA_SETREF:
					current_param.modifiers = "R"+current_param.modifiers;
					break;
				case SA_SETRVAL:
					current_param.modifiers = "O"+current_param.modifiers;
					break;
				case SA_SETCPAIR:
					current_param.modifiers = "C"+current_param.modifiers;
					break;
				case SA_SETIM:
					current_param.modifiers = "G"+current_param.modifiers;
					break;

				//start a template, enter a new level of recursion
				case SA_BEGINTEMPL:
					//push everything into stacks and clean up
					name_vector_stack.push(current_name);
					current_name.clear();

					name_substitution_stack.push(name_substitution_vector);
					name_substitution_vector.clear();
					type_substitution_stack.push(type_substitution_vector);
					type_substitution_vector.clear();

					btypesub_stack.push(btypesub);
					btypesub = false;

					param_stack.push(current_param);
					pName.type_t_clear(current_param);

					param_vector_stack.push(current_param_vector);
					current_param_vector.clear();
					break;

				//template ends, store all of its parameters and quit current level of recursion
				case SA_STORETEMPLATE:
					//if the param type was named, move the name to the param
					if (!current_name.empty()) {
						current_param.type = cName::TT_NAME;
						current_param.n = current_name;
						current_name.clear();
					}

					//save the current param's substitution
					if (btypesub) {
						if (type_substitution_vector.size() < BSUBMAX) {
							type_substitution_vector.push_back(current_param);
						}
						btypesub = false;
					}
					//move the param to current vector of params
					current_param_vector.push_back(current_param);
					pName.type_t_clear(current_param);

					//restore previous 'current_param' from the stack
					current_param = param_stack.top();
					param_stack.pop();

					//restore previous 'current qualified name' from the stack
					current_name = name_vector_stack.top();
					name_vector_stack.pop();

					//restore previous btypesub state
					btypesub = btypesub_stack.top();
					btypesub_stack.pop();

					//restore previous substitution name vector
					name_substitution_vector = name_substitution_stack.top();
					name_substitution_stack.pop();
					type_substitution_vector = type_substitution_stack.top();
					type_substitution_stack.pop();

					//insert current template into the last name element of current qualified name
					if (current_name.empty()) {
						errString = "Fatal error: Current name is empty!!";
						retvalue = ERROR_SYN;
						break;
					}
					else {
						tempname = current_name.back();
						current_name.pop_back();
						tempname.tpl = static_cast<void *>(new vector<cName::type_t>(current_param_vector));
						current_name.push_back(tempname);
					}

					//restore previous 'current vector of parameters' from the stack
					current_param_vector = param_vector_stack.top();
					param_vector_stack.pop();
					break;

				case SA_TEMPL2TFTPL:
					//if the param type was named, move the name to the param
					if (!current_name.empty()) {
						current_param.type = cName::TT_NAME;
						current_param.n = current_name;
						current_name.clear();
					}

					//save the current param's substitution
					if (btypesub) {
						if (type_substitution_vector.size() < BSUBMAX) {
							type_substitution_vector.push_back(current_param);
						}
						btypesub = false;
					}
					//move the param to current vector of params
					current_param_vector.push_back(current_param);
					pName.type_t_clear(current_param);

					//restore previous 'current_param' from the stack
					current_param = param_stack.top();
					param_stack.pop();

					//restore previous btypesub state
					btypesub = btypesub_stack.top();
					btypesub_stack.pop();

					//restore previous 'current qualified name' from the stack
					current_name = name_vector_stack.top();
					name_vector_stack.pop();

					//restore previous substitution name vector
					name_substitution_vector = name_substitution_stack.top();
					name_substitution_stack.pop();
					type_substitution_vector = type_substitution_stack.top();
					type_substitution_stack.pop();

					//insert current template into the last name element of current qualified name
					if (current_name.empty()) {
						errString = "Fatal error: Current name is empty!!";
						retvalue = ERROR_SYN;
						break;
					}
					else {
						tempname.tpl = static_cast<void *>(new vector<cName::type_t>(current_param_vector));
						pName.settftpl(tempname.tpl);
						tempname.tpl = nullptr;
					}

					//restore previous 'current vector of parameters' from the stack
					current_param_vector = param_vector_stack.top();
					param_vector_stack.pop();
					break;

				//store current template argument into the current vector of params
				case SA_STORETEMPARG:
					//if the param type was named, move the name to the param
					if (!current_name.empty()) {
						current_param.type = cName::TT_NAME;
						current_param.n = current_name;
						current_name.clear();
					}

					//save the current param's substitution
					if (btypesub) {
						if (type_substitution_vector.size() < BSUBMAX) {
							type_substitution_vector.push_back(current_param);
						}
						btypesub = false;
					}
					//move the param to current vector of params
					current_param_vector.push_back(current_param);
					pName.type_t_clear(current_param);
					break;

				//store current function parameter
				case SA_STOREPAR:
					//if the param type was named, move the name to the param
					if (!current_name.empty()) {
						current_param.type = cName::TT_NAME;
						current_param.n = current_name;
						current_name.clear();
					}
					//move the param to current vector of params
					if (rettype) {
						pName.setret(current_param);
						rettype = false;
					}
					else {
					//save the current param's substitution
					if (btypesub) {
						if (type_substitution_vector.size() < BSUBMAX) {
							type_substitution_vector.push_back(current_param);
						}
						btypesub = false;
					}
						current_param_vector.push_back(current_param);
					}
					pName.type_t_clear(current_param);
					break;

				case SA_LOADARR:
					//remove the number terminal and "_" from element stack
					elemstack.pop();
					elemstack.pop();
					current_id_length = 0;

					while (input[position] != '_') {
						if (position == input.length()) {
							errString = "cGram::analyze: Syntax error: Unexpected end of array.";
							retvalue = ERROR_SYN;
							break;
						}
						if (!isdigit(input[position])) {
							errString = string("") + "cGram::analyze: Syntax error: Unknown array symbol " + input[position] + ".";
							retvalue = ERROR_SYN;
							break;
						}
						current_id_length *= 10;
						current_id_length += input[position] - '0';
						++position;
					}
					if (retvalue != ERROR_OK) {
						break;
					}
					//_
					++position;

					current_param.is_array = true;
					current_param.array_dimensions.push_back(current_id_length);
					break;

				//loads one unqualified name into current qualified name
				case SA_LOADID:
					//remove the number terminal from element stack
					elemstack.pop();
					current_id_length = 0;
					//load the length of ID
					while (isdigit(input[position])) {
						if (position == input.length()) {
							errString = "cGram::analyze: Syntax error: Unexpected end of identifier length.";
							retvalue = ERROR_SYN;
							break;
						}
						current_id_length *= 10;
						current_id_length += (input[position++] - '0');
					}
					if (retvalue != ERROR_OK) {
						break;
					}

					//load the ID
					for (unsigned int i = 0; i < current_id_length; ++i) {
						if (position == input.length()) {
							errString = string("") + "cGram::analyze: Syntax error: Unexpected end of identifier " + current_unq_name.un + ".";
							retvalue = ERROR_SYN;
							break;
						}
						current_unq_name.un += input[position++];
					}
					if (retvalue != ERROR_OK) {
						break;
					}

					//move the ID into current qualified name
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.tpl = nullptr;

					//synchronize current_char to input[position]
					if (position == input.length()) {
						//if the ID was the last element in the mangled name, continue on to SA_PAR2F
						current_char = '\0';
					}
					else {
						current_char = input[position];
					}
					break;

				case SA_ADDCHARTONAME:
					current_unq_name.un += current_char;
				break;

				case SA_STORENAME:
					current_name.push_back(current_unq_name);
					current_unq_name.un.clear();
					current_unq_name.tpl = nullptr;
				break;

				//move the current vector of parameters into function parameter vector
				case SA_PAR2F:
					//if the param type was named, move the name to the param
					if (!current_name.empty()) {
						current_param.type = cName::TT_NAME;
						current_param.n = current_name;
						current_name.clear();
					}
					//save the current param's substitution
					if (btypesub) {
						if (type_substitution_vector.size() < BSUBMAX) {
							type_substitution_vector.push_back(current_param);
						}
						btypesub = false;
					}
					//move the param to current vector of params
					current_param_vector.push_back(current_param);
					pName.type_t_clear(current_param);

					//insert the param vector into the demangled name as the function parameters
					pName.addpar(current_param_vector);
					current_param_vector.clear();
					break;

				//save current qualified name into the function name
				case SA_UNQ2F:
					if (!current_name.empty()) {
						//if the name is a template function which is not a constructor or destructor, the first parameter will be the return type
						if (current_name.back().tpl != nullptr && pName.getnametype() != cName::NT_CONSTRUCTOR && pName.getnametype() != cName::NT_DESTRUCTOR) {
							rettype = true;
						}
						pName.addname(current_name);
						current_name.clear();
						if (!current_param.modifiers.empty()) {
							pName.setmodifiers(current_param.modifiers);
							current_param.modifiers.clear();
						}
					}
					break;

				case SA_UNQ2P:
					current_param.type = cName::TT_NAME;
					current_param.n = current_name;
					current_name.clear();
					break;

				//store current parameter as the return type
				case SA_PAR2SPEC:
					pName.setspec(current_param);
					pName.type_t_clear(current_param);
					btypesub = false;
					break;
				case SA_PAR2RET:
					pName.setret(current_param);
					pName.type_t_clear(current_param);
					btypesub = false;
					break;

				case SA_TYPE2EXPR:
					current_param.type = cName::TT_PEXPR;
					break;
				case SA_EXPRVAL:
					if (!isdigit(current_char)) {
						errString = string("") + "cGram::analyze: Syntax error: Unknown expression value symbol " + input[position] + ".";
						retvalue = ERROR_SYN;
						break;
					}
					if (current_param.b == cName::T_BOOL) {
						current_param.value = malloc(sizeof(bool));
						if (current_param.value == nullptr) {
							errString = string("") + "cGram::analyze: Syntax error: Couldn't allocate memory for bool expression value " + input[position] + ".";
							retvalue = ERROR_MEM;
							break;
						}
						*(static_cast<bool*>(current_param.value)) = current_char - '0';
					}
					elemstack.pop();
					++position;
					break;

				case SA_REVERSENAME:
					reverse(current_name.begin(), current_name.end());
					break;

				case SA_SETPRIVATE:
					pName.setmfacc(cName::MFM_PRIVATE);
					break;
				case SA_SETPUBLIC:
					pName.setmfacc(cName::MFM_PUBLIC);
					break;
				case SA_SETPROTECTED:
					pName.setmfacc(cName::MFM_PROTECTED);
					break;
				case SA_SETFCDECL:
					pName.setfcall(cName::FCC_CDECL);
					break;
				case SA_SETFPASCAL:
					pName.setfcall(cName::FCC_PASCAL);
					break;
				case SA_SETFFORTRAN:
					pName.setfcall(cName::FCC_FORTRAN);
					break;
				case SA_SETFTHISCALL:
					pName.setfcall(cName::FCC_THISCALL);
					break;
				case SA_SETFSTDCALL:
					pName.setfcall(cName::FCC_STDCALL);
					break;
				case SA_SETFFASTCALL:
					pName.setfcall(cName::FCC_FASTCALL);
					break;
				case SA_SETFINTERRUPT:
					pName.setfcall(cName::FCC_INTERRUPT);
					break;

				case SA_SETUNION:
					current_param.mst = cName::MST_UNION;
					break;
				case SA_SETSTRUCT:
					current_param.mst = cName::MST_STRUCT;
					break;
				case SA_SETCLASS:
					current_param.mst = cName::MST_CLASS;
					break;
				case SA_SETENUM:
					current_param.mst = cName::MST_ENUM;
					break;

				case SA_SETSTATIC:
					pName.setstatic();
					break;
				case SA_SETVIRTUAL:
					pName.setvirtual();
					break;

				case SA_STCLCONST:
					pName.addstcl('K');
					break;
				case SA_STCLVOL:
					pName.addstcl('V');
					break;
				case SA_STCLFAR:
					pName.addstcl('b');
					break;
				case SA_STCLHUGE:
					pName.addstcl('c');
					break;

				case SA_ADDMCONST:
					pName.addmodifier('K');
					break;
				case SA_ADDMVOL:
					pName.addmodifier('V');
					break;
				case SA_ADDMFAR:
					pName.addmodifier('b');
					break;
				case SA_ADDMHUGE:
					pName.addmodifier('c');
					break;

				case SA_SAVENAMESUB:
					if (name_substitution_vector.size() < 10 && !current_name.back().op) {
						name_substitution_vector.push_back(current_name.back());
					}
					rettype = false;
					break;

				case SA_LOADNAMESUB:
					if (static_cast<unsigned int>(current_char - '0') < name_substitution_vector.size()) {
						current_unq_name = name_substitution_vector[static_cast<unsigned int>(current_char - '0')];
						if (current_unq_name.tpl != nullptr) {
							current_unq_name.tpl = copynametpl(current_unq_name.tpl);
							if (current_unq_name.tpl == nullptr) {
								errString = string("") + "cGram::analyze: Error when copying template of name substitution " + current_unq_name.un + ".";
								retvalue = ERROR_MEM;
								break;
							}
						}
						current_name.push_back(current_unq_name);
						current_unq_name.un.clear();
						current_unq_name.tpl = nullptr;
					}
					else {
						errString = string("") + "cGram::analyze: name_substitution_vector does not contain substitution #" + current_char + ".";
						retvalue = ERROR_MEM;
						break;
					}

					break;

				case SA_MSTEMPLPSUB:
					if (name_vector_stack.top().back().op) {
						break;
					}
					name_substitution_vector.push_back(name_vector_stack.top().back());
					break;

				case SA_SETNAMEVT:
					pName.setnametype(cName::NT_VTABLE);
					break;

				case SA_SETNAMER0:
					pName.setnametype(cName::NT_R0);
					break;

				case SA_SETNAMER1:
					pName.setnametype(cName::NT_R1);
					break;

				case SA_SETNAMER2:
					pName.setnametype(cName::NT_R2);
					break;

				case SA_SETNAMER3:
					pName.setnametype(cName::NT_R3);
					break;

				case SA_SETNAMER4:
					pName.setnametype(cName::NT_R4);
					break;

				case SA_SETNAME_A:
					pName.setnametype(cName::NT__A);
					break;

				case SA_SETNAME_B:
					pName.setnametype(cName::NT__B);
					break;

				case SA_SETNAME_C:
					pName.setnametype(cName::NT__C);
					break;

				case SA_SETNAME_D:
					pName.setnametype(cName::NT__D);
					break;

				case SA_SETNAME_E:
					pName.setnametype(cName::NT__E);
					break;

				case SA_SETNAME_F:
					pName.setnametype(cName::NT__F);
					break;

				case SA_SETNAME_G:
					pName.setnametype(cName::NT__G);
					break;

				case SA_SETNAME_H:
					pName.setnametype(cName::NT__H);
					break;

				case SA_SETNAME_I:
					pName.setnametype(cName::NT__I);
					break;

				case SA_SETNAME_J:
					pName.setnametype(cName::NT__J);
					break;

				case SA_SETNAME_K:
					pName.setnametype(cName::NT__K);
					break;

				case SA_SETNAME_L:
					pName.setnametype(cName::NT__L);
					break;

				case SA_SETNAME_M:
					pName.setnametype(cName::NT__M);
					break;

				case SA_SETNAME_N:
					pName.setnametype(cName::NT__N);
					break;

				case SA_SETNAME_O:
					pName.setnametype(cName::NT__O);
					break;

				case SA_SETNAME_P:
					pName.setnametype(cName::NT__P);
					break;

				case SA_SETNAME_Q:
					pName.setnametype(cName::NT__Q);
					break;

				case SA_SETNAME_R:
					pName.setnametype(cName::NT__R);
					break;

				case SA_SETNAME_S:
					pName.setnametype(cName::NT__S);
					break;

				case SA_SETNAME_T:
					pName.setnametype(cName::NT__T);
					break;

				case SA_SETNAME_U:
					pName.setnametype(cName::NT__U);
					break;

				case SA_SETNAME_V:
					pName.setnametype(cName::NT__V);
					break;

				case SA_SETNAME_W:
					pName.setnametype(cName::NT__W);
					break;

				case SA_SETNAME_X:
					pName.setnametype(cName::NT__X);
					break;

				case SA_SETNAME_Y:
					pName.setnametype(cName::NT__Y);
					break;

				case SA_SETNAME_Z:
					pName.setnametype(cName::NT__Z);
					break;

				case SA_BEGINBSUB:
					btypesub = true;
					break;

				case SA_LOADBSUB:
					btypesub = false;
					if (static_cast<unsigned int>(current_char - '0') < type_substitution_vector.size()) {
						current_param = type_substitution_vector[static_cast<unsigned int>(current_char - '0')];
						for (vector<cName::name_t>::iterator i=current_param.n.begin(); i != current_param.n.end(); ++i) {
							if (i->tpl != nullptr) {
								i->tpl = copynametpl(i->tpl);
								//if there was an error during allocation, clean up
								if (i->tpl == nullptr) {
								errString = "cGram::analyze: Error when copying template of type substitution.";
								retvalue = ERROR_MEM;
								break;
								}
							}
						}
						break;
					}
					else {
						errString = string("") + "cGram::analyze: type_substitution_vector does not contain substitution #" + current_char + ".";
						retvalue = ERROR_MEM;
						break;
					}

				case SA_LOADBORLANDSUB:
					//bsub won't be disabled -> the newly loaded substitution will be stored as a new substitution again
					//because Borland works that way
					if (current_char >= '1' && current_char <= '9') {
						bsubtemp = static_cast<unsigned int>(current_char - '1');
					}
					else if (current_char >= 'a' && current_char <= 'z') {
						bsubtemp = static_cast<unsigned int>(current_char - 'a') + 9;
					}
					if (bsubtemp < type_substitution_vector.size()) {
						current_param = type_substitution_vector[bsubtemp];
						for (vector<cName::name_t>::iterator i=current_param.n.begin(); i != current_param.n.end(); ++i) {
							if (i->tpl != nullptr) {
								i->tpl = copynametpl(i->tpl);
								//if there was an error during allocation, clean up
								if (i->tpl == nullptr) {
								errString = "cGram::analyze: Error when copying template of type substitution.";
								retvalue = ERROR_MEM;
								break;
								}
							}
						}

						break;
					}
					else {
						errString = string("") + "cGram::analyze: type_substitution_vector does not contain substitution #" + current_char + ".";
						retvalue = ERROR_MEM;
						break;
					}

				case SA_LOADMSNUM:
					//remove the msnumber non-terminal from element stack
					elemstack.pop();
					current_number = 0;

					if (input[position] == '?') {
						tempbool = true;
						++position;
					}
					else {
						tempbool = false;
					}
					if (input[position] >= '0' && input[position] <= '9') {
						current_number = input[position] - '0' + 1;
						++position;
					}
					else if (input[position] >= 'A' && input[position] <= 'P') {
						while (input[position] != '@') {
							if (position == input.length()-1) {
								errString = "cGram::analyze: Syntax error: Unexpected end of MSVC++ number";
								retvalue = ERROR_SYN;
								break;
							}
							if (input[position] >= 'A' && input[position] <= 'P') {
								current_number *= 16;
								current_number += input[position] - 'A';
								++position;
							}
							else {
								errString = string("") + "cGram::analyze: Syntax error: Unexpected character " + input[position] + " instead of a MSVC++ number";
								retvalue = ERROR_SYN;
								break;
							}
						}
						if (retvalue != ERROR_OK) {
							break;
						}
						++position;
					}
					else {
							errString = string("") + "cGram::analyze: Syntax error: Unexpected character " + input[position] + " instead of a MSVC++ number";
							retvalue = ERROR_SYN;
							break;
					}

					if (tempbool) {
						current_number = -current_number;
					}

					break;

				case SA_NUMTORTTIBCD:
					pName.addrttinum(current_number);
					break;

				case SA_NUMTOTYPE:
					current_param.type = cName::TT_NUM;
					current_param.num = current_number;
					break;

				case SA_BORLANDNORMALIZEPARNAME:
					xreplace(current_name.back().un,"@","::");
					break;

				case SA_BORLANDID:
					//remove the number terminal from element stack
					elemstack.pop();
					current_id_length = 0;
					//load the length of ID
					while (isdigit(input[position])) {
						if (position == input.length()) {
							errString = "cGram::analyze: Syntax error: Unexpected end of identifier length.";
							retvalue = ERROR_SYN;
							break;
						}
						current_id_length *= 10;
						current_id_length += (input[position++] - '0');
					}
					if (retvalue != ERROR_OK) {
						break;
					}

					//check if input is long enough for the separator insertion
					if (input.length() < position + current_id_length) {
						errString = string("") + "cGram::analyze: Syntax error: Unexpected end of input when inserting a separator.";
						retvalue = ERROR_SYN;
						break;
					}

					//insert the separator
					input = input.substr(0,position + current_id_length) + "|" + input.substr(position + current_id_length);
					break;

				case SA_BORLANDARR:
					//remove the number terminal from element stack
					elemstack.pop();
					current_id_length = 0;

					while (isdigit(input[position])) {
						if (position == input.length()) {
							errString = "cGram::analyze: Syntax error: Unexpected end of array.";
							retvalue = ERROR_SYN;
							break;
						}
						current_id_length *= 10;
						current_id_length += input[position] - '0';
						++position;
					}
					if (retvalue != ERROR_OK) {
						break;
					}

					//store the array info to the current param
					current_param.is_array = true;
					current_param.array_dimensions.push_back(current_id_length);
					break;

				//last rule - successfully end
				case SA_END:
					last_rule = true;
					break;
				//else do nothing
				default:
					break;
			}
		}
		//top of stack is a terminal
		else {
			if (current_element.t == current_char) {
				//just move to next char
				++position;
			}
			else {
				errString = string("") + "cGram::analyze: Syntax error: Unexpected terminal " + current_char + ". Expected was " + current_element.t + ".";
				retvalue = ERROR_SYN;
				break;
			}
		}
	}

	return retvalue;
}

/**
 * Try to demangle string into class name.
 */
void cGram::demangleClassName(const std::string& input, cName* retvalue, cGram::errcode& err_i)
{
	//C++ class name demangler hack for gcc and msvc.
	if (err_i != ERROR_OK) {
		std::string className;

		//7Polygon
		//14PolygonPolygon
		if (compiler == "gcc") {
			static std::regex rgx("(\\d+)(\\w+)");
			std::smatch match;
			if (std::regex_match(input, match, rgx)) {
				std::string sLength = match[1];
				std::string name = match[2];

				unsigned long length = std::stoul(sLength);
				if (name.length() == length) {
					className = name;
				}
			}
		}
		//.?AVPolygon@@
		//.?AVtype_info@@
		else if (compiler == "ms") {
			static std::regex rgx("\\.\\?AV(\\w+)\\@\\@");
			std::smatch match;
			if (std::regex_match(input, match, rgx)) {
				className = match[1];
			}
		}

		if (!className.empty()) {
			err_i = ERROR_OK;
			retvalue->name_type = cName::NT_CLASS;
			cName::name_t n;
			n.un = className;
			retvalue->name.push_back(n);
		}
	}
}

/**
 * @brief An envelope function for demangling of the input name. Calls the necessary analysis functions.
 * @param input The mangled name to be demangled.
 * @param err Pointer to an errcode into which the error code will be stored.
 * @return Pointer to an object of the cName class containing the demangled name.
 */
cName *cGram::perform(const string input, cGram::errcode *err) {
	cName *retvalue = new cName();
	errcode err_i = ERROR_OK;
	string temp = input;
	//substitution analysis (for now only in GCC)
	if (SubAnalyzeEnabled) {
		temp = subanalyze(temp,&err_i);
	}
#ifdef DEMANGLER_SUBDBG
	cout << temp << endl;
#else
	if (err_i == ERROR_OK) {
		err_i = analyze(temp, *retvalue);
	}
#endif

	demangleClassName(input, retvalue, err_i);

	*err = err_i;
	return retvalue;
}

/**
 * @brief Function which prints grammar rules on stdout.
 */
void cGram::showrules() {
	//for all rules
	for(vector<rule_t>::iterator i=rules.begin(); i != rules.end(); ++i) {
		//left
		cout << i->n << ": <" << i->left.nt << ">";
		cout << " ";
		cout << "::=";
		cout << " ";
		//right
		for(vector<gelem_t>::iterator j=i->right.begin(); j != i->right.end(); ++j) {
			if (j->type == GE_TERM) {
				cout << j->t;
			}
			else {
				cout << "<" << j->nt << ">";
			}
			cout << " ";
		}
		cout << endl;
	}
	cout << endl;
}

/**
 * @brief Function which prints "Empty" set on stdout.
 */
void cGram::showempty() {
	cout << "Empty:" << endl;
	for(map<string,bool>::iterator i=empty.begin(); i != empty.end(); ++i) {
		cout << "<" << i->first << ">" << ": ";
		if (i->second == false) {
			cout << "0";
		}
		else {
			cout << "{e}";
		}
		cout << endl;
	}
	cout << endl;
}

/**
 * @brief Function which prints "First" set on stdout.
 */
void cGram::showfirst() {
	cout << "First:" << endl;
	for(map<string,set<gelem_t,comparegelem_c>>::iterator i=first.begin(); i != first.end(); ++i) {
		cout << "<" << i->first << ">" << ": ";
		for(set<gelem_t,comparegelem_c>::iterator j=i->second.begin(); j != i->second.end(); ++j) {
			cout << j->t << " ";
		}
		cout << endl;
	}
	cout << endl;
}

/**
 * @brief Function which prints "Follow" set on stdout.
 */
void cGram::showfollow() {
	cout << "Follow:" << endl;
	for(map<string,set<gelem_t,comparegelem_c>>::iterator i=follow.begin(); i != follow.end(); ++i) {
		cout << "<" << i->first << ">" << ": ";
		for(set<gelem_t,comparegelem_c>::iterator j=i->second.begin(); j != i->second.end(); ++j) {
			if (j->t == '\0') {
				cout << "EOF";
			}
			else {
				cout << j->t;
			}
			cout << " ";
		}
		cout << endl;
	}
	cout << endl;
}

/**
 * @brief Function which prints "Predict" set on stdout.
 */
void cGram::showpredict() {
	cout << "Predict:" << endl;
	for(vector<rule_t>::iterator i=rules.begin(); i != rules.end(); ++i) {
		cout << i->n << ": ";
		for(set<gelem_t,comparegelem_c>::iterator j=predict[i->n].begin(); j != predict[i->n].end(); ++j) {
			if (j->t == '\0') {
				cout << "EOF";
			}
			else {
				cout << j->t;
			}
			cout << " ";
		}
		cout << endl;
	}
}

/**
 * @brief Function which prints LL table on stdout.
 */
void cGram::showll() {
	cout << "LL:" << endl;
	for(map<string,map<char,pair<unsigned int, semact>>>::iterator i=ll.begin(); i != ll.end(); ++i) {
		cout << "<" << i->first << ">" << ": ";
		for(map<char,pair<unsigned int, semact>>::iterator j=i->second.begin(); j != i->second.end(); ++j) {
			if (j->first == '\0') {
				cout << "EOF";
			}
			else {
				cout << j->first;
			}
			cout << "=";
			cout << j->second.first;
			cout << " ";
		}
		cout << endl;
	}
	cout << endl;
}

/**
 * @brief Function which sets qualified function name of the currently demangled name.
 * @param inname Qualified name consisting of unqualified names.
 */
void cName::addname(const vector<cName::name_t> & inname) {
	name = inname;
}

/**
 * @brief Function which sets parameters of the currently demangled name.
 * @param inpar Vector of parameters.
 */
void cName::addpar(const vector<cName::type_t> & inpar) {
	parameters = inpar;
}

/**
 * @brief Function which empties a type_t structure
 * @param x Reference to the structure to be cleared.
 */
void cName::type_t_clear(type_t &x) {
	x.type = TT_UNKNOWN;
	x.n.clear();
	x.value = nullptr;
	x.modifiers.clear();
	x.is_array = false;
	x.array_dimensions.clear();
	x.is_const = false;
	x.is_restrict = false;
	x.is_volatile = false;
	x.is_pointer = 0;
	x.is_reference = false;
	x.is_rvalue = false;
	x.is_cpair = false;
	x.is_imaginary = false;
	x.mst = MST_NULL;
}

/**
 * @brief Function which sets the type of the mangled name.
 * @param x The new type of name.
 */
void cName::setnametype(cName::ntype x) {
	name_type = x;
}

/**
 * @brief Function which sets the function call convention of the current name.
 * @param x The new type of name.
 */
void cName::setfcall(cName::fcall_t x) {
	function_call = x;
}

/**
 * @brief Function which sets member function access level.
 * @param x The new type of name.
 */
void cName::setmfacc(cName::memfacc_t x) {
	member_function_access = x;
}

/**
 * @brief Function which gets the type of the mangled name.
 * @return Current type of the mangled name
 */
cName::ntype cName::getnametype() {
	return name_type;
}

/**
 * @brief Function which sets the type of operator.
 * @param x The new type of operator.
 */
void cName::setop(cName::optype x) {
	operator_type = x;
}

/**
 * @brief Function which sets the return type.
 * @param x The type struct which will be assigned as return type of the name.
 */
void cName::setret(cName::type_t x) {
	return_type = x;
}

/**
 * @brief Function which sets the special type.
 * @param x The type struct which will be assigned as special type of the name.
 */
void cName::setspec(cName::type_t x) {
	special_type = x;
}

/**
 * @brief Function which sets the name's static flag.
 */
void cName::setstatic() {
	is_static = true;
}

/**
 * @brief Function which sets the name's virtual flag.
 */
void cName::setvirtual() {
	is_virtual = true;
}

/**
 * @brief Function which sets modifiers of the entire name.
 * @param x The string which will be assigned as type modifiers of the name.
 */
void cName::setmodifiers(string x) {
	modifiers = x;
}

/**
 * @brief Function which sets the template of the template function (for constructors or destructors).
 * @param x Pointer to the template (name_t vector).
 */
void cName::settftpl(void* x) {
	tf_tpl = x;
}

/**
 * @brief Function which adds a number to RTTI Base Class Descriptor.
 * @param x An unsigned integer to be added to the RTTI-BCD vector.
 */
void cName::addrttinum(long int x) {
	rttibcd.push_back(x);
}

/**
 * @brief Function which a modifier to the name.
 * @param x A char representing the modifier.
 */
void cName::addmodifier(char x) {
	modifiers += x;
}

/**
 * @brief Function which a storage class modifier.
 * @param x A char representing the modifier.
 */
void cName::addstcl(char x) {
	storage_class += x;
}

/**
 * @brief Function which recursively deletes parameters including the templates in their qualified names.
 * @param vec The vector of parameters.
 */
void cName::deleteparams(vector<cName::type_t> & vec) {
	for(vector<type_t>::iterator i=vec.begin(); i != vec.end(); ++i) {
		if (i->value != nullptr) {
			free(i->value);
		}
		for(vector<name_t>::iterator j=i->n.begin(); j != i->n.end(); ++j) {
			if (j->tpl != nullptr) {
				deleteparams(*(static_cast<vector<type_t>*>(j->tpl)));
				static_cast<vector<type_t>*>(j->tpl)->clear();
				delete static_cast<vector<type_t>*>(j->tpl);
			}
		}
	}
}

/**
 * @brief Destructor of cName class which deletes all dynamically allocated template arguments.
 */
cName::~cName() {
	for(vector<name_t>::iterator i=name.begin(); i != name.end(); ++i) {
		if (i->tpl != nullptr) {
			deleteparams(*(static_cast<vector<type_t>*>(i->tpl)));
			static_cast<vector<type_t>*>(i->tpl)->clear();
			delete static_cast<vector<type_t>*>(i->tpl);
		}
	}
	deleteparams(parameters);
	if (tf_tpl != nullptr) {
		deleteparams(*(static_cast<vector<type_t>*>(tf_tpl)));
		static_cast<vector<type_t>*>(tf_tpl)->clear();
		delete static_cast<vector<type_t>*>(tf_tpl);
	}
}

/**
 * @brief Function which shows the function name of the current demangled name.
 * @param vec The qualified name to be printed.
 * @param compiler Select the format of name according to compiler ("gcc", "ms", "borland"). Default is "gcc".
 */
string cName::printname(vector<cName::name_t> & vec, string compiler) {
	string retvalue;
	//for every name element...
	for(vector<name_t>::iterator i=vec.begin(); i != vec.end(); ++i) {
		last_shown_endtpl = false;
		//separate individual names with ::
		if (i != vec.begin()) {
			retvalue += "::";
		}
		//print the name
		retvalue += i->un;
		//if the name element contains pointer to a template, print the template
		if (i->tpl != nullptr) {
			retvalue += "<";
			retvalue += printparams(*(static_cast<vector<type_t>*>(i->tpl)), false, compiler);
			if (last_shown_endtpl) {
				retvalue += " ";
				last_shown_endtpl = false;
			}
			retvalue += ">";
			last_shown_endtpl = true;
		}
	}
	return retvalue;
}

/**
 * @brief Function which shows the modifiers of a type or name.
 * @param x The modifiers of a type or name.
 * @param space Indentation with space or no.
 * @return String containing modifiers.
 */
string cName::printmodifiers(string x, bool space) {
	string retvalue;
	//parameter modifiers
	for(size_t i=0; i<x.length(); ++i) {
		switch(x[i]) {
			case 'r':
				if (space) {retvalue += " ";}
				retvalue += "restrict";
				if (!space) {retvalue += " ";}
				break;
			case 'V':
				if (space) {retvalue += " ";}
				retvalue += "volatile";
				if (!space) {retvalue += " ";}
				break;
			case 'K':
				if (space) {retvalue += " ";}
				retvalue += "const";
				if (!space) {retvalue += " ";}
				break;
			case 'P':
				if (space) {retvalue += " ";}
				retvalue += "*";
				if (!space) {retvalue += " ";}
				break;
			case 'R':
				if (space) {retvalue += " ";}
				retvalue += "&";
				if (!space) {retvalue += " ";}
				break;
			case 'O':
				if (space) {retvalue += " ";}
				retvalue += "&&";
				if (!space) {retvalue += " ";}
				break;
			case 'C':
				if (space) {retvalue += " ";}
				retvalue += "complex pair";
				if (!space) {retvalue += " ";}
				break;
			case 'G':
				if (space) {retvalue += " ";}
				retvalue += "imaginary";
				if (!space) {retvalue += " ";}
				break;
			case 'a':
				if (space) {retvalue += " ";}
				retvalue += "near";
				if (!space) {retvalue += " ";}
				break;
			case 'c':
				if (space) {retvalue += " ";}
				retvalue += "huge";
				if (!space) {retvalue += " ";}
				break;
			case 'd':
				if (space) {retvalue += " ";}
				retvalue += "__unaligned";
				if (!space) {retvalue += " ";}
				break;
			case 'e':
				if (space) {retvalue += " ";}
				retvalue += "__restrict";
				if (!space) {retvalue += " ";}
				break;
			default:
				break;
		}
	}
	return retvalue;
}

/**
 * @brief Function which shows the modifiers of a type placed before the type.
 * @param x The modifiers of a type or name.
 * @param space Indentation with space or no.
 * @return String containing modifiers.
 */
string cName::printpremodifiers(string x, bool space) {
	string retvalue;
	//parameter modifiers
	for(size_t i=0; i<x.length(); ++i) {
		switch(x[i]) {
			case 'r':
				if (space) {retvalue += " ";}
				retvalue += "restrict";
				if (!space) {retvalue += " ";}
				break;
			case 'V':
				if (space) {retvalue += " ";}
				retvalue += "volatile";
				if (!space) {retvalue += " ";}
				break;
			case 'K':
				if (space) {retvalue += " ";}
				retvalue += "const";
				if (!space) {retvalue += " ";}
				break;
			case 'C':
				if (space) {retvalue += " ";}
				retvalue += "complex pair";
				if (!space) {retvalue += " ";}
				break;
			case 'G':
				if (space) {retvalue += " ";}
				retvalue += "imaginary";
				if (!space) {retvalue += " ";}
				break;
			case 'a':
				if (space) {retvalue += " ";}
				retvalue += "near";
				if (!space) {retvalue += " ";}
				break;
			case 'c':
				if (space) {retvalue += " ";}
				retvalue += "huge";
				if (!space) {retvalue += " ";}
				break;
			case 'd':
				if (space) {retvalue += " ";}
				retvalue += "__unaligned";
				if (!space) {retvalue += " ";}
				break;
			case 'e':
				if (space) {retvalue += " ";}
				retvalue += "__restrict";
				if (!space) {retvalue += " ";}
				break;
			default:
				break;
		}
	}
	return retvalue;
}

/**
 * @brief Function which shows the modifiers of a type or name placed after the type.
 * @param x The modifiers of a type or name.
 * @param space Indentation with space or no.
 * @return String containing modifiers.
 */
string cName::printpostmodifiers(string x, bool space) {
	string retvalue;
	//parameter modifiers
	for(size_t i=0; i<x.length(); ++i) {
		switch(x[i]) {
			case 'P':
				if (space) {retvalue += " ";}
				retvalue += "*";
				if (!space) {retvalue += " ";}
				break;
			case 'R':
				retvalue += "&";
				if (!space) {retvalue += " ";}
				break;
			case 'O':
				if (space) {retvalue += " ";}
				retvalue += "&&";
				if (!space) {retvalue += " ";}
				break;
			default:
				break;
		}
	}
	return retvalue;
}

/**
 * @brief Function which prints a primary expression type.
 * @param x The primary expression to be printed.
 */
string cName::printpexpr(type_t & x) {
	string retvalue;
	switch(x.b) {
		case T_BOOL:
			if (*(static_cast<bool*>(x.value))) {
				retvalue += "true";
			}
			else {
				retvalue += "false";
			}
			break;
		default:
			break;
	}
	return retvalue;
}

/**
 * @brief Function which prints a sequence of parameters into a string.
 * @param vec The vector of parameters to be printed.
 * @param ignorevoid Ignore void arguments.
 * @param compiler Select the format of name according to compiler ("gcc", "ms", "borland"). Default is "gcc".
 * @return String containing the sequence of parameters
 */
string cName::printparams(vector<cName::type_t> & vec, bool ignorevoid, string compiler) {
	string retvalue;
	//for every parameter...
	if ((ignorevoid == false) || !(vec.size() == 1 && vec.front().type == TT_BUILTIN && vec.front().b == T_VOID && vec.front().modifiers == "")) {
		for(vector<type_t>::iterator i=vec.begin(); i != vec.end(); ++i) {
			last_shown_endtpl = false;
			//separate individual parameters by comma
			if (i != vec.begin()) {
				retvalue += ", ";
			}

			//Borland
			if (compiler == "borland") {
				if (!i->modifiers.empty()) {
					last_shown_endtpl = false;
					retvalue += printpremodifiers(i->modifiers,false);
				}
			}

			if (i->mst != MST_NULL) {
				switch(i->mst) {
					case MST_UNION:
						retvalue += "union ";
						break;
					case MST_STRUCT:
						retvalue += "struct ";
						break;
					case MST_CLASS:
						retvalue += "class ";
						break;
					case MST_ENUM:
						retvalue += "enum ";
						break;
					default:
						break;
				}
			}

			//type of the parameter
			switch (i->type) {
				case TT_UNKNOWN:
					break;
				case TT_PEXPR:
					retvalue += printpexpr(*i);
					break;
				//built-in type
				case TT_BUILTIN:
					switch(i->b) {
						case T_VOID:
							retvalue += "void";
							break;
						case T_WCHAR:
							retvalue += "wchar_t";
							break;
						case T_BOOL:
							retvalue += "bool";
							break;
						case T_CHAR:
							retvalue += "char";
							break;
						case T_SCHAR:
							retvalue += "signed char";
							break;
						case T_UCHAR:
							retvalue += "unsigned char";
							break;
						case T_SHORT:
							retvalue += "short";
							break;
						case T_USHORT:
							retvalue += "unsigned short";
							break;
						case T_INT:
							retvalue += "int";
							break;
						case T_UINT:
							retvalue += "unsigned int";
							break;
						case T_LONG:
							retvalue += "long";
							break;
						case T_ULONG:
							retvalue += "unsigned long";
							break;
						case T_LONGLONG:
							retvalue += "long long";
							break;
						case T_ULONGLONG:
							retvalue += "unsigned long long";
							break;
						case T_INT128:
							retvalue += "__int128";
							break;
						case T_UINT128:
							retvalue += "unsigned __int128";
							break;
						case T_FLOAT:
							retvalue += "float";
							break;
						case T_DOUBLE:
							retvalue += "double";
							break;
						case T_LONGDOUBLE:
							retvalue += "long double";
							break;
						case T_FLOAT128:
							retvalue += "__float128";
							break;
						case T_ELLIPSIS:
							retvalue += "ellipsis";
							break;
						case T_DD:
							retvalue += "IEEE 754r decimal floating point (64 bits)";
							break;
						case T_DE:
							retvalue += "IEEE 754r decimal floating point (128 bits)";
							break;
						case T_DF:
							retvalue += "IEEE 754r decimal floating point (32 bits)";
							break;
						case T_DH:
							retvalue += "IEEE 754r half-precision floating point (16 bits)";
							break;
						case T_CHAR32:
							retvalue += "char32_t";
							break;
						case T_CHAR16:
							retvalue += "char16_t";
							break;
						case T_AUTO:
							retvalue += "auto";
							break;
						case T_NULLPTR:
							retvalue += "std::nullptr_t";
							break;
						default:
							break;
					}
					break;

				//number
				case TT_NUM:
					retvalue += std::to_string(i->num);
					break;
				//named type
				case TT_NAME:
					//print the name
					retvalue += printname(i->n, compiler);
					break;
				default:
					break;
			}

			//array
			if (i->is_array) {
				last_shown_endtpl = false;
				retvalue += " ";
				if (i->array_dimensions.empty()) {
					retvalue += "[]";
				}
				else {
					for(vector<unsigned int>::iterator j = i->array_dimensions.begin(); j != i->array_dimensions.end(); ++j) {
						retvalue += "[";
						retvalue += std::to_string(*j);
						retvalue += "]";
					}
				}
			}

			//Borland
			if (compiler == "borland") {
				if (!i->modifiers.empty()) {
					last_shown_endtpl = false;
					retvalue += printpostmodifiers(i->modifiers,true);
				}
			}

			else {
				if (!i->modifiers.empty()) {
					last_shown_endtpl = false;
					retvalue += printmodifiers(i->modifiers,true);
				}
			}
		}
	}
	return retvalue;
}

/**
 * @brief Function which converts optype enum to string.
 * @param x The optype to be converted.
 * @return The string containing name of the operator.
 */
string cName::optypetostr(optype x) {
	string retvalue;
	switch (x) {
		case OT_NEW:
			retvalue = "new";
			break;
		case OT_NEWARR:
			retvalue = "new[]";
			break;
		case OT_DEL:
			retvalue = "delete";
			break;
		case OT_DELARR:
			retvalue = "delete[]";
			break;
		case OT_UPLUS:
			retvalue = "+";
			break;
		case OT_UMINUS:
			retvalue = "-";
			break;
		case OT_UAND:
			retvalue = "&";
			break;
		case OT_UAST:
			retvalue = "*";
			break;
		case OT_TILDA:
			retvalue = "~";
			break;
		case OT_PLUS:
			retvalue = "+";
			break;
		case OT_MINUS:
			retvalue = "-";
			break;
		case OT_AST:
			retvalue = "*";
			break;
		case OT_DIV:
			retvalue = "/";
			break;
		case OT_MOD:
			retvalue = "%";
			break;
		case OT_AND:
			retvalue = "&";
			break;
		case OT_OR:
			retvalue = "|";
			break;
		case OT_EXP:
			retvalue = "^";
			break;
		case OT_ASSIGN:
			retvalue = "=";
			break;
		case OT_PLUSASS:
			retvalue = "+=";
			break;
		case OT_MINUSASS:
			retvalue = "-=";
			break;
		case OT_ASTASS:
			retvalue = "*=";
			break;
		case OT_DIVASS:
			retvalue = "/=";
			break;
		case OT_MODASS:
			retvalue = "%=";
			break;
		case OT_ANDASS:
			retvalue = "&=";
			break;
		case OT_ORASS:
			retvalue = "|=";
			break;
		case OT_EXPASS:
			retvalue = "^=";
			break;
		case OT_LSHIFT:
			retvalue = "<<";
			break;
		case OT_RSHIFT:
			retvalue = ">>";
			break;
		case OT_LSHIFTASS:
			retvalue = "<<=";
			break;
		case OT_RSHIFTASS:
			retvalue = ">>=";
			break;
		case OT_EQ:
			retvalue = "==";
			break;
		case OT_NEQ:
			retvalue = "!=";
			break;
		case OT_LT:
			retvalue = "<";
			break;
		case OT_GT:
			retvalue = ">";
			break;
		case OT_LE:
			retvalue = "<=";
			break;
		case OT_GE:
			retvalue = ">=";
			break;
		case OT_NOT:
			retvalue = "!";
			break;
		case OT_ANDAND:
			retvalue = "&&";
			break;
		case OT_OROR:
			retvalue = "||";
			break;
		case OT_PLUSPLUS:
			retvalue = "++";
			break;
		case OT_MINUSMINUS:
			retvalue = "--";
			break;
		case OT_COMMA:
			retvalue = ",";
			break;
		case OT_PTAST:
			retvalue = "->*";
			break;
		case OT_PT:
			retvalue = "->";
			break;
		case OT_BRACKETS:
			retvalue = "()";
			break;
		case OT_ARR:
			retvalue = "[]";
			break;
		case OT_QUESTION:
			retvalue = "?";
			break;
		case OT_SIZEOFT:
			retvalue = "sizeof";
			break;
		case OT_SIZEOFE:
			retvalue = "sizeof";
			break;
		case OT_ALIGNOFT:
			retvalue = "alignof";
			break;
		case OT_ALIGNOFE:
			retvalue = "alignof";
			break;
		default:
			break;
	}
	return retvalue;
}

/**
 * @brief Print the calling convention to a string.
 * @param callconv The calling convention to be printed.
 * @return String containing calling convention.
 */
string cName::printcallingconvention(fcall_t callconv) {
	string retvalue = "";
	if (callconv != FCC_NULL) {
		switch(callconv) {
			case FCC_CDECL:
				retvalue = "__cdecl";
				break;
			case FCC_PASCAL:
				retvalue = "__pascal";
				break;
			case FCC_FORTRAN:
				retvalue = "__fortran";
				break;
			case FCC_THISCALL:
				retvalue = "__thiscall";
				break;
			case FCC_STDCALL:
				retvalue = "__stdcall";
				break;
			case FCC_FASTCALL:
				retvalue = "__fastcall";
				break;
			case FCC_INTERRUPT:
				retvalue = "interrupt";
				break;
			default:
				break;
		}
	}
	return retvalue;
}

/**
 * @brief Function which prints everything the demangled name contains into a string.
 * @param compiler Select the format of name according to compiler ("gcc", "ms", "borland"). Default is "gcc".
 * @return String containing declaration of the demangled function.
 */
string cName::printall(string compiler) {
	string retvalue;
	last_shown_endtpl = false;
	vector<type_t> ret_vec;
	vector<name_t> nam_vec;

	if (member_function_access != MFM_NULL) {
		switch(member_function_access) {
			case MFM_PRIVATE:
				retvalue += "private: ";
				break;
			case MFM_PUBLIC:
				retvalue += "public: ";
				break;
			case MFM_PROTECTED:
				retvalue += "protected: ";
				break;
			default:
				break;
		}
	}

	if (is_static) {
		retvalue += "static ";
	}

	if (is_virtual) {
		retvalue += "virtual ";
	}

	if (return_type.type != TT_UNKNOWN) {
		ret_vec.push_back(return_type);
		retvalue += printparams(ret_vec, false, compiler);
		ret_vec.clear();
		retvalue += " ";
	}

	if (!storage_class.empty()) {
		retvalue += printmodifiers(storage_class,false);
	}

	//get calling convention
	retvalue += printcallingconvention(function_call);
	if (function_call != FCC_NULL) {
		retvalue += " ";
	}

	//print the function name
	retvalue += printname(name, compiler);

	if (name_type == NT_VTABLE) {
		retvalue += "::`vftable'";
	}
	else if (name_type == NT_R0) {
		retvalue += "`RTTI Type Descriptor'";
	}
	else if (name_type == NT_R1) {
		retvalue += "::`RTTI Base Class Descriptor at (";
		for(vector<long int>::iterator i=rttibcd.begin(); i != rttibcd.end(); ++i) {
			if (i != rttibcd.begin()) {
				retvalue += ", ";
			}
			retvalue += std::to_string(*i);
		}
		retvalue += ")'";
	}
	else if (name_type == NT_R2) {
		retvalue += "::`RTTI Base Class Array'";
	}
	else if (name_type == NT_R3) {
		retvalue += "::`RTTI Class Hierarchy Descriptor'";
	}
	else if (name_type == NT_R4) {
		retvalue += "::`RTTI Complete Object Locator'";
	}
	else if (name_type == NT__A) {
		retvalue += "::`typeof'";
	}
	else if (name_type == NT__B) {
		retvalue += "::`local static guard'";
	}
	else if (name_type == NT__C) {
		retvalue += "::";
	}
	else if (name_type == NT__D) {
		retvalue += "::`vbase destructor'";
	}
	else if (name_type == NT__E) {
		retvalue += "::`vector deleting destructor'";
	}
	else if (name_type == NT__F) {
		retvalue += "::`default constructor closure'";
	}
	else if (name_type == NT__G) {
		retvalue += "::`scalar deleting destructor'";
	}
	else if (name_type == NT__H) {
		retvalue += "::`vector constructor iterator'";
	}
	else if (name_type == NT__I) {
		retvalue += "::`vector destructor iterator'";
	}
	else if (name_type == NT__J) {
		retvalue += "::`vector vbase constructor iterator'";
	}
	else if (name_type == NT__K) {
		retvalue += "::`virtual displacement map'";
	}
	else if (name_type == NT__L) {
		retvalue += "::`eh vector constructor iterator'";
	}
	else if (name_type == NT__M) {
		retvalue += "::`eh vector destructor iterator'";
	}
	else if (name_type == NT__N) {
		retvalue += "::`eh vector vbase constructor iterator'";
	}
	else if (name_type == NT__O) {
		retvalue += "::`copy constructor closure'";
	}
	else if (name_type == NT__P) {
		retvalue += "::";
	}
	else if (name_type == NT__Q) {
		retvalue += "::";
	}
	else if (name_type == NT__R) {
		retvalue += "::";
	}
	else if (name_type == NT__S) {
		retvalue += "::`local vftable'";
	}
	else if (name_type == NT__T) {
		retvalue += "::`local vftable constructor closure'";
	}
	else if (name_type == NT__U) {
		retvalue += "::";
	}
	else if (name_type == NT__V) {
		retvalue += "::";
	}
	else if (name_type == NT__W) {
		retvalue += "::";
	}
	else if (name_type == NT__X) {
		retvalue += "::`placement delete closure'";
	}
	else if (name_type == NT__Y) {
		retvalue += "::`placement delete[] closure'";
	}
	else if (name_type == NT__Z) {
		retvalue += "::";
	}

	//if name is a constructor, duplicate the last name element
	if (name_type == NT_CONSTRUCTOR) {
		retvalue += "::";
		//MSVC++
		if (compiler == "ms") {
			nam_vec.push_back(name.back());
			retvalue += printname(nam_vec, compiler);
			nam_vec.clear();
		}

		//GCC
		else {
			nam_vec.push_back(name.back());
			nam_vec.back().tpl = tf_tpl;
			retvalue += printname(nam_vec, compiler);
			nam_vec.clear();
		}
	}
	//if name is a destructor, duplicate the last name element with a tilda
	else if (name_type == NT_DESTRUCTOR) {
		retvalue += "::~";

		//MSVC++
		if (compiler == "ms") {
			nam_vec.push_back(name.back());
			retvalue += printname(nam_vec, compiler);
			nam_vec.clear();
		}

		//GCC
		else {
			nam_vec.push_back(name.back());
			nam_vec.back().tpl = tf_tpl;
			retvalue += printname(nam_vec, compiler);
			nam_vec.clear();
		}
	}
	//if name is an operator, append "operator" and the operator name
	if (name_type == NT_OPERATOR) {
		if (operator_type == OT_CAST) {
			ret_vec.push_back(special_type);
			retvalue += printparams(ret_vec, false, compiler);
			ret_vec.clear();
		}
	}

	//print the parameters
	if (!parameters.empty()) {
		retvalue += "(";
		retvalue += printparams(parameters, true, compiler);
		retvalue += ")";
	}

	if (!modifiers.empty()) {
		retvalue += printmodifiers(modifiers,true);
	}

	return retvalue;
}

/**
 * @brief Function which prints everything the demangled name contains into a string. (overloaded for backward compatibility)
 * @param msvcpp Print the declaration in the MS VC++ style (Some differences in constructors / destructors)? - Default is false.
 * @return String containing declaration of the demangled function.
 */
string cName::printall_old(bool msvcpp) {
	if (msvcpp == true) {
		return printall(string("ms"));
	}
	else {
		return printall(string("gcc"));
	}

}

/**
 *
 */
string cName::type_t::getLlvmType() {
	if (llvmIr.empty()) {
		llvmIr = getLlvmTypePrivate();
		return llvmIr;
	} else {
		return llvmIr;
	}
}

string cName::type_t::getLlvmTypePrivate() {
	//array
	if (is_array && !array_dimensions.empty()) {
		unsigned int arrDim = array_dimensions[0];

		//reduce dimensions by one (if this was the last one, clear the isArray bool)
		array_dimensions.erase(array_dimensions.begin());
		if (array_dimensions.empty()) {
			is_array = false;
		}
		//get array type recursively from the rest of this type
		return "[" + to_string(arrDim) + " x " + getLlvmTypePrivate() + " ]";
	}

	//structure
	if (mst == MST_STRUCT) {
		//TODO: this is not OK, this method is not supported anymore, and it created only empty structure before.
		//If there are some info about structure, create new StructType, fill it with info and add to type manager
		//using TypeManager::addStructType(StructType *s).
		return "i32";
	}

	string typeName = modifiers + " ";
	for (unsigned k = 0; k < n.size(); ++k) {
		typeName += n[k].un + " ";
	}

	//named type
	if (type == cName::TT_NAME) {
		if (typeName.find("AnsiString") != string::npos) {
			return "i8*";
		} else if (typeName.find("WideString") != string::npos) {
			return "%wchar_t*";
		} else {
			return "i32*";
		}
	}

	//modifiers
	while (true) {
		if (modifiers.empty()) {
			break;
		}

		char modifier = modifiers[modifiers.size()-1] ;

		//modifier is a pointer, make a pointer of recursive call on the rest of the type
		if (modifier == 'P') {
			modifiers = modifiers.substr(0, modifiers.size()-1);
			return getLlvmTypePrivate() + "*";
		} else {
			modifiers = modifiers.substr(0,modifiers.size()-1);
		}
	}

	//switch the built-in type
	//TODO add support for named types, take modifiers (const, ppointer, reference) into account, etc.
	//See "struct type_t" in demangler/gparser.h
	switch (b) {
		case demangler::cName::T_VOID: return "void";
		case demangler::cName::T_FLOAT: return "float";
		case demangler::cName::T_DOUBLE: return "double";
		case demangler::cName::T_CHAR:
			//pointer to char is a string
			if (modifiers == "P") {
				return "i8*";
			}
			//normal signed char
			else {
				return "i8";
			}
			break;

		//char
		case demangler::cName::T_SCHAR:
			return "i8";
			break;
		case demangler::cName::T_UCHAR:
			return "i8";
			break;

		//short int
		case demangler::cName::T_SHORT:
			return "i16";
			break;
		case demangler::cName::T_USHORT:
			return "i16";
			break;

		//int
		case demangler::cName::T_INT:
			return "i32";
			break;
		case demangler::cName::T_UINT:
			return "i32";
			break;

		//long int (32-bit?)
		case demangler::cName::T_LONG:
			return "i32";
			break;
		case demangler::cName::T_ULONG:
			return "i32";
			break;

		//long long int (64-bit)
		case demangler::cName::T_LONGLONG:
			return "i64";
			break;
		case demangler::cName::T_ULONGLONG:
			return "i64";
			break;

		//int (128-bit)
		case demangler::cName::T_INT128:
			return "i128";
			break;
		case demangler::cName::T_UINT128:
			return "i128";
			break;

		//TODO unsupported built-in type
		default:
			return "i32";
			break;
	}

	//TODO: an unknown or unsupported type (or a named type, class, union,...)
	return "i32";
}

} // namespace demangler
