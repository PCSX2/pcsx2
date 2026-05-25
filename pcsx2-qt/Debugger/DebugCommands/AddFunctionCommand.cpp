
#include "Debugger/DebugCommands/AddFunctionCommand.h"
#include "Debugger/DebugCommands/DebugCommandStack.h"


AddFunctionCommand::AddFunctionCommand(
	std::string name,
	ccc::Address address,
	size_t size,
	ccc::FunctionHandle existingFunctionHandle,
	size_t originalExistingSize,
	size_t newExistingSize
	)
	: m_name{name}
	, m_address{address}
	, m_size{size}
	, m_existing_function{existingFunctionHandle}
	, m_originalExistingFunctionSize{originalExistingSize}
	, m_new_existing_function_size{newExistingSize}
{

}

bool AddFunctionCommand::Do(CommandContext& ctx)
{
	// if we already created the function before, we should recreate it with the same handle for history consistency
	if (m_movedFunction.has_value())
	{
		ccc::Address existingFunctionAddress;

		ctx.cpu.GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
			m_movedFunction.value().address();

			std::vector<ccc::Function> functionList;
			functionList.emplace_back(std::move(m_movedFunction.value()));
			database.functions.merge_from_vector(functionList);

			database.parameter_variables.merge_from_vector(m_movedParameters);
			database.local_variables.merge_from_vector(m_movedLocals);

			auto previousFunction = database.functions.symbol_from_handle(m_existing_function);
			if (previousFunction)
			{
				previousFunction->set_size(m_new_existing_function_size);
				existingFunctionAddress = previousFunction->address();
			}

			m_movedFunction = std::nullopt;
		});

		ctx.EnableHistory(m_address);

		if (existingFunctionAddress.valid())
		{
			ctx.Move(existingFunctionAddress, m_address);
		}

		return true;
	}


	ctx.cpu.GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		ccc::Result<ccc::SymbolSourceHandle> source = database.get_symbol_source("User-Defined");
		if (!source.success())
		{
			return;
		}

		ccc::Result<ccc::Function*> function = database.functions.create_symbol(m_name, m_address, *source, nullptr);
		if (!function.success())
		{
			return;
		}

		(*function)->set_size(m_size);

		ccc::Function* existing_function = database.functions.symbol_from_handle(m_existing_function);
		if (existing_function)
			existing_function->set_size(m_new_existing_function_size);

		m_generatedHandle = (*function)->handle();
	});

	return true;
}

bool AddFunctionCommand::Undo(CommandContext& ctx)
{
	ccc::Address existingFunctionAddress;

	ctx.cpu.GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		m_movedFunction = std::move(*database.functions.symbol_from_handle(m_generatedHandle));

		if (!m_movedFunction.has_value())
			return;

		auto previousFunc = database.functions.symbol_overlapping_address(m_movedFunction.value().address().value - 4);
		if (previousFunc)
		{
			existingFunctionAddress = previousFunc->address();
			previousFunc->set_size(m_originalExistingFunctionSize);
		}

		auto parameters = m_movedFunction.value().parameter_variables();
		if (parameters)
		{
			for (auto handle : parameters.value())
			{
				m_movedParameters.emplace_back(std::move(*database.parameter_variables.symbol_from_handle(handle)));
			}
		}

		auto locals = m_movedFunction.value().local_variables();
		if (locals)
		{
			for (auto handle : locals.value())
			{
				m_movedLocals.emplace_back(std::move(*database.local_variables.symbol_from_handle(handle)));
			}
		}

		database.functions.mark_symbol_for_destruction(m_generatedHandle, &database); // should mark parameters and local variables for destruction
		database.destroy_marked_symbols();
	});

	ctx.DisableHistory(m_address);

	if (existingFunctionAddress.valid ())
	{
		ctx.Move(m_address, existingFunctionAddress);
	}

	return true;
}

bool AddFunctionCommand::CanDo(CommandContext& ctx) const
{
	bool result = false;
	ctx.cpu.GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		ccc::Result<ccc::SymbolSourceHandle> source = database.get_symbol_source("User-Defined");
		if (!source.success())
		{
			//error = "Cannot create symbol source.";
			result = false;
		}

		auto previous = database.functions.symbol_overlapping_address(m_address.value - 4);

		if (!previous)
		{
			result = false;
			return;
		}

		if (previous->handle() == m_existing_function && previous->size() == m_originalExistingFunctionSize)
		{
			result = true;
			return;
		}
	});

	return result;
}

bool AddFunctionCommand::CanUndo(CommandContext& ctx) const
{
	if (!m_generatedHandle.valid ())
		return false;

	if (m_movedFunction.has_value())
		return false;

	bool result = false;
	ctx.cpu.GetSymbolGuardian().Read([&](ccc::SymbolDatabase const& database) {
		auto previous = database.functions.symbol_overlapping_address(m_address.value - 4);

		if (!previous)
		{
			result = false;
			return;
		}

		if (previous->handle() == m_existing_function && previous->size() == m_new_existing_function_size)
		{
			result = true;
			return;
		}
	});

	return result;
}

std::string AddFunctionCommand::GetName() const
{
	return "Add Function: " + m_name;
}
