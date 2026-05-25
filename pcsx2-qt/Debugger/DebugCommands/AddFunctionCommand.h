#pragma once

#include <Debugger/DebugCommands/IDebugCommand.h>

class AddFunctionCommand: public IDebugActionCommand
{
public:
	AddFunctionCommand(
		std::string name,
		ccc::Address address,
		size_t size,
		ccc::FunctionHandle existingFunctionHandle,
		size_t originalExistingSize,
		size_t newExistingSize
	);

	bool Do(CommandContext& ctx) override;
	bool Undo(CommandContext& ctx) override;
	bool CanDo(CommandContext& ctx) const override;
	bool CanUndo(CommandContext& ctx) const override;
	std::string GetName() const override;

private:
	std::string m_name;
	ccc::Address m_address;
	size_t m_size = 0;

	ccc::FunctionHandle m_existing_function;
	size_t m_new_existing_function_size = 0;
	size_t m_originalExistingFunctionSize = 0;

	ccc::FunctionHandle m_generatedHandle;

	std::optional<ccc::Function> m_movedFunction;
	std::vector<ccc::ParameterVariable> m_movedParameters;
	std::vector<ccc::LocalVariable> m_movedLocals;


	bool m_inOtherStack = false;
};
