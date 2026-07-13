#include "Debugger/DebugCommands/DebugCommandStack.h"

ccc::Address DebugCommandStack::GetAddress(ccc::Address address)
{
	//ccc::Address nextAddress = address;
	//
	//while (m_redirections.find(nextAddress) != m_redirections.end())
	//{
	//	nextAddress = m_redirections[nextAddress];
	//}
	//
	//return nextAddress;

	return address;
}

void DebugCommandStack::Do(DebugInterface& cpu, ccc::Address address)
{
	ccc::Address stackAddress = GetAddress(address);
	Stack& stack = m_commands[stackAddress];

	CommandContext ctx{stackAddress, cpu, *this};

	if (stack.DoCurrent(ctx))
	{
		stack.Up();
	}

	for (auto& t : ctx.m_toMove)
	{
		MoveCurrent(t.first, t.second);
	}
}

void DebugCommandStack::Undo(DebugInterface& cpu, ccc::Address address)
{
	ccc::Address stackAddress = GetAddress(address);
	Stack& stack = m_commands[stackAddress];

	CommandContext ctx{stackAddress, cpu, *this};

	if (stack.UndoCurrent(ctx))
	{
		stack.Down();
	}

	for (auto& t : ctx.m_toMove)
	{
		MoveCurrent(t.first, t.second);
	}
}

bool DebugCommandStack::CanDo(DebugInterface& cpu, ccc::Address address)
{
	ccc::Address stackAddress = GetAddress(address);
	Stack& stack = m_commands[stackAddress];

	if (!stack.InDoRange ())
		return false;

	const IDebugActionCommand* command = stack.commands.at(stack.index).get();

	if (!command)
		return false;

	CommandContext ctx{stackAddress, cpu, *this};
	if (!command->CanDo(ctx))
	{
		return false;
	}

	return true;
}

bool DebugCommandStack::CanUndo(DebugInterface& cpu, ccc::Address address)
{
	ccc::Address stackAddress = GetAddress(address);
	Stack& stack = m_commands[stackAddress];

	if (!stack.InUndoRange ())
		return false;

	const IDebugActionCommand* command = stack.commands.at(stack.index - 1).get();

	if (!command)
		return false;

	CommandContext ctx{stackAddress, cpu, *this};
	if (!command->CanUndo(ctx))
	{
		return false;
	}

	return true;
}

std::string DebugCommandStack::GetUndoName(ccc::Address address)
{
	Stack const& stack = m_commands[address];

	if (!stack.InUndoRange ())
		return "Undo (no command)";

	const IDebugActionCommand* command = stack.commands.at(stack.index - 1).get();

	if (!command)
		return "Undo (no command)";

	return "Undo (" + command->GetName() + ")";
}

std::string DebugCommandStack::GetRedoName(ccc::Address address)
{
	Stack const& stack = m_commands[address];

	if (!stack.NextInAccessRange ())
		return "Redo (no command)";

	const IDebugActionCommand* command = stack.commands.at(stack.index).get();

	if (!command)
		return "Redo (no command)";

	return "Redo (" + command->GetName() + ")";
}

bool DebugCommandStack::MoveCurrent(ccc::Address source, ccc::Address destination)
{
	if (source == destination)
		return false;

	Stack& srcStack = m_commands[source];
	Stack& dstStack = m_commands[destination];

	if (!srcStack.size())
		return false;

	size_t srcIdx = srcStack.index;
	size_t dstIdx = dstStack.index;

	if (srcIdx == -1 || !srcStack.size())
		return false;
	
	bool left = false;
	if (srcIdx == srcStack.size() && srcStack.commands[srcIdx - 1])
	{
		left = true;
		--srcIdx;
	}

	if (dstIdx == -1 || (dstIdx < dstStack.size() && dstStack.commands[dstIdx]) || dstIdx == dstStack.size())
	{
		if (dstIdx < dstStack.size())
			++dstIdx;

		else if (dstIdx == -1)
			dstIdx = 0;

		dstStack.commands.insert(dstStack.commands.begin() + dstIdx, std::move(srcStack.commands.at(srcIdx)));

	}
	else
	{
		dstStack.commands[dstIdx] = std::move(srcStack.commands.at(srcIdx));
	}

	dstStack.index = dstIdx + left;
	srcStack.index = srcIdx;

	return true;
}

void DebugCommandStack::DisableHistory(ccc::Address source)
{
	m_commands[source].enabled = false;
}

void DebugCommandStack::EnableHistory(ccc::Address source)
{
	m_commands[source].enabled = true;
}

std::optional<std::string> DebugCommandStack::GetErrorMessage(ccc::Address address)
{
	auto error = m_commands[address].error;
	m_commands[address].error = std::nullopt;
	return error;
}

void DebugCommandStack::SetError(ccc::Address address, std::string message)
{
	m_commands[address].error = message;
}

bool DebugCommandStack::Stack::DoCurrent(CommandContext& ctx)
{
	if (!enabled)
		return false;

	if (!InDoRange ())
		return false;

	IDebugActionCommand* command = commands.at(index).get();

	if (command->CanDo(ctx))
	{
		return command->Do(ctx);
	}

	return false;
}

bool DebugCommandStack::Stack::UndoCurrent(CommandContext& ctx)
{
	if (!enabled)
		return false;

	if (!InUndoRange ())
		return false;

	IDebugActionCommand* command = commands.at(index - 1).get();
		
	if (command == nullptr)
		return false;

	if (command->CanUndo(ctx))
	{
		return command->Undo(ctx);
	}

	return false;
}

void DebugCommandStack::Stack::ResetHistory() noexcept
{
	commands.clear();
	index = 0;
}

void DebugCommandStack::Stack::ClearBranch() noexcept
{
	if (index == -1)
	{
		if (commands.size() == 0)
			return;

		commands.clear ();
		return;
	}

	commands.erase(commands.begin() + index, commands.end());
}

bool DebugCommandStack::Stack::InDoRange() const
{
	if (!enabled)
		return false;

	return index + 1 >= 0 && index < commands.size() && commands.size () && index != -1;
}

bool DebugCommandStack::Stack::InUndoRange() const
{
	if (!enabled)
		return false;

	return index - 1 >= 0 && index <= commands.size() && commands.size() && index - 1 != -1;
}

bool DebugCommandStack::Stack::InAccessRange() const
{
	if (!enabled)
		return false;

	return index >= 0 && index < commands.size() && commands.size ();
}

bool DebugCommandStack::Stack::NextInAccessRange() const
{
	if (!enabled)
		return false;

	return index + 1 >= 0 && index < commands.size() && commands.size ();
}
