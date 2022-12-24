/**
 * @file include/demangler/gparser.h
 * @brief Parser of LL grammar.
 * @copyright (c) 2017 Avast Software, licensed under the MIT license
 */

#ifndef DEMANGLER_GPARSER_H
#define DEMANGLER_GPARSER_H

#include <map>
#include <set>
#include <stack>
#include <string>
#include <utility>
#include <vector>

//internal grammars

namespace demangler {

/**
 * @brief Class which holds a demangled name.
 */
class cName {
public:
	/**
	 * @brief Enum of types of the mangled types (Determines whether a type is built-in or named).
	 */
	enum ttype { //type of type
		TT_UNKNOWN = 0, //can be used for unknown return type
		TT_BUILTIN,
		TT_NAME,
		TT_NUM,
		TT_PEXPR
	};

	/**
	 * @brief Enum of types of the mangled names.
	 */
	enum ntype { //type of the mangled name
		NT_FUNCTION = 0,
		NT_TEMPLFUNCTION,
		NT_OPERATOR,
		NT_CONSTRUCTOR,
		NT_DESTRUCTOR,
		NT_DATA,
		NT_VTABLE,
		NT_R0,
		NT_R1,
		NT_R2,
		NT_R3,
		NT_R4,
		NT__A,
		NT__B,
		NT__C,
		NT__D,
		NT__E,
		NT__F,
		NT__G,
		NT__H,
		NT__I,
		NT__J,
		NT__K,
		NT__L,
		NT__M,
		NT__N,
		NT__O,
		NT__P,
		NT__Q,
		NT__R,
		NT__S,
		NT__T,
		NT__U,
		NT__V,
		NT__W,
		NT__X,
		NT__Y,
		NT__Z,
		NT_CLASS
	};

	/**
	 * @brief Enum of operator types.
	 */
	enum optype { //type of operator
		OT_NULL = 0,
		OT_NEW,
		OT_NEWARR,
		OT_DEL,
		OT_DELARR,
		OT_UPLUS,
		OT_UMINUS,
		OT_UAND,
		OT_UAST,
		OT_TILDA,
		OT_PLUS,
		OT_MINUS,
		OT_AST,
		OT_DIV,
		OT_MOD,
		OT_AND,
		OT_OR,
		OT_EXP,
		OT_ASSIGN,
		OT_PLUSASS,
		OT_MINUSASS,
		OT_ASTASS,
		OT_DIVASS,
		OT_MODASS,
		OT_ANDASS,
		OT_ORASS,
		OT_EXPASS,
		OT_LSHIFT,
		OT_RSHIFT,
		OT_LSHIFTASS,
		OT_RSHIFTASS,
		OT_EQ,
		OT_NEQ,
		OT_LT,
		OT_GT,
		OT_LE,
		OT_GE,
		OT_NOT,
		OT_ANDAND,
		OT_OROR,
		OT_PLUSPLUS,
		OT_MINUSMINUS,
		OT_COMMA,
		OT_PTAST,
		OT_PT,
		OT_BRACKETS,
		OT_ARR,
		OT_QUESTION,
		OT_SIZEOFT,
		OT_SIZEOFE,
		OT_ALIGNOFT,
		OT_ALIGNOFE,
		OT_CAST
	};

	/**
	 * @brief Enum of built-in types.
	 */
	enum st_type { //standard built-in types
		T_VOID = 0,
		T_WCHAR,
		T_BOOL,
		T_CHAR,
		T_SCHAR,
		T_UCHAR,
		T_SHORT,
		T_USHORT,
		T_INT,
		T_UINT,
		T_LONG,
		T_ULONG,
		T_LONGLONG,
		T_ULONGLONG,
		T_INT128,
		T_UINT128,
		T_FLOAT,
		T_DOUBLE,
		T_LONGDOUBLE,
		T_FLOAT128,
		T_ELLIPSIS,
		T_DD,
		T_DE,
		T_DF,
		T_DH,
		T_CHAR32,
		T_CHAR16,
		T_AUTO,
		T_NULLPTR
	};

	/**
	 * @brief Enum of member function accessibility.
	 */
	enum memfacc_t { //standard built-in types
		MFM_NULL = 0,
		MFM_PRIVATE,
		MFM_PUBLIC,
		MFM_PROTECTED
	};

