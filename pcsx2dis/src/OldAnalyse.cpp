#include "Main.h"
#include "Processor.h"
#include "Logical.h"
#include "Analyse.h"

#include <iostream> // TODO: Remove this eventually!

Function* functions;
int       function_count = 0;

Register* last_affected_register;

Register* global_registers;

inline bool Check_Surroundings(unsigned int address)
{
	if (address - 4 < 0 || address + 4 > memlen8)
		return 1;
		
	int surrounding[2] = {GET_OPERATION(mem[(address >> 2) - 1]), GET_OPERATION(mem[(address >> 2) + 1])};

	for (char i = 0; i < 2; i ++)
	{
		if (surrounding[i] == JAL || surrounding[i] == J || surrounding[i] == JR || 
			surrounding[i] == BEQ || surrounding[i] == BNE)
			return 0;
	}

	return 1;
}

void Find_Functions()
{
	int templist_length = (0x01000000 / 8) + 1;
	unsigned int* temp_list = (unsigned int*) malloc(templist_length * 4);

	for (int i = 0; i < templist_length; i ++)
	{
		if ((i & 0x07) == 0) // Clear this part before adding list items.
			temp_list[i >> 3] = 0x00000000;

		if (mem[i] == 0x03E00008)
			temp_list[i >> 3] |= 1 << (i & 0x07);
	}

	for (unsigned int i = 0; i < 0x01000000 / 4; i ++)
	{
		unsigned int data = mem[i];
		unsigned int address = GET_ADDRESS(data);
		bool already_called = 0, is_overlapping = 0;

		if (GET_OPERATION(data) != JAL)
			continue;

		// Some random checks for validity.
		// The next line must not be a jump; that wouldn't make any sense.
		if (! Check_Surroundings(i << 2))
			continue;

		// To perhaps do: Change this, so that something that shares the same return_address as an earlier function and also has a lower 
		// starting address, can replace the old one? Hard to decide.
		for (int l = 0; l < function_count; l ++)
		{
			if (functions[l].address == address)
				already_called = 1;
			else if (functions[l].address < address && functions[l].return_address >= address)
				is_overlapping = 1;
			else
				continue;
			break; // Wow, look at this fancy hack!
		}

		if (! already_called && ! is_overlapping)
		{
			Function* new_function;
			unsigned int return_address;

			bool skip = 0;
			for (int l = address / 4; ; l ++)
			{
				if (l >= (0x01000000 / 4) || l >= memlen32) // Automatically discludes any function that starts at >=0x01000000.
				{
					skip = 1;
					break;
				}

				if ((temp_list[l >> 3] & 1 << (l & 0x07)))
				{
					return_address = l * 4;
					break;
				}
			}

			if (skip)
				continue;

			functions = (Function*) realloc(functions, (function_count + 1) * sizeof (Function));
			new_function = &functions[function_count ++];

			new_function->address = address;
			new_function->return_address = return_address;
			new_function->replacements = 0;

			new_function->analysed = 0;

			if (! (function_count % 100))
				printf("ADDED %08X (at %08X)\n", new_function->address, i * 4);
		}
	}

	printf("Total functions: %i\n", function_count);

	free(temp_list);
}

