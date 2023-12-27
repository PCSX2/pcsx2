// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

kernel void waste_time(constant uint& cycles [[buffer(0)]], device uint* spin [[buffer(1)]])
{
	uint value = spin[0];
	// The compiler doesn't know, but spin[0] == 0, so this loop won't actually go anywhere
	for (uint i = 0; i < cycles; i++)
		value = spin[value];
	// Store the result back to the buffer so the compiler can't optimize it away
	spin[0] = value;
}