	/**
	 * @brief Enum of function call conventions.
	 */
	enum fcall_t { //standard built-in types
		FCC_NULL = 0,
		FCC_CDECL,
		FCC_PASCAL,
		FCC_FORTRAN,
		FCC_THISCALL,
		FCC_STDCALL,
		FCC_FASTCALL,
		FCC_INTERRUPT
	};

	/**
	 * @brief Enum of named type classifications. Union, Struct, Class, Enum.
	 */
	enum mstype_t { //standard built-in types
		MST_NULL = 0,
		MST_UNION,
		MST_STRUCT,
		MST_CLASS,
		MST_ENUM
	};


	/**
	 * @brief Structure of an unqualified name.
	 * @param un String which holds the name.
	 * @param tpl Pointer to the the template (vector of types) of this unqualified name. If nullptr, the unqualified name consists only of the string.
	 */
	struct name_t {
		std::string un; //unqualified name
		void *tpl = nullptr; //std::vector<type_t>
		bool op = false; //is it operator name element?
	};

	/**
	 * @brief Structure of a type.
	 * @param type Type of the type.
	 * @param b Built-in type. This value is defined only if 'type' is TT_BUILTIN.
	 * @param n Qualified name of the type. This value is defined only if 'type' is TT_NAME.
	 * @param is_const Bool value which determines whether the type is const.
	 * @param is_restrict Bool value which determines whether the type is restrict.
	 * @param is_volatile Bool value which determines whether the type is volatile.
	 * @param is_pointer Integer value determining the pointer level of the type.
	 * @param is_reference Bool value which determines whether the type is a reference.
	 * @param is_rvalue Bool value which determines whether the type is an R-value.
	 * @param is_cpair Bool value which determines whether the type is a complex pair.
	 * @param is_imaginary Bool value which determines whether the type is imaginary.
	 */
	struct type_t {
		ttype type = TT_UNKNOWN; //type of type... builtin or named type
		st_type b = T_VOID; //builtin type
		void *value = nullptr; //expression value
		std::vector<name_t> n; //qualified name of named type
		std::string modifiers;
		mstype_t mst = MST_NULL;
		int num = 0;

		bool is_array = false;
		std::vector<unsigned int> array_dimensions;
		bool is_const = false;
		bool is_restrict = false;
		bool is_volatile = false;
		unsigned int is_pointer = 0;
		bool is_reference = false;
		bool is_rvalue = false; //r-value reference
		bool is_cpair = false; //complex pair
		bool is_imaginary = false;

		std::string getLlvmType();

		private:
			std::string llvmIr;
			std::string getLlvmTypePrivate();
	};

	ntype name_type = NT_FUNCTION; //name type
	optype operator_type = OT_NULL; //type of operator. it is OT_NULL if function is not an operator
	type_t return_type;
	type_t special_type; //return value for function or conversion type for operator OT_CAST
	std::string modifiers;

	memfacc_t member_function_access = MFM_NULL;
	fcall_t function_call = FCC_NULL;
	bool is_static = false;
	bool is_virtual = false;
	std::string storage_class;
	std::vector<long int> rttibcd;

	std::vector<type_t> parameters; //function parameters
	std::vector<name_t> name; //qualified name composed of unqualified names

	void *tf_tpl = nullptr;

	bool last_shown_endtpl = false; //an auxiliary variable which helps to add space between multiple '>' at the end of templates

	void deleteparams(std::vector<type_t> & vec);
	cName(); //constructor
	virtual ~cName(); //mass destruction
	void type_t_clear(type_t &x);
	void addname(const std::vector<name_t> & inname); //set the function name
	void addpar(const std::vector<type_t> & inpar); //set the parameters of the name
	void setnametype(ntype x); //set type of the mangled name
	void setfcall(fcall_t x); //set type of the function call convention
	void setmfacc(memfacc_t x); //set type of the member function access level
	ntype getnametype(void); //get type of the mangled name
	void setop(optype x); //set operator type
	void setret(type_t x); //set return type
	void setspec(type_t x); //set special type
	void setstatic(); //set name's static flag
	void setvirtual(); //set name's virtual flag
	void addmodifier(char x); //add a modifier to the name modifier string
	void addstcl(char x); //add a modifier to the storage class string
	void setmodifiers(std::string x); //set modifiers
	void settftpl(void* x); //set template function template
	void addrttinum(long int x); //add a RTTI Base Class descriptor num
	std::string optypetostr(optype x); //operator type to string
	std::string printmodifiers(std::string x, bool space);
	std::string printpremodifiers(std::string x, bool space);
	std::string printpostmodifiers(std::string x, bool space);
	std::string printname(std::vector<name_t> & vec, std::string compiler = "gcc");
	std::string printparams(std::vector<type_t> & vec, bool ignorevoid = false, std::string compiler = "gcc");
	std::string printpexpr(type_t & x);