void Find_Function_Arguments()
{
	for (int l = 0; l < function_count; l ++)
	{
		Function* func = &functions[l];
		int written_registers = 0x00000000; // Bit-based record of registers that have been written to.

		func->arguments = 0;

		for (unsigned int i = func->address / 4; i < func->return_address / 4; i ++)
		{
			unsigned int code = mem[i];

			int id = GET_OPERATION(code);

			if (! (id == SQ) && ! (id == SD)) // Also had (! id == LQ), but that isn't needed, is it?
											  // SD comes up in some functions, don't know why.
			{
				if (notes[id] & READS_RS && ! (written_registers & (1 << GET_RS(code))))
					func->arguments |= 1 << GET_RS(code);
				if (notes[id] & READS_RT && ! (written_registers & (1 << GET_RT(code))))
					func->arguments |= 1 << GET_RT(code);
				if (notes[id] & READS_RD && ! (written_registers & (1 << GET_RD(code))))
					func->arguments |= 1 << GET_RD(code);
			}

			if (notes[id] & WRITES_RS) written_registers |= 1 << GET_RS(code);
			if (notes[id] & WRITES_RT) written_registers |= 1 << GET_RT(code);
			if (notes[id] & WRITES_RD) written_registers |= 1 << GET_RD(code);
		}

		func->arguments &= 0x0FFFFFFE; // Removes zero, gp, sp, fp, and ra.

		char new_label[256];

		sprintf(new_label, "FNC_%08X(", func->address);

		bool last_arg = 0;
		for (int i = 0; i < 32; i ++)
		{
			if (func->arguments & (1 << i))
			{
				sprintf(&new_label[String_Length(new_label) - 1], "%s%s", last_arg ? ", " : "", registers[i]); // Appending.
				last_arg = 1;
			}
		}

		sprintf(&new_label[String_Length(new_label) - 1], ")");

		//sprintf(&new_label[String_Length(new_label) - 1], " [%i]", ((func->return_address - func->address) / 4) + 1);

		AddLabel(new_label, func->address);
	}
}

