#include <cstdio>
#include <cstdlib>

#include "Main.h"
#include "Logical.h"
#include "Processor.h"
#include "Analyse.h"
#include "Windows.h"

void Convert_Line(unsigned int code, char* result);

void Format_String(char* string, unsigned int code);

bool Reads  (unsigned int code, char reg);
bool Writes (unsigned int code, char reg);

int Is_Conditional(unsigned int position, unsigned int end);


bool Get_Replacement(Function* func, unsigned int address, char* string);

extern int viewlist_length, viewlist_itemcount;

/*void Convert(unsigned int _start, unsigned int _end)
{
	int label_list[100]; // It couldn't get much bigger than 100, surely.
	int labellist_length = 0;

	int start = _start / 4;
	int end   = _end   / 4;

	for (int i = 0; i < numlabels; i ++)
	{
		if (labels[i].address >= _start && labels[i].address <= _end)
			label_list[labellist_length ++] = i;
	}

	for (int i = start; viewlist_length < viewlist_itemcount; i ++)
	{
		char* label = "[Label]";
		bool got_label = 0;
		char test[100];
		char buffer[1024];
		int ii;

		if (i >= memlen32 || i < 0)
			break;

		/*ii = Is_Conditional(i, end); // CAUSES CRASH! But to be honest I forgot why I added this in the first place. Bah.

		if (ii != i)
		{
			i = ii - 1;
			continue;
		}*//*

		Convert_Line(mem[i], test);

		sprintf(buffer, "%s\n"/* (operation %02X)\n"*//*, test, GET_OPERATION(mem[i]));

		for (int l = 0; l < labellist_length; l ++)
		{
			if (labels[label_list[l]].address == i * 4)
			{
				label = labels[label_list[l]].string;
				got_label = 1;
			}
		}

		Function* func = NULL;
		for (int l = 0; l < function_count; l ++)
		{
			if ((i * 4) >= functions[l].address && (i * 4) <= functions[l].return_address)
				func = &functions[l];
		}

		if (func != NULL && func->analysed)
		{
			char replacement[512];

			if (Get_Replacement(func, i * 4, replacement))
			{
//				if (String_Compare(replacement, "")) // Thanks to its ultra efisyensy, the function cut out a line altogether. Yay!
//					continue;						 // Move on to the next line.

				AddItem(DATATYPE_CODE, i * 4, mem[i], replacement, buffer);
				continue;
			}
			else
				continue;
		}

		if (lines[i].datatype == DATATYPE_CODE)
			AddItem(DATATYPE_CODE, i * 4, mem[i], label, buffer);
		if (lines[i].datatype == DATATYPE_BYTE)
		{
			for (int l = 0; l < 4; l ++)
			{
				char character = mem8[(i * 4) + l];

				AddItem(DATATYPE_BYTE, (i * 4) + l, character, ((! l) && got_label) ? label : (char*) "", "");
			}
		}
	}
}*/

void Convert_Line(unsigned int code, char* result)
{
	int id = GET_OPERATION(code);
	/*int id = code & 0xFC000000;

	if (! id)
		id = code & 0x0000003F;*/

	char* format = operations[id];
	bool hack = 0; // Hack will make us replace rt with the actual register at rs to make the code more readable. It's for the best, 'kay?

	if (format == NULL)
	{
		Copy_String("", result);

		return;
	}

	if (id == 0 && GET_RT(code) == 0 && GET_RD(code) == 0) // TODO: Add _h to this check.
	{
		Copy_String("nop", result);

		return;
	}

	if (sc_operations[id] != NULL)
	{
		if (id <= 0x3F && GET_RD(code) == GET_RS(code))
			format = sc_operations[id];
		else if (id > 0x3F && GET_RT(code) == GET_RS(code))
			format = sc_operations[id];
	}

	if (sc2_operations[id] != NULL)
	{
		if (GET_RS(code) == 0x00)
			format = sc2_operations[id];
		if (GET_RT(code) == 0x00 && id <= 0x3F) // TOFIX: subtraction operations will not have the exact same effect.
		{
			format = sc2_operations[id];

			hack = 1;
		}
	}

	char immediate_value[5];

	Copy_String(format, result);

	if (id > 0x3F) // No need to convert the immediate value to a string if the instruction isn't an immediate. DUH.
		Hex_to_String(GET_IMMEDIATE(code), immediate_value, 4);

	if (hack)
		String_Replace(result, "rt", (char*) registers[GET_RS(code)]);

	Format_String(result, code);
}

void Format_String(char* string, unsigned int code)
{
	char immediate_value[5];

	String_Replace(string, "rs", (char*)   registers[GET_RS(code)]);
	String_Replace(string, "rd", (char*)   registers[GET_RD(code)]);
	String_Replace(string, "rt", (char*)   registers[GET_RT(code)]);
	String_Replace(string, "fs", (char*) f_registers[GET_RS(code)]);
	String_Replace(string, "fd", (char*) f_registers[GET_RD(code)]);
	String_Replace(string, "ft", (char*) f_registers[GET_RT(code)]);

	Hex_to_String(GET_IMMEDIATE(code), immediate_value, 4);
	String_Replace(string, "_i", immediate_value);
}

int Is_Conditional(unsigned int position, unsigned int end)
{
	int operation = GET_OPERATION(mem[position]);

	if (operation < 0x40 || operation > 0x46)
		return position;

	if (GET_IMMEDIATE(mem[position]) < 0x0000)
		return position;

	// Check if it's a return.
	bool is_return = 0;
	unsigned int returnval_code = 0xFFFFFFFF;
	for (int i = position + GET_IMMEDIATE(mem[position]); i < end; i ++)
	{
		unsigned int code = mem[i];
		int id = GET_OPERATION(code);

		if (id == 0x08 && Reads(code, 31)) // jr ra
		{
			is_return = 1;
			break;
		}

		if ((id == 0x5D || id == 0x62 || id == 0x76) && Reads(code, 29)) // lq, lw or ld for sp
			continue;

		if (id == 0x47 || id == 0x48 || id == 0x57 || id == 0x58)
		{
			if (returnval_code != 0xFFFFFFFF)
				break;

			returnval_code = code;
		}

		break;
	}

	if (is_return)
	{
		char* ifcode = NULL;

		switch (operation)
		{
			case 0x40: // beq
				ifcode = "if (%s == %s)";
			break;
			case 0x41: // bne
				ifcode = "if (%s != %s)";
		}

		printf(ifcode, registers[GET_RS(mem[position])], registers[GET_RT(mem[position])]);

		if (returnval_code != 0xFFFFFFFF)
		{
			char test[100];

			printf("\n");

			Convert_Line(returnval_code, test);

			printf("{\n	%s\n", test);
			printf("	return v0\n}\n", test);
		}
		else
			printf(" return;\n");
	}

	return position;
}
