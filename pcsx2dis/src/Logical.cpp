#include <windows.h>
#include <cstdio>

#include "Logical.h"

char Get_Needed_Bytes(int number)
{
    for (char i = 1; ; i ++)
    {
        if (number < (unsigned long long int) 1 << (i << 3)) // Equivalent to 256 to the power of i.
            return i;
    }
}

bool Write_Bytes(char* array, int array_part, int number, char bytes)
{
    if (Get_Needed_Bytes(number) > bytes || ! bytes)
        return 0;

    for (char i = 0; i < bytes; i ++)
        array[array_part + i] = ((unsigned long long int) number >> (i << 3)) & 0xFF;

    return 1;
}

int Read_Bytes(char* array, int array_part, char bytes)
{
    int number = 0;

    for (char i = 0; i < bytes; i ++)
        number += ((unsigned char) array[array_part + i]) << (i << 3);

    return number;
}

void Hex_to_String(unsigned int hex, char* string, int length)
{
	int i = 0;
	int lensubone;

	if (length == -1)
	{
		unsigned int temp_hex = hex;

		length = 0;

		do
		{
			length ++;

			temp_hex >>= 4;
		} while (temp_hex > 0);
	}

	lensubone = length - 1;

	do
	{
		char digit = (hex >> ((lensubone - i) << 2)) & 0xF;

		if (digit < 0x0A)
			string[i] = '0' + digit;
		else
			string[i] = 0x37 + digit; // 0x37 is 'A' minus 0x0A.

		i ++;
	} while (i < length);

	string[i] = 0;
}

void Int_to_String(unsigned int number, char* string)
{
	int length = 0;

	int i = 0;

	unsigned int temp_number = number;
	long long    divisor = 1;

	do
	{
		length ++;

		temp_number /= 10;
		divisor     *= 10;
	} while (temp_number > 0);

	do
	{
		divisor /= 10;

		char digit = (number / divisor);

		number -= digit * divisor;

		string[i] = '0' + digit;

		i ++;
	} while (i < length);

	string[i] = 0;
}

#define Return_Error(err) {if (error != NULL) {*error = err;} return 0xFFFFFFFF;}

unsigned int String_to_Hex(char* string, char* error)
{
	int i;

	unsigned int result = 0;

	for (i = 0; i < 8; i ++)
	{
		if (! string[i])
			return result;

		char digit = string[i];

		result <<= 4;

		if      (digit >= '0' && digit <= '9')
			result += digit - '0';
		else if (digit >= 'A' && digit <= 'F')
			result += digit - 0x37;
		else if (digit >= 'a' && digit <= 'f')
			result += digit - 0x57;
		else
			Return_Error(1);
	}

	// If the string is null-terminated at 8, then nothing's wrong. Otherwise the number's too long.
	if (! string[8])
		return result;

	Return_Error(2);
}

unsigned int String_to_Int(char* string, char* error)
{
	int i;

	unsigned int result = 0;

	for (i = 0; i < 10; i ++)
	{
		if (! string[i])
			return result;

		char digit = string[i];
		unsigned int last_result = result;

		result *= 10;
		result += digit - '0';

		if (digit < '0' || digit > '9')
			Return_Error(1);

		if (result < last_result) // Number has overflown..! Too big!
			Return_Error(3);
	}

	if (! string[10]) // This number is also too big.. but the loop was too slow to catch it.
		return result;

	Return_Error(2);
}

bool Is_Valid_Number(char* int_string, bool signed_number, bool decimal_number)
{
	unsigned char i;

	char number_length = 0;

	for (i = 0; i < 255; i ++)
	{
		if (! int_string[i])
			break;

		if (int_string[i] == '.' && decimal_number)
		{
			if (! number_length) // .52 or something doesn't make sense to this function.
				return 0;
			continue;
		}
		if (int_string[i] == '-' && signed_number)
		{
			if (number_length)
				return 0;
			continue;
		}

		if (int_string[i] < '0' || int_string[i] > '9')
			return 0;

		number_length ++;
	}

	if (number_length > 0)
		return 1;

	return 0;
}