void Analyse_Function(unsigned int address, Function* func)
{
/*	int written_registers = 0x00000000; // Bit-based record of registers that have been written to.

	func->address = address;
	func->arguments = 0;

	func->replacements = 0;
	func->replacement_code    = NULL;
	func->replacement_address = NULL;

	unsigned int last_arguments = 0x00000000;

	for (unsigned int i = address / 4; i < mem_length / 4; i ++)
	{
		unsigned int code = mem[i];

		int id = GET_OPERATION(code);

		if (! (id == 0x5D || id == 0x5E))
		{
			if (notes[id] & READS_RS && ! (written_registers & (1 << GET_RS(code))))
				func->arguments |= 1 << GET_RS(code);
			if (notes[id] & READS_RT && ! (written_registers & (1 << GET_RT(code))))
				func->arguments |= 1 << GET_RT(code);
			if (notes[id] & READS_RD && ! (written_registers & (1 << GET_RD(code))))
				func->arguments |= 1 << GET_RD(code);
		}

		if (notes[id] & WRITES_RS) written_registers |= 1 << GET_RS(code);
		if (notes[id] & WRITES_RT) written_registers |= 1 << GET_RT(code);
		if (notes[id] & WRITES_RD) written_registers |= 1 << GET_RD(code);

		if (code == 0x03E00008)
		{
			func->return_address = i * 4;
			break;
		}
	}

	func->arguments &= 0x8FFFFFFE; // Removes zero, gp, sp, and fp.

	char new_label[256];

	sprintf(new_label, "FNC_%08X(", func->address);

	bool last_arg = 0;
	for (int i = 0; i < 32; i ++)
	{
		if (func->arguments & (1 << i))
		{
			sprintf(&new_label[String_Length(new_label) - 1], "%s%s", last_arg ? ", " : "", registers[i]); // Appending.
			last_arg = 1;
		}
	}

	sprintf(&new_label[String_Length(new_label) - 1], ")");

	Add_Label(new_label, func->address);*/

	Register registers[32];

	global_registers = registers;

	// Reset the registers.
	for (int i = 0; i < 32; i ++)
	{
		registers[i].absolute_value = 0;
		registers[i].valuechain_length = 0;
		registers[i].last_write = func->address;
		registers[i].last_read  = func->address;
		registers[i].TEMP_REGID = i;
	}

	// Scan through the function, collecting register values and "optimising" them. (Optimising for readability, that is.)
	for (int i = func->address / 4; i < func->return_address / 4; i ++)
	{
		unsigned int code = mem[i];

		int id = GET_OPERATION(code);

		char read_regs[3];
		int reads;

		reads = Get_Reads(code, read_regs);

		for (int r = 0; r < reads; r ++)
		{
			// If it's only writing to itself, then it doesn't matter.
			if (Writes(code, read_regs[r]) && ! (notes[GET_OPERATION(code)] & REPLACES))
				continue;

			// Otherwise, use the register's last_write variable to put the replacement back.
			char value_string[512];

			Get_Register_Value(&registers[read_regs[r]], value_string);

			Add_Replacement(func, registers[read_regs[r]].last_write, value_string);
		}

		char write_regs[3] = {-1, -1, -1};
		char writes;

		Get_Writes(code, write_regs);
		writes = write_regs[0];

		// What are we doing here? We're looking at whichever register has been changed during this execution.
		// Then we look for registers that read from this in the past. That was then and this is now. Their values will still have the 
		// register that has just been changed, so if nothing else can be done, we'll just have to clear the reader's value and set it to 
		// the mysterious "reader = reader".
		// TODO: "If nothing else can be done" - something else could be done. Add it.
		if (writes != -1)
		{
			for (int r = 0; r < 32; r ++)
			{
				Register* reg = &registers[r];

				for (int v = 0; v < reg->valuechain_length; v ++)
				{
					Value* val = &reg->value_chain[v];

					// If the register that has just been changed was read..
					if (val->value_type == VALUETYPE_REGISTER && val->data == writes || 
						val->value_type == VALUETYPE_VARIABLE && ((val->data & 0xFFFF) == writes) || 
						val->value_type == VALUETYPE_CONDITIONAL && ((val->data & 0xFF) == writes || ((val->data >> 8) & 0xFF) == writes))
					{
						char value_string[512];

						Get_Register_Value(reg, value_string);

						Add_Replacement(func, reg->last_write, value_string);

						// Register is now somewhat unknown, but we can save it from a terrible fate by calling it... itself!
						// Example: a0 was s1 + 4 but s1 changed, 4 is then added again to a0 so now a0 = a0 + 0004.
						reg->valuechain_length = 0;
						Add_Value(reg, "=", VALUETYPE_REGISTER, r, 0);
			
						break;
					}
				}
			}
		}

		char value_string[512]; // If someone's setting their values any more than this, they're probably insane.
								// Likewise, not setting a real limit is also insane. You know what they say - fight fire with fire!
		Do_Assignment(code, registers, i);

		// Add the value of this register to the code list.;
		Get_Register_Value(&registers[writes], value_string);

		Add_Replacement(func, i * 4, value_string);

/*		Register* reg = last_affected_register;
		int ADDRRRR = i * 4;
		for (int i = 1; i < reg->valuechain_length; i ++)
		{
			Value* current_value = &reg->value_chain[i];
			Value* last_value    = &reg->value_chain[i - 1];

			if (current_value->value_type == VALUETYPE_UINT && last_value->value_type == VALUETYPE_UINT && 
				(String_Compare(last_value->operation, "=") || String_Compare(last_value->operation, "+")) && 
				String_Compare(current_value->operation, "+"))
			{
				printf("Cutting off some inefficiency like scissors in an art class... (Address: %08X)\n", ADDRRRR);

				last_value->data = last_value->data + current_value->data;
			}
		}*/
	}

	func->analysed = 1;
}

