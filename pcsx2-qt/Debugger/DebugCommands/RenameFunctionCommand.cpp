#include "RenameFunctionCommand.h"

bool RenameFunctionCommand::Do(CommandContext& ctx)
{
	ctx.cpu.GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		database.functions.rename_symbol(m_handle, m_newName);
	});

	return true;
}

bool RenameFunctionCommand::Undo(CommandContext& ctx)
{
	ctx.cpu.GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		database.functions.rename_symbol(m_handle, m_oldName);
	});

	return true;
}

bool RenameFunctionCommand::CanDo(CommandContext& ctx) const
{
	bool result;
	ctx.cpu.GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		ccc::Function* func = database.functions.symbol_from_handle(m_handle);

		if (func)
		{
			if (func->name() == m_oldName)
			{
				result = true;
				return;
			}
		}
	});
	return result;
}

bool RenameFunctionCommand::CanUndo(CommandContext& ctx) const
{
	bool result = false;
	ctx.cpu.GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		ccc::Function* func = database.functions.symbol_from_handle(m_handle);

		if (func)
		{
			if (func->name() == m_newName)
			{
				result = true;
				return;
			}
		}
	});
	return result;
}