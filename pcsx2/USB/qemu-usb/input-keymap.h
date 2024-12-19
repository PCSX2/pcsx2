// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "hid.h"

int qemu_input_qcode_to_number(const QKeyCode value);
int qemu_input_key_value_to_number(const KeyValue* value);
int qemu_input_key_value_to_scancode(const KeyValue* value, bool down, int* codes);