bool Is_Valid_Hex(char* hex_string)
{
	unsigned char i;

	char number_length = 0;

	for (i = 0; i < 255; i ++)
	{
		if (! hex_string[i])
			break;

		if ((hex_string[i] < '0' || hex_string[i] > '9') && 
			(hex_string[i] < 'a' || hex_string[i] > 'f') && 
			(hex_string[i] < 'A' || hex_string[i] > 'F'))
			return 0;

		number_length ++;
	}

	if (number_length > 0)
		return 1;

	return 0;
}

void Append_String(char* main_string, char* appending_string)
{
	int i;
	int append_i;

	for (i = 0; i >= 0; i ++)
	{
		if (! main_string[i])
			break;
	}

	for (append_i = 0; ; )
	{
		main_string[i ++] = appending_string[append_i];

		if (! appending_string[append_i])
			break;

		append_i ++;
	}

	// Too bad there isn't anything that can mark the end of this void function more nicely...
}

void Copy_String(char* source_string, char* destination_string)
{
	int i;

	for (i = 0; i >= 0; i ++)
	{
		destination_string[i] = source_string[i];

		if (! source_string[i])
			break;
	}
}

bool String_Compare(const char* first_string, const char* second_string)
{
	int i;

	for (i = 0; i >= 0; i ++)
	{
		if (first_string[i] != second_string[i])
			return 0;

		if (! first_string[i])
			return 1;
	}

	// Well, what do we do now?
	return 0;
}

bool String_Compare_Part(const char* first_string, const char* second_string, int length)
{
	int i;

	for (i = 0; i < length; i ++)
	{
		if (first_string[i] != second_string[i])
			return 0;
	}

	return 1;
}

void Uppercase_String(char* input, char* output)
{
	int i;

	for (i = 0; i >= 0; i ++)
	{
		char input_char = input[i];

		if (input_char >= 'a' && input_char <= 'z')
			input_char -= 0x20;

		output[i] = input_char;

		if (! input_char)
			return;
	}
}

void Lowercase_String(char* input, char* output)
{
	int i;

	for (i = 0; i >= 0; i ++)
	{
		char input_char = input[i];

		if (input_char >= 'A' && input_char <= 'Z')
			input_char += 0x20;

		output[i] = input_char;

		if (! input_char)
			return;
	}
}

int String_Length(char* string)
{
	int i;

	__asm
	{
		mov eax,dword ptr [string]
		mov bl,0

		LoopStart:
		mov cl,byte ptr [eax]
		add eax,1

		cmp cl,bl

		jne LoopStart

		sub eax,dword ptr [string]
		//sub eax,1
		mov dword ptr [i], eax
	};
	/*while (string[i ++]);*/

	return i;
}

bool File_Exists(char* filename)
{
	FILE* test_file = fopen(filename, "rb");

	if (test_file == NULL)
		return 0;

	fclose(test_file);

	return 1;
}

bool Get_Filename_from_Path(char* filepath, char* filename)
{
	int i;

	int last_slash_position = 0;

	for (i = 0; ; i ++)
	{
		if (! filepath[i])
			break;

		if (filepath[i] == '\\' || filepath[i] == '/')
			last_slash_position = i + 1;
	}

	for (i = last_slash_position; ; i ++)
	{
		filename[i - last_slash_position] = filepath[i];

		if (! filepath[i])
			break;
	}

	return 1;
}

bool Get_Path_from_Filename(char* filename, char* filepath)
{
	int i;

	int last_slash_position = 0;

	for (i = 0; ; i ++)
	{
		if (! filename[i])
			break;

		if (filename[i] == '\\' || filename[i] == '/')
			last_slash_position = i;
	}

	if (! last_slash_position)
		return 0;

	for (i = 0; i <= last_slash_position; i ++)
		filepath[i] = filename[i];

	filepath[++ last_slash_position] = 0;

	return 1;
}