	/**
	* @brief Print the calling convention to a string.
	* @param callconv The calling convention to be printed.
	* @return String containing calling convention.
	*/
	std::string printcallingconvention(fcall_t callconv);

	std::string printall(std::string compiler = "gcc");
	std::string printall_old(bool msvcpp = false);
}; //class cName

/**
 * @brief Grammar class. It's member functions allow loading an external grammar and demangling a mangled name using the grammar.
 */
class cGram {

public:
	/**
	* @brief Global array of semantic action names. Used when building internal LL table from external grammar.
	*/
	static const char *semactname[];

	/**
	* @brief Enum of error codes.
	*/
	enum errcode {
		ERROR_OK = 0,
		ERROR_FILE,
		ERROR_FSM,
		ERROR_SYN,
		ERROR_MEM,
		ERROR_GRAM,
		ERROR_LL,
		ERROR_UNK
	};

	/**
	 * @brief An array of error messages.
	 */
	static const char *errmsg[];

	/**
	 * @brief Type of a grammar element. It can be either a terminal or a non-terminal.
	 */
	enum gelemtype {
		GE_TERM = 0,
		GE_NONTERM
	};


	/**
	 * @brief Structure of a grammar element.
	 * @param type Type of the element (terminal or non-terminal).
	 * @param nt The name of the non-terminal. Only vylid if type is GE_NONTERM.
	 * @param t The byte value of the terminal. Only valid if type is GE_TERM.
	 */
	struct gelem_t {
		gelem_t(gelemtype t, char* n, unsigned int i, char c) :
			type(t),
			nt(n),
			ntst(i),
			t(c)
		{}
		gelem_t() {}
		gelemtype type = GE_TERM;
		char* nt = nullptr;
		unsigned int ntst = 0;
		char t = 0;
	};

	/**
	 * @brief Enum of semantic actions.
	 */
		enum semact {
		//do nothing
		SA_NULL = 0,

		//set type of name (function, operator, constructor, destructor, data)
		SA_SETNAMEF,
		SA_SETNAMETF,
		SA_SETNAMEO,
		SA_SETNAMEC,
		SA_SETNAMED,
		SA_SETNAMEX,
		SA_SETNAMEVT,

		//set operator type
		SA_SETOPXX,
		SA_SETOPNW,
		SA_SETOPNA,
		SA_SETOPDL,
		SA_SETOPDA,
		SA_SETOPPS,
		SA_SETOPNG,
		SA_SETOPAD,
		SA_SETOPDE,
		SA_SETOPCO,
		SA_SETOPPL,
		SA_SETOPMI,
		SA_SETOPML,
		SA_SETOPDV,
		SA_SETOPRM,
		SA_SETOPAN,
		SA_SETOPOR,
		SA_SETOPEO,
		SA_SETOPASS,
		SA_SETOPPLL,
		SA_SETOPMII,
		SA_SETOPMLL,
		SA_SETOPDVV,
		SA_SETOPRMM,
		SA_SETOPANN,
		SA_SETOPORR,
		SA_SETOPEOO,
		SA_SETOPLS,
		SA_SETOPRS,
		SA_SETOPLSS,
		SA_SETOPRSS,
		SA_SETOPEQ,
		SA_SETOPNE,
		SA_SETOPLT,
		SA_SETOPGT,
		SA_SETOPLE,
		SA_SETOPGE,
		SA_SETOPNT,
		SA_SETOPAA,
		SA_SETOPOO,
		SA_SETOPPP,
		SA_SETOPMM,
		SA_SETOPCM,
		SA_SETOPPM,
		SA_SETOPPT,
		SA_SETOPCL,
		SA_SETOPIX,
		SA_SETOPQU,
		SA_SETOPST,
		SA_SETOPSZ,
		SA_SETOPAT,
		SA_SETOPAZ,
		SA_SETOPCV,

