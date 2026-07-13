#pragma once

#include "Debugger/DebugCommands/IDebugCommand.h"

class DeleteFunctionCommand : public IDebugActionCommand
{
public:
	DeleteFunctionCommand(ccc::FunctionHandle handle)
		: m_handle{handle}
	{
	}

	bool Do(CommandContext& ctx) override;
	bool Undo(CommandContext& ctx) override;

	bool CanDo(CommandContext& ctx) const override;
	bool CanUndo(CommandContext& ctx) const override;

	std::string GetName() const
	{
		return "Delete Function: " + m_deletedFunctionName;
	}

protected:
	ccc::FunctionHandle m_handle;
	std::string m_deletedFunctionName = "no selected function";


	std::optional<ccc::Function> m_function = std::nullopt;
	std::vector<ccc::ParameterVariable> m_parameters;
	std::vector<ccc::LocalVariable> m_locals;

	ccc::FunctionHandle m_previousFunctionHandle;
	u32 m_previousFunctionOriginalSize = 0;
};
