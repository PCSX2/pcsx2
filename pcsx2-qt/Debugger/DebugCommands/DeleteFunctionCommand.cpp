
#include "DeleteFunctionCommand.h"

#include "Debugger/DebuggerWindow.h"

#include "DebugTools/DebugInterface.h"
#include "DebugTools/DisassemblyManager.h"
#include "DebugTools/Breakpoints.h"
#include "DebugTools/MipsAssembler.h"

#include "SymbolTree/NewSymbolDialogs.h"

#include "Debugger/DebugCommands/DebugCommandStack.h"


bool DeleteFunctionCommand::Do(CommandContext& ctx)
{
	if (m_function.has_value())
		return false;

	ccc::Address source;
	ccc::Address destination;

	ctx.cpu.GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		m_function = std::move(*database.functions.symbol_from_handle(m_handle));

		auto previousFunc = database.functions.symbol_overlapping_address(m_function.value().address().value - 4);
		if (previousFunc)
		{
			m_previousFunctionOriginalSize = previousFunc->size();
			m_previousFunctionHandle = previousFunc->handle();

			previousFunc->set_size(m_function.value().size() + previousFunc->size());
		}

		auto parameters = m_function.value().parameter_variables();
		if (parameters)
		{
			for (auto handle : parameters.value())
			{
				m_parameters.emplace_back(std::move(*database.parameter_variables.symbol_from_handle(handle)));
			}
		}

		auto locals = m_function.value().local_variables();
		if (locals)
		{
			for (auto handle : locals.value())
			{
				m_locals.emplace_back(std::move(*database.local_variables.symbol_from_handle(handle)));
			}
		}

		source = m_function.value().address();
		destination = previousFunc->address();

		database.functions.mark_symbol_for_destruction(m_handle, &database); // should mark parameters and local variables for destruction
		database.destroy_marked_symbols();

		m_deletedFunctionName = m_function->name();
	});

	ctx.DisableHistory(source);
	ctx.Move(source, destination);

	return true;
}

bool DeleteFunctionCommand::Undo(CommandContext& ctx)
{
	ccc::Address source;
	ccc::Address destination;

	if (!m_function.has_value())
		return false;

	ctx.cpu.GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		destination = m_function.value().address();

		std::vector<ccc::Function> functionList;
		functionList.emplace_back(std::move(m_function.value()));
		database.functions.merge_from_vector(functionList);

		database.parameter_variables.merge_from_vector(m_parameters);
		database.local_variables.merge_from_vector(m_locals);

		auto previousFunction = database.functions.symbol_from_handle(m_previousFunctionHandle);
		previousFunction->set_size(m_previousFunctionOriginalSize);

		m_function = std::nullopt;

		source = previousFunction->address();
	});

	ctx.EnableHistory(destination);
	ctx.Move(source, destination);

	return true;
}

bool DeleteFunctionCommand::CanDo(CommandContext& ctx) const
{
	bool result = true;
	ctx.cpu.GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		auto current = database.functions.symbol_from_handle(m_handle);

		if (!current)
		{
			result = false;
			return;
		}

		auto previous = database.functions.symbol_overlapping_address(current->address().value - 4);

		if (!previous)
		{
			result = false;
			return;
		}

		if (previous->handle() == m_previousFunctionHandle && previous->size() == m_previousFunctionOriginalSize)
		{
			result = true;
			return;
		}
	});
	return result;
}

bool DeleteFunctionCommand::CanUndo(CommandContext& ctx) const
{
	bool result = true;
	ctx.cpu.GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		auto current = database.functions.symbol_from_handle(m_handle);

		if (current)
		{
			result = false;
			return;
		}
	});
	return result;
}