		//builtin types
		SA_SETTYPEV,
		SA_SETTYPEW,
		SA_SETTYPEB,
		SA_SETTYPEC,
		SA_SETTYPEA,
		SA_SETTYPEH,
		SA_SETTYPES,
		SA_SETTYPET,
		SA_SETTYPEI,
		SA_SETTYPEJ,
		SA_SETTYPEL,
		SA_SETTYPEM,
		SA_SETTYPEX,
		SA_SETTYPEY,
		SA_SETTYPEN,
		SA_SETTYPEO,
		SA_SETTYPEF,
		SA_SETTYPED,
		SA_SETTYPEE,
		SA_SETTYPEG,
		SA_SETTYPEZ,

		//parameter modifiers
		SA_SETCONST,
		SA_SETRESTRICT,
		SA_SETVOLATILE,
		SA_SETPTR,
		SA_SETREF,
		SA_SETRVAL,
		SA_SETCPAIR,
		SA_SETIM,

		//substitutions
		SA_SUBSTD, //::std::
		SA_SUBALC, //::std::allocator
		SA_SUBSTR, //::std::basic_string
		SA_SUBSTRS, //::std::basic_string<char,::std::char_traits<char>,::std::allocator<char>>
		SA_SUBISTR, //::std::basic_istream<char,  std::char_traits<char>>
		SA_SUBOSTR, //::std::basic_ostream<char,  std::char_traits<char>>
		SA_SUBIOSTR, //::std::basic_iostream<char,  std::char_traits<char>>

		//other very important semantic actions
		SA_LOADID, //load an unqualified name into the qualified vector of names
		SA_LOADSUB, //load a substitution
		SA_LOADTSUB, //load a template sub
		SA_LOADARR, //load an array dimension
		SA_STOREPAR, //store current parameter to vector of parameters
		SA_STORETEMPARG, //store current parameter to current vector of template arguments
		SA_STORETEMPLATE, //store the whole template into the last name element of last name vector
		SA_BEGINTEMPL, //begin a template
		SA_SKIPTEMPL, //skip a template
		SA_PAR2F, //store current vector of parameters into the function
		SA_PAR2RET, //store current parameter to the return value
		SA_PAR2SPEC, //store current parameter to the special value
		SA_UNQ2F, //future identifiers are added to the function name
		SA_UNQ2P, //function identifiers are added to parameter name

		//substitution expansion modifiers
		SA_SSNEST, //nested sub
		SA_STUNQ, //unqualified std:: sub
		SA_SSNO, //other sub derived from <name>

		SA_TYPE2EXPR, //builtin type is converted to primary expression
		SA_EXPRVAL, //load expression value
		SA_BEGINPEXPR, //begin a primary expression
		SA_STOREPEXPR, //end a primary expression
		SA_COPYTERM, //copy the terminal on the input into current_name in substitution analyzer

		SA_ADDCHARTONAME, //add current character to current unqualified name
		SA_STORENAME, //move current unqualified name into current name vector
		SA_REVERSENAME,
		SA_SETPRIVATE,
		SA_SETPUBLIC,
		SA_SETPROTECTED,
		SA_SETFCDECL,
		SA_SETFPASCAL,
		SA_SETFFORTRAN,
		SA_SETFTHISCALL,
		SA_SETFSTDCALL,
		SA_SETFFASTCALL,
		SA_SETFINTERRUPT,
		SA_SETUNION,
		SA_SETSTRUCT,
		SA_SETCLASS,
		SA_SETENUM,
		SA_SETSTATIC,
		SA_SETVIRTUAL,
		SA_STCLCONST,
		SA_STCLVOL,
		SA_STCLFAR,
		SA_STCLHUGE,
		SA_SAVENAMESUB,
		SA_LOADNAMESUB,
		SA_MSTEMPLPSUB,
		SA_SETNAMER0,
		SA_SETNAMER1,
		SA_SETNAMER2,
		SA_SETNAMER3,
		SA_SETNAMER4,
		SA_SETNAME_A,
		SA_SETNAME_B,
		SA_SETNAME_C,
		SA_SETNAME_D,
		SA_SETNAME_E,
		SA_SETNAME_F,
		SA_SETNAME_G,
		SA_SETNAME_H,
		SA_SETNAME_I,
		SA_SETNAME_J,
		SA_SETNAME_K,
		SA_SETNAME_L,
		SA_SETNAME_M,
		SA_SETNAME_N,
		SA_SETNAME_O,
		SA_SETNAME_P,
		SA_SETNAME_Q,
		SA_SETNAME_R,
		SA_SETNAME_S,
		SA_SETNAME_T,
		SA_SETNAME_U,
		SA_SETNAME_V,
		SA_SETNAME_W,
		SA_SETNAME_X,
		SA_SETNAME_Y,
		SA_SETNAME_Z,
		SA_TEMPL2TFTPL,
		SA_BEGINBSUB,
		SA_LOADBSUB,
		SA_ADDMCONST,
		SA_ADDMVOL,
		SA_ADDMFAR,
		SA_ADDMHUGE,
		SA_LOADMSNUM,
		SA_NUMTORTTIBCD,
		SA_NUMTOTYPE,
		SA_BORLANDNORMALIZEPARNAME,
		SA_BORLANDID,
		SA_LOADBORLANDSUB,
		SA_BORLANDARR,
		SA_END
	};

