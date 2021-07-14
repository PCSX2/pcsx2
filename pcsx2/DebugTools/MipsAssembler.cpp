/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2014-2020  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"
#include "Core/Assembler.h"
#include "Core/FileManager.h"
#include "MipsAssembler.h"
#include "System.h"
#include "SymbolMap.h"
#include "Util/Util.h"

class Ps2AssemblerFile : public AssemblerFile
{
public:
	explicit Ps2AssemblerFile(DebugInterface* cpu)
		: cpu(cpu)
		, address(0)
		, fileName("Memory")
	{
	}

	bool open(bool onlyCheck) override { return true; }
	void close() override {}
	bool isOpen() override { return true; }
	bool write(void* data, size_t length) override
	{
		if (!cpu->isValidAddress((u32)(address + length - 1)))
			return false;

		u8* ptr = (u8*)data;
		for (size_t i = 0; i < length; i++)
		{
			cpu->write8(address + i, ptr[i]);
		}

		address += length;
		return true;
	}
	int64_t getVirtualAddress() override { return address; }
	int64_t getPhysicalAddress() override { return getVirtualAddress(); }
	int64_t getHeaderSize() override { return 0; }
	bool seekVirtual(int64_t virtualAddress) override
	{
		if (!cpu->isValidAddress(virtualAddress))
			return false;
		address = virtualAddress;
		return true;
	}
	bool seekPhysical(int64_t physicalAddress) override { return seekVirtual(physicalAddress); }
	const fs::path& getFileName() override { return fileName; }

private:
	DebugInterface* cpu;
	int64_t address;
	fs::path fileName;
};

bool MipsAssembleOpcode(const char* line, DebugInterface* cpu, u32 address, std::string& errorText)
{
	SysClearExecutionCache();

	std::vector<std::wstring> errors;

	wchar_t str[64];
	swprintf(str, 64, L".ps2\n.org 0x%08X\n", address);

	ArmipsArguments args;
	args.mode = ArmipsMode::MEMORY;
	args.content = str + convertUtf8ToWString(line);
	args.silent = true;
	args.memoryFile = std::make_shared<Ps2AssemblerFile>(cpu);
	args.errorsResult = &errors;

	symbolMap.GetLabels(args.labels);

	errorText = "";
	if (!runArmips(args))
	{
		for (size_t i = 0; i < errors.size(); i++)
		{
			errorText += convertWStringToUtf8(errors[i]);
			if (i != errors.size() - 1)
				errorText += "\n";
		}

		return false;
	}

	return true;
}
