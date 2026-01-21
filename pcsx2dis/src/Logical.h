char Get_Needed_Bytes (int number);
bool Write_Bytes      (char* array_2, int array_part, int number, char bytes);
int  Read_Bytes       (char* array, int array_part, char bytes);

void Hex_to_String (unsigned int, char*, int length = -1);
void Int_to_String (unsigned int, char*);

unsigned int String_to_Hex (char*, char* error = 0);
unsigned int String_to_Int (char*, char* error = 0);

void Uppercase_String (char*, char*);
void Lowercase_String (char*, char*);

void Append_String (char*, char*);
void Copy_String   (char*, char*);

bool Get_Path_from_Filename (char* filename, char* filepath);
bool Get_Filename_from_Path (char* filepath, char* filename);

bool Is_Valid_Number (char* int_string, bool sign = 0, bool dec = 0);
bool Is_Valid_Hex    (char* hex_string);

int String_Length(char* string);

bool String_Compare      (const char* first_string, const char* second_string);
bool String_Compare_Part (const char* first_string, const char* second_string, int length);

void Hash      (char* string, int string_length, char* resulting_bytes);
void New_Hash  (char* string, int string_length, char* resulting_bytes);
void Text_Hash (char* string, int string_length, char* resulting_bytes);

void Show_Variables(int one, int two = -65537, int three = -65537, int four = -65537);
