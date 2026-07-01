#pragma once

#include "IDebugCommand.h"

class RenameFunctionCommand : public IDebugActionCommand
{
public:
	RenameFunctionCommand(std::string old_name, std::string new_name, ccc::FunctionHandle handle)
		: m_oldName{old_name}
		, m_newName{new_name}
		, m_handle{handle}
	{
	}

	bool Do(CommandContext& ctx) override;
	bool Undo(CommandContext& ctx) override;

	bool CanDo(CommandContext& ctx) const override;
	bool CanUndo(CommandContext& ctx) const override;

	std::string GetName() const
	{
		return "Rename Function: " + m_oldName + " -> " + m_newName;
	}


protected:
	std::string m_oldName;
	std::string m_newName;

	ccc::FunctionHandle m_handle;
};
