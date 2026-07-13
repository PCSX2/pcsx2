
#include "CommandContext.h"

#include "Debugger/DebugCommands/DebugCommandStack.h"

CommandContext::CommandContext(ccc::Address address, DebugInterface& cpu, DebugCommandStack& stack)
	: stackAddress{address}
	, cpu{cpu}
	, m_stack{stack}
{

}

void CommandContext::Move(ccc::Address source, ccc::Address destination)
{
	m_toMove.emplace_back(std::pair {source, destination});
}

void CommandContext::EnableHistory(ccc::Address source)
{
	m_stack.EnableHistory(source);
}

void CommandContext::DisableHistory(ccc::Address source)
{
	m_stack.DisableHistory(source);
}