	/**
	* @brief Structure of an element in an internal LL table.
	* @param n Rule number. Numbered from 1. 0 is reserved for "no rule", which indicates a syntax error.
	* @param s Semantic action to be done when this LL element is used.
	*/
	struct llelem_t {
		llelem_t(unsigned int i, semact ss) :
			n(i),
			s(ss)
		{}
		llelem_t() {}
		unsigned int n = 0;
		semact s = SA_NULL;
	};

	/**
	* @brief Struct used to describe a rule boundaries in an internal LL table.
	* @param offset Offset from the start of ruleelements array.
	* @param size Number of elements in the current rule.
	*/
	struct ruleaddr_t {
		ruleaddr_t(unsigned int o, unsigned int s) :
			offset(o),
			size(s)
		{}
		unsigned int offset = 0;
		unsigned int size = 0;
	};

	/**
	 * @brief Structure of a grammar rule.
	 * @param n Number of the rule. Numbered from 1. 0 is reserved for "no rule", which indicates a syntax error.
	 * @param left The left side of the rule, consisting of only one non-terminal.
	 * @param right The right side of the rule, which is a sequence of terminals or non-terminals. May be empty.
	 */
	struct rule_t {
		unsigned int n = 0;
		gelem_t left;
		std::vector<gelem_t> right;
	};

	/**
	 * @brief Types of substitution expansion.
	 */
	enum subtype {
		ST_NULL = 0,
		ST_STUNQ,
		ST_SSNEST,
		ST_SSNO
	};

	/**
	 * @brief States of the FSM for parsing grammar rules from a file.
	 */
	enum fsmstate {
		S_START = 0, //beginning of a line
		S_NT_LEFT, //non-terminal on the left side
		S_OP1, //:
		S_OP2, //:
		S_OP3, //=
		S_RIGHT, //right side
		S_NT_RIGHT, //non-terminal on the right side
		S_T, //terminal on the right side
		S_QT, //quoted terminal on the right side
		S_QT_ESC, //escape sequence of a quoted terminal
		S_IGNORE, //ignore the rest of the line
		S_ERROR, //error ocurred
		S_NULL //just a NULL terminator for the array of final states
	};

	/**
	 * @brief Class for comparing two grammar element structures. Used in std::set of grammar elements.
	 */
	class comparegelem_c {
	public:
		/**
		 * @brief Comparison function for two grammar element structures
		 * @param t1 First grammar element.
		 * @param t2 Second grammar element.
		 */
		bool operator() (const gelem_t t1, const gelem_t t2) const {
			//if types don't match, terminal is less than non-terminal
			if (t1.type!= t2.type) {
				return (t1.type == GE_TERM)?true:false;
			}
			//for two terminals, compare their byte values
			if (t1.type == GE_TERM) {
				return t1.t < t2.t;
			}
			//for two non-terminals, compare the non-terminal name strings
			else {
				return t1.nt < t2.nt;
			}
		}
	};

	/**
	 * @brief Struct for internal grammar.
	 */
	struct igram_t {
		igram_t(unsigned int tsx, unsigned int rax, unsigned int rex, unsigned int lx, unsigned int ly,
			gelem_t r, unsigned char* ts, ruleaddr_t* ra, gelem_t* re, llelem_t** lt) :
			terminal_static_x(tsx),
			ruleaddrs_x(rax),
			ruleelements_x(rex),
			llst_x(lx),
			llst_y(ly),
			root(r),
			terminal_static(ts),
			ruleaddrs(ra),
			ruleelements(re),
			llst(lt)
		{}
		igram_t() {}
		//dimensions of the arrays
		unsigned int terminal_static_x = 0;
		unsigned int ruleaddrs_x = 0;
		unsigned int ruleelements_x = 0;
		unsigned int llst_x = 0; //first one
		unsigned int llst_y = 0; //second one
		//root element
		gelem_t root;
		//the arrays
		unsigned char* terminal_static = nullptr; //array of used terminals
		ruleaddr_t* ruleaddrs = nullptr; //structures defining offset and size of each rule in the ruleelements table
		gelem_t* ruleelements = nullptr; //all elements of all rules
		llelem_t** llst = nullptr; //the LL table
	};

