#pragma once

#include "DebugTools/DebugInterface.h"

class DebugCommandStack;

class CommandContext
{
public:
	void Move(ccc::Address source, ccc::Address destination);
	void EnableHistory (ccc::Address source);
	void DisableHistory(ccc::Address source);

	ccc::Address stackAddress;
	DebugInterface& cpu;


	CommandContext(ccc::Address address, DebugInterface& cpu, DebugCommandStack& stack);

private:
	DebugCommandStack& m_stack;

	std::vector<std::pair<ccc::Address, ccc::Address>> m_toMove;

	friend class DebugCommandStack;
};