void Do_Assignment(unsigned int code, Register* registers, int i)
{
	#define Add_Value(a, b, c, d) Add_Value(a, b, c, d, i * 4)
	int id = GET_OPERATION(code);

	if (id == LUI)
		Add_Value(&registers[GET_RT(code)], "=", VALUETYPE_UINT, GET_IMMEDIATE(code) << 16);

	if (id == ADDI || id == ADDIU)
	{
		if (GET_RT(code) == GET_RS(code))
			Add_Value(&registers[GET_RT(code)], "+", VALUETYPE_UINT, GET_IMMEDIATE(code));
		else
		{
			Add_Value(&registers[GET_RT(code)], "=", VALUETYPE_REGISTER, GET_RS(code));

			if (GET_IMMEDIATE(code) != 0x0000) // No point if it's 0.
				Add_Value(&registers[GET_RT(code)], "+", VALUETYPE_UINT, GET_IMMEDIATE(code));
		}
	}

	if (id == DADDU || id == DADD || id == ADDU || id == ADD)
	{
		char add_reg = -1;

		if (GET_RD(code) == GET_RS(code))
			add_reg = GET_RT(code);
		if (GET_RD(code) == GET_RT(code))
			add_reg = GET_RS(code);

		if (add_reg != -1)
			Add_Value(&registers[GET_RD(code)], "+", VALUETYPE_REGISTER, add_reg);
		else
		{
			Add_Value(&registers[GET_RD(code)], "=", VALUETYPE_REGISTER, GET_RS(code));
			Add_Value(&registers[GET_RD(code)], "+", VALUETYPE_REGISTER, GET_RT(code)); // TODO: Check over.. I don't think this is right.
		}
	}

	if (id == SLL || id == DSLL || id == SRL || id == DSRL || id == SRA || id == DSRA)
	{
		char* op;

		switch (id)
		{
			case SLL:
			case DSLL:
				op = "<<";
				break;
			case SRL:
			case DSRL:
			case SRA:
			case DSRA:
				op = ">>";
				break;
		}

		if (GET_RD(code) != GET_RT(code))
			Add_Value(&registers[GET_RD(code)], "=", VALUETYPE_REGISTER, GET_RT(code));

		Add_Value(&registers[GET_RD(code)], op, VALUETYPE_UINT, GET_SHIFT(code));
	}

	if (id == ANDI || id == ORI || id == XORI)
	{
		char* op;

		switch (id)
		{
			case ANDI:
				op = "&";
			break;
			case ORI:
				op = "|";
			break;
			case XORI:
				op = "^";
			break;
		}

		if (GET_RT(code) != GET_RS(code))
			Add_Value(&registers[GET_RT(code)], "=", VALUETYPE_REGISTER, GET_RS(code));

		Add_Value(&registers[GET_RT(code)], op, VALUETYPE_UINT, GET_IMMEDIATE(code));
	}

	if (id == LB || id == LBU)
		Add_Value(&registers[GET_RT(code)], "=", VALUETYPE_VARIABLE, (GET_IMMEDIATE(code) << 16) + GET_RS(code));

	if (id == MOVZ || id == MOVN)
	{
		Add_Value(&registers[GET_RD(code)], "=", VALUETYPE_CONDITIONAL, (GET_RT(code)) | (GET_RS(code) << 8) | ((id == MOVN) << 16));
	}
	#undef Add_Value
}

void Add_Replacement(Function* func, unsigned int address, char* string)
{
	// If we already have a replacement... then replace the replacement! Genius, no?
	for (int i = 0; i < func->replacements; i ++)
	{
		if (func->replacement_address[i] == address)
		{
			Copy_String(string, &func->replacement_code[i * 512]);

			return;
		}
	}

	// Re-alocate the lists and copy the string.
	func->replacement_code    = (char*)         realloc(func->replacement_code,    (func->replacements + 1) * 512);
	func->replacement_address = (unsigned int*) realloc(func->replacement_address, (func->replacements + 1) *   4);

	Copy_String(string, &func->replacement_code[func->replacements * 512]);
	func->replacement_address[func->replacements] = address;

	func->replacements ++;
}

bool Get_Replacement(Function* func, unsigned int address, char* string)
{
	for (int i = 0; i < func->replacements; i ++)
	{
		if (func->replacement_address[i] == address)
		{
			if (string != NULL)
				Copy_String(&func->replacement_code[i * 512], string);

			return 1;
		}
	}

	return 0;
}

void Add_Value(Register* reg, const char* operation, char value_type, unsigned int value, unsigned int address)
{
	if (value_type == VALUETYPE_REGISTER && value == 0x00 && 							    // Register is 0, and adding/subtracting 0 doesn't
		(String_Compare((char*) operation, "+") || String_Compare((char*) operation, "-"))) // do that much. Dividing, on the other hand...
		return;

	if (String_Compare((char*) operation, "="))
		reg->valuechain_length = 0; // We're setting the register to something entirely different.

	reg->value_chain[reg->valuechain_length].operation = operation;
	reg->value_chain[reg->valuechain_length].value_type = value_type;
	reg->value_chain[reg->valuechain_length].data = value;

	reg->valuechain_length ++;

	reg->last_write = address;
	if (value_type == VALUETYPE_REGISTER || value_type == VALUETYPE_VARIABLE)
		global_registers[(value & 0xFFFF)].last_read = address;

	last_affected_register = reg;
}