void New_Hash(char* string, int string_length, char* resulting_bytes)
{
	int i;

	char result_position = 0;

	char add_char  = 127;
	char last_char = 0;

	// Start off at zero for every byte.
	// If we don't do this, the result will be random (in some sense).
	for (i = 0; i < 16; i ++)
		resulting_bytes[i] = 0;

	for (i = 0; i < string_length; i ++)
	{
		char current_char, switcher, even;

		// Set the current char to 255 minus the byte at this part of the string.
		current_char = 255 - string[i];

		// Set the "switcher", which is basically a randomisation variable.
		// The switcher is set to 1 if the current_char is a less-than-zero number.
		// Otherwise it's -1.
		switcher = current_char < 0 ? 1 : -1;

		// Depending on whether the current_char is less than zero, this is what'll happen.
		// (Remember, the switcher is basically a note of this main byte's signed-ness.
        if (switcher)
            current_char -= 9 * (current_char / 5);
        else
            current_char += 9 / (current_char * 5);

		// Even determines if current_char is.. well, even!!!!! LOL!!!!!!1
		even = ! (current_char & 1);

		// Now we're going to play around with the result position.
		// The switcher is really the only thing that changes it.
		result_position += switcher;

		if (result_position > 15)
			result_position = 0;
		if (result_position < 0)
			result_position = 15;

		// Do a bit more 'randomisation'.
		current_char += add_char * (-switcher);

		// Some even MORE 'randomisation', which'll affect the next byte greatly.
		add_char -= (current_char + last_char) * (even == 1 ? 1 : -1) - (add_char / 7);

		// Finish this byte off.
		last_char = current_char;

		resulting_bytes[result_position] += current_char;
	}

    for (i = 0; i < 16; i ++)
    {
		int l;

        for (l = i; l < 16; l ++)
            resulting_bytes[l] += resulting_bytes[i];
    }

    for (i = 0; i < 16; i ++)
    {
        if (resulting_bytes[i] == 0)
            resulting_bytes[i] = 1;
    }
}

void Hash(char* string, int string_size, char* result)
{
    char current_result_position = 0;
    char add_char = 127;
    char last_char = 0;

    for (int i = 0; i <= 15; i ++)
        result[i] = 0;

    for (int i = 0; i < string_size; i ++)
    {
        char current_char = 255 - string[i];
        char switcher;
        bool even;

        switcher = current_char < 0 ? 1 : -1;

        if (switcher)
            current_char -= 9 * (current_char / 5);
        else
            current_char += 9 / (current_char * 5);

        even = (float)(current_char / 2) == (float)current_char / 2;

        current_result_position += 1 * switcher;

        if (current_result_position > 15)
            current_result_position = 0;
        if (current_result_position < 0)
            current_result_position = 15;

        current_char += add_char * (-switcher);
        add_char -= (current_char + last_char) * (even == 1 ? 1 : -1) - (add_char / 7);

        last_char = current_char;

        result[current_result_position] += current_char;
    }

    for (char i = 0; i <= 15; i ++)
    {
        for (char l = i; l <= 15; l ++)
            result[l] += result[i];
    }

    for (char i = 0; i <= 15; i ++)
    {
        if (result[i] == 0)
            result[i] = 1;
    }
}

void Text_Hash(char* string, int string_length, char* resulting_string)
{
	int i;

	char byte_string[16];

	New_Hash(string, string_length, byte_string);

	// Basically set the resulting string's length to zero.
	resulting_string[0] = 0;

	for (i = 0; i < 16; i ++)
	{
		char byte[3];

		Hex_to_String(byte_string[i], byte, 2);

		if (i)
			Append_String(resulting_string, " ");

		Append_String(resulting_string, byte);
	}
}

void Show_Variables(int first_var, int second_var, int third_var, int fourth_var)
{
	char output_string[100];

	char number_string[12];

	Int_to_String(first_var, number_string);

	Copy_String(number_string, output_string);

	if (second_var != -65537)
	{
		char newline[3] = {0x0D, 0x0A, 0x00};

		Int_to_String(second_var, number_string);

		Append_String(output_string, newline);
		Append_String(output_string, number_string);

		if (third_var != -65537)
		{
			Int_to_String(third_var, number_string);

			Append_String(output_string, newline);
			Append_String(output_string, number_string);

			if (fourth_var != -65537)
			{
				Int_to_String(fourth_var, number_string);

				Append_String(output_string, newline);
				Append_String(output_string, number_string);
			}
		}
	}

	MessageBox(NULL, output_string, "Showing variables:", MB_OK);
}