	/**
	* @brief The struct variable containing pointers to internal grammar data.
	*/
	cGram::igram_t internalGrammarStruct;

	//FSM for parsing external rules
	static const fsmstate fsm_final[];
	static const gelem_t T_EOF;


	bool internalGrammar = false;
	std::string compiler;

	/*
	 * Variables used for generation of new internal grammars
	 */
	std::string createIGrammar; //if this thing is on, new internal grammar will be generated from external grammar file
	unsigned int newIG_terminal_static_x = 0;
	std::size_t newIG_ruleaddrs_x = 0;
	std::size_t newIG_ruleelements_x = 0;
	std::size_t newIG_llst_x = 0;
	std::size_t newIG_llst_y = 0;
	std::string newIG_root;
	std::string newIG_terminal_static;
	std::string newIG_ruleaddrs;
	std::string newIG_ruleelements;
	std::string newIG_llst;


	/*
	 * Variables for parsed external grammar
	 */
	std::vector<rule_t> rules;
	std::map<std::string,bool> empty;
	std::map<std::string,std::set<gelem_t,comparegelem_c>> first;
	std::map<std::string,std::set<gelem_t,comparegelem_c>> follow;
	std::map<unsigned int,std::set<gelem_t,comparegelem_c>> predict;
	std::map<std::string,std::map<char,std::pair<unsigned int, semact>>> ll;

	std::vector<unsigned char> terminals;
	std::vector<std::string> nonterminals;

	size_t lex_position = 0; //position in the source file
	std::fstream *source = nullptr; //pointer to the input filestream

	/*
	 * methods
	 */
	errcode loadfile(const std::string filename);
	bool is_final(fsmstate s); //is s a final state of fsm?
	char getc();
	bool eof();
	bool lf();
	errcode getgrammar(const std::string filename);
	bool copyset(std::set<gelem_t,comparegelem_c> & src, std::set<gelem_t,comparegelem_c> & dst);
	void genempty();
	void genfirst();
	bool getempty(std::vector<gelem_t> & src);
	std::set<gelem_t,comparegelem_c> getfirst(std::vector<gelem_t> & src);
	llelem_t getllpair(std::string nt, unsigned int ntst, unsigned char t);
	void genfollow();
	void genpredict();
	errcode genll();
	errcode genconstll();
	void genllsem();
	errcode analyze(std::string input, cName & pName);
	std::string subanalyze(const std::string input, cGram::errcode *err);
	semact getsem(const std::string input);
	void *getbstpl(cName & pName);
	void *getstrtpl(cName & pName);
	bool issub(std::string candidate,std::vector<std::string> & vec);
	void showsubs(std::vector<std::string> & vec);
	long int b36toint(std::string x);
	void * copynametpl(void * src);
	public:
		//constructor
		cGram();
		//destructor
		virtual ~cGram();
		errcode initialize(std::string gname, bool i = true);
		errcode parse(const std::string filename);
		cName *perform(const std::string input, errcode *err);
		void demangleClassName(const std::string& input, cName* retvalue, cGram::errcode& err_i);
		void showrules();
		void showempty();
		void showfirst();
		void showfollow();
		void showpredict();
		void showll();
		unsigned int isnt(std::vector<std::string> & nonterminals, std::string nonterminal);
		unsigned int ist(std::vector<unsigned char> & terminals, unsigned char terminal);

		void resetError() {errString = "";}
		std::string errString; //string containing last error message
		bool errValid = false; //is the gParser valid? false if it has not been properly initialized yet
		bool SubAnalyzeEnabled = false; //enable substitution analysis for GCC demangler?
		void setSubAnalyze(bool x);

		errcode generateIgrammar(const std::string inputfilename, const std::string outputname);
		std::string generatedTerminalStatic;

}; //class cGram

} // namespace demangler

#endif
