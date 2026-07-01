#pragma once

#include <ccc/symbol_database.h>

#include "DebugTools/DebugInterface.h"
#include "Debugger/DebugCommands/CommandContext.h"

#include <string>
#include <vector>
#include <memory>
#include <utility>

class DebugCommandStack;

// commands with side-effects should be able to completly revert them
struct IDebugActionCommand
{
	virtual bool Do(CommandContext& ctx) = 0;
	virtual bool Undo(CommandContext& ctx) = 0;

	virtual bool CanDo(CommandContext& ctx) const = 0;
	virtual bool CanUndo(CommandContext& ctx) const = 0;

	virtual std::string GetName() const = 0;
};
