#include <map>
#include "hid.h"

extern const std::map<const QKeyCode, unsigned short> qemu_input_map_qcode_to_qnum;
int qemu_input_qcode_to_number(const QKeyCode value);
int qemu_input_key_value_to_number(const KeyValue *value);
int qemu_input_key_value_to_scancode(const KeyValue *value, bool down, int *codes);