void Get_Register_Value(Register* reg, char* return_string)
{
	if (reg->valuechain_length <= 0)
	{
		// Say register = register, just in case something has gone horribly wrong.
		sprintf(return_string, "%s = %s (ERROR)", registers[reg->TEMP_REGID], registers[reg->TEMP_REGID]);
		return;
	}

	#define APPEND &return_string[String_Length(return_string) - 1]

	sprintf(return_string, "%s ", registers[reg->TEMP_REGID]);

	for (int l = 0; l < reg->valuechain_length; l ++)
	{
		unsigned int data = reg->value_chain[l].data;

		if (l == 0 && reg->valuechain_length > 1)
		{
			if (String_Compare(reg->value_chain[0].operation, "=") && 
				(String_Compare(reg->value_chain[1].operation, "+") || String_Compare(reg->value_chain[1].operation, "-") || 
				 String_Compare(reg->value_chain[1].operation, "&") || String_Compare(reg->value_chain[1].operation, "|") || 
				 String_Compare(reg->value_chain[1].operation, "^")))
			{
				int valtype = reg->value_chain[1].value_type;

				if (valtype == VALUETYPE_UINT)
				{
					sprintf(APPEND, "%s= %08X", reg->value_chain[1].operation, reg->value_chain[1].data);
					l = 1; // Basically make l = 2 next cycle.
					continue;
				}
			}
		}

		sprintf(APPEND, "%s ", reg->value_chain[l].operation);

		if (reg->value_chain[l].value_type == VALUETYPE_UINT)
			sprintf(APPEND, "%08X", data);
		if (reg->value_chain[l].value_type == VALUETYPE_REGISTER)
			sprintf(APPEND, "%s", registers[data]);
		if (reg->value_chain[l].value_type == VALUETYPE_VARIABLE)
		{
			if (reg->value_chain[l].data >> 16)
				sprintf(APPEND, "*(%04X + %s)", data >> 16, registers[data & 0xFFFF]);
			else
				sprintf(APPEND, "*(%s)", registers[data & 0xFFFF]);
		}
		if (reg->value_chain[l].value_type == VALUETYPE_CONDITIONAL)
		{
			if (reg->value_chain[l].data << 16 == 1)
				sprintf(APPEND, "(%s ? %s : %s)", registers[data & 0xFF], registers[(data >> 8) & 0xFF], registers[reg->TEMP_REGID]);
			else
				sprintf(APPEND, "((! %s) ? %s : %s)", registers[data & 0xFF], registers[(data >> 8) & 0xFF], registers[reg->TEMP_REGID]);
		}

		sprintf(APPEND, " ");
	}
}

void Find_Labels()
{
	for (addr i = 0x00000000; i < memlen8; i += 4)
	{
		char label[256]; // TODO: Sort out the character limit.
		int  label_length;
		bool got_label = 0;

		for (int l = 0; l < 256; l ++)
		{
			if (i + l >= memlen8)
				break;

			char character = mem8[i + l];

			label[l] = character;

			if (character == 0 && l > 4 && mem8[i - 1] == 0) // Make sure the previous character, before this string, is 0.
																   // We don't really want labels that are partially cut-off.
			{
				got_label = 1;

				label_length = ((l / 4) * 4) + 1;

				break;
			}

			if ((character < 0x20 && character != 0x0D && character != 0x0A) || 
				character >= 0x7F)
				break;
		}

		if (got_label)
		{
			// Add some double-quotes before adding the label.
			char add_label[256 + 5];

			sprintf(add_label, "\"%s\"", label);

			AddLabel(add_label, i);

			// Change the data type of this portion of the code.
			for (int l = i >> 2; l <= (i >> 2) + (label_length >> 2) + 1; l ++)
				lines[l].datatype = DATATYPE_BYTE;

			i += label_length - 1;
		}
	}
}