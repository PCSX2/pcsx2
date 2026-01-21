struct Function
{
	unsigned int address;        // Starting address of the function.
	unsigned int return_address; // Ending address of the function. (Presumably.)

	int arguments;    // Registers read directly from the caller (arguments).

	int           replacements;
	char*         replacement_code;
	unsigned int* replacement_address;

	char analysed;
};


#define VALUETYPE_REGISTER    1
#define VALUETYPE_UINT        2
#define VALUETYPE_VARIABLE    4 // Will this become bit-based? Who knows!
#define VALUETYPE_CONDITIONAL 8

struct Value
{
	const char* operation; // "+", "/", etc.
	unsigned char value_type;
	unsigned int data;
};


struct Register
{
	Value value_chain[30]; // You'd have to be a pretty crazy programmer to manage more than 30 operations in one assignment.
	int valuechain_length;

	bool absolute_value;

	unsigned int last_write;
	unsigned int last_read;

	int TEMP_REGID;
};

extern Function* functions;
extern int       function_count;

void Add_Value(Register* reg, const char* operation, char value_type, unsigned int value, unsigned int address);

void Add_Replacement (Function* func, unsigned int address, char* string);
bool Get_Replacement (Function* func, unsigned int address, char* string);

void Get_Register_Value(Register* reg, char* return_string);

void Find_Labels();

void Do_Assignment(unsigned int code, Register* registers, int i);
