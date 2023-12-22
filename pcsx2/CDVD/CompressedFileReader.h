// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include <string>

// Factory - creates an AsyncFileReader derived instance which can read a compressed file
class CompressedFileReader
{
public:
	// fileName and its contents may be used to choose the compressed reader.
	// If no matching handler is found, NULL is returned.
	// The returned instance still needs ->Open(filename) before usage.
	// Open(filename) may still fail.
	static AsyncFileReader* GetNewReader(const std::string& fileName);

private:
	virtual ~CompressedFileReader() = 0;
};
