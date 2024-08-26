// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "NewSymbolDialogs.h"

#include <QtCore/QTimer>
#include <QtCore/QMetaMethod>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

#include "TypeString.h"

NewSymbolDialog::NewSymbolDialog(u32 flags, u32 alignment, DebugInterface& cpu, QWidget* parent)
	: QDialog(parent)
	, m_cpu(cpu)
	, m_alignment(alignment)
{
	m_ui.setupUi(this);

	connect(m_ui.buttonBox, &QDialogButtonBox::accepted, this, &NewSymbolDialog::createSymbol);
	connect(m_ui.storageTabBar, &QTabBar::currentChanged, this, &NewSymbolDialog::onStorageTabChanged);

	if (flags & GLOBAL_STORAGE)
	{
		int tab = m_ui.storageTabBar->addTab(tr("Global"));
		m_ui.storageTabBar->setTabData(tab, GLOBAL_STORAGE);
	}

	if (flags & REGISTER_STORAGE)
	{
		int tab = m_ui.storageTabBar->addTab(tr("Register"));
		m_ui.storageTabBar->setTabData(tab, REGISTER_STORAGE);

		setupRegisterField();
	}

	if (flags & STACK_STORAGE)
	{
		int tab = m_ui.storageTabBar->addTab(tr("Stack"));
		m_ui.storageTabBar->setTabData(tab, STACK_STORAGE);
	}

	if (m_ui.storageTabBar->count() == 1)
		m_ui.storageTabBar->hide();

	m_ui.form->setRowVisible(Row::SIZE, flags & SIZE_FIELD);
	m_ui.form->setRowVisible(Row::EXISTING_FUNCTIONS, flags & EXISTING_FUNCTIONS_FIELD);
	m_ui.form->setRowVisible(Row::TYPE, flags & TYPE_FIELD);
	m_ui.form->setRowVisible(Row::FUNCTION, flags & FUNCTION_FIELD);

	if (flags & SIZE_FIELD)
	{
		setupSizeField();
		updateSizeField();
	}

	if (flags & FUNCTION_FIELD)
		setupFunctionField();

	connectInputWidgets();
	onStorageTabChanged(0);
	adjustSize();
}

void NewSymbolDialog::setName(QString name)
{
	m_ui.nameLineEdit->setText(name);
}

void NewSymbolDialog::setAddress(u32 address)
{
	m_ui.addressLineEdit->setText(QString::number(address, 16));
}

void NewSymbolDialog::setCustomSize(u32 size)
{
	m_ui.customSizeRadioButton->setChecked(true);
	m_ui.customSizeSpinBox->setValue(size);
}

void NewSymbolDialog::setupRegisterField()
{
	m_ui.registerComboBox->clear();
	for (int i = 0; i < m_cpu.getRegisterCount(0); i++)
		m_ui.registerComboBox->addItem(m_cpu.getRegisterName(0, i));
}

void NewSymbolDialog::setupSizeField()
{
	connect(m_ui.customSizeRadioButton, &QRadioButton::toggled, m_ui.customSizeSpinBox, &QSpinBox::setEnabled);
	connect(m_ui.addressLineEdit, &QLineEdit::textChanged, this, &NewSymbolDialog::updateSizeField);
}

void NewSymbolDialog::setupFunctionField()
{
	m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) {
		const ccc::Function* default_function = database.functions.symbol_overlapping_address(m_cpu.getPC());

		for (const ccc::Function& function : database.functions)
		{
			QString name = QString::fromStdString(function.name());
			name.truncate(64);
			m_ui.functionComboBox->addItem(name);
			m_functions.emplace_back(function.handle());

			if (default_function && function.handle() == default_function->handle())
				m_ui.functionComboBox->setCurrentIndex(m_ui.functionComboBox->count() - 1);
		}
	});
}

void NewSymbolDialog::connectInputWidgets()
{
	QMetaMethod parse_user_input = metaObject()->method(metaObject()->indexOfSlot("parseUserInput()"));
	for (QObject* child : children())
	{
		QWidget* widget = qobject_cast<QWidget*>(child);
		if (!widget)
			continue;

		QMetaProperty property = widget->metaObject()->userProperty();
		if (!property.isValid() || !property.hasNotifySignal())
			continue;

		connect(widget, property.notifySignal(), this, parse_user_input);
	}
}

void NewSymbolDialog::updateErrorMessage(QString error_message)
{
	m_ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(error_message.isEmpty());
	m_ui.errorMessage->setText(error_message);
}

NewSymbolDialog::FunctionSizeType NewSymbolDialog::functionSizeType() const
{
	if (m_ui.fillExistingFunctionRadioButton->isChecked())
		return FILL_EXISTING_FUNCTION;

	if (m_ui.fillEmptySpaceRadioButton->isChecked())
		return FILL_EMPTY_SPACE;

	return CUSTOM_SIZE;
}

void NewSymbolDialog::updateSizeField()
{
	bool ok;
	u32 address = m_ui.addressLineEdit->text().toUInt(&ok, 16);
	if (ok)
	{
		m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) {
			std::optional<u32> fill_existing_function_size = fillExistingFunctionSize(address, database);
			if (fill_existing_function_size.has_value())
				m_ui.fillExistingFunctionRadioButton->setText(
					tr("Fill existing function (%1 bytes)").arg(*fill_existing_function_size));
			else
				m_ui.fillExistingFunctionRadioButton->setText(
					tr("Fill existing function (none found)"));
			m_ui.fillExistingFunctionRadioButton->setEnabled(fill_existing_function_size.has_value());

			std::optional<u32> fill_empty_space_size = fillEmptySpaceSize(address, database);
			if (fill_empty_space_size.has_value())
				m_ui.fillEmptySpaceRadioButton->setText(
					tr("Fill space (%1 bytes)").arg(*fill_empty_space_size));
			else
				m_ui.fillEmptySpaceRadioButton->setText(tr("Fill space (no next symbol)"));
			m_ui.fillEmptySpaceRadioButton->setEnabled(fill_empty_space_size.has_value());
		});
	}
	else
	{
		// Add some padding to the end of the radio button text so that the
		// layout engine knows we need some more space for the size.
		QString padding(16, ' ');
		m_ui.fillExistingFunctionRadioButton->setText(tr("Fill existing function").append(padding));
		m_ui.fillEmptySpaceRadioButton->setText(tr("Fill space").append(padding));
	}
}

std::optional<u32> NewSymbolDialog::fillExistingFunctionSize(u32 address, const ccc::SymbolDatabase& database)
{
	const ccc::Function* existing_function = database.functions.symbol_overlapping_address(address);
	if (!existing_function)
		return std::nullopt;

	return existing_function->address_range().high.value - address;
}

std::optional<u32> NewSymbolDialog::fillEmptySpaceSize(u32 address, const ccc::SymbolDatabase& database)
{
	const ccc::Symbol* next_symbol = database.symbol_after_address(
		address, ccc::FUNCTION | ccc::GLOBAL_VARIABLE | ccc::LOCAL_VARIABLE);
	if (!next_symbol)
		return std::nullopt;

	return next_symbol->address().value - address;
}

u32 NewSymbolDialog::storageType() const
{
	return m_ui.storageTabBar->tabData(m_ui.storageTabBar->currentIndex()).toUInt();
}

void NewSymbolDialog::onStorageTabChanged(int index)
{
	u32 storage = m_ui.storageTabBar->tabData(index).toUInt();

	m_ui.form->setRowVisible(Row::ADDRESS, storage == GLOBAL_STORAGE);
	m_ui.form->setRowVisible(Row::REGISTER, storage == REGISTER_STORAGE);
	m_ui.form->setRowVisible(Row::STACK_POINTER_OFFSET, storage == STACK_STORAGE);

	QTimer::singleShot(0, this, [&]() {
		parseUserInput();
	});
}

std::string NewSymbolDialog::parseName(QString& error_message)
{
	std::string name = m_ui.nameLineEdit->text().toStdString();
	if (name.empty())
		error_message = tr("Name is empty.");

	return name;
}

u32 NewSymbolDialog::parseAddress(QString& error_message)
{
	bool ok;
	u32 address = m_ui.addressLineEdit->text().toUInt(&ok, 16);
	if (!ok)
		error_message = tr("Address is not valid.");

	if (address % m_alignment != 0)
		error_message = tr("Address is not aligned.");

	return address;
}

// *****************************************************************************

NewFunctionDialog::NewFunctionDialog(DebugInterface& cpu, QWidget* parent)
	: NewSymbolDialog(GLOBAL_STORAGE | SIZE_FIELD | EXISTING_FUNCTIONS_FIELD, 4, cpu, parent)
{
	setWindowTitle("New Function");

	m_ui.customSizeSpinBox->setValue(8);
}

bool NewFunctionDialog::parseUserInput()
{
	QString error_message;
	m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) {
		m_name = parseName(error_message);
		if (!error_message.isEmpty())
			return;

		m_address = parseAddress(error_message);
		if (!error_message.isEmpty())
			return;

		m_size = 0;
		switch (functionSizeType())
		{
			case FILL_EXISTING_FUNCTION:
			{
				std::optional<u32> fill_existing_function_size = fillExistingFunctionSize(m_address, database);
				if (!fill_existing_function_size.has_value())
				{
					error_message = tr("No existing function found.");
					return;
				}

				m_size = *fill_existing_function_size;

				break;
			}
			case FILL_EMPTY_SPACE:
			{
				std::optional<u32> fill_space_size = fillEmptySpaceSize(m_address, database);
				if (!fill_space_size.has_value())
				{
					error_message = tr("No next symbol found.");
					return;
				}

				m_size = *fill_space_size;

				break;
			}
			case CUSTOM_SIZE:
			{
				m_size = m_ui.customSizeSpinBox->value();
				break;
			}
		}

		if (m_size == 0 || m_size > 256 * 1024 * 1024)
		{
			error_message = tr("Size is invalid.");
			return;
		}

		if (m_size % 4 != 0)
		{
			error_message = tr("Size is not a multiple of 4.");
			return;
		}

		// Handle an existing function if it exists.
		const ccc::Function* existing_function = database.functions.symbol_overlapping_address(m_address);
		m_existing_function = ccc::FunctionHandle();
		if (existing_function)
		{
			if (existing_function->address().value == m_address)
			{
				error_message = tr("A function already exists at that address.");
				return;
			}

			if (m_ui.shrinkExistingRadioButton->isChecked())
			{
				m_new_existing_function_size = m_address - existing_function->address().value;
				m_existing_function = existing_function->handle();
			}
		}
	});

	updateErrorMessage(error_message);
	return error_message.isEmpty();
}

void NewFunctionDialog::createSymbol()
{
	if (!parseUserInput())
		return;

	QString error_message;
	m_cpu.GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		ccc::Result<ccc::SymbolSourceHandle> source = database.get_symbol_source("User-defined");
		if (!source.success())
		{
			error_message = tr("Cannot create symbol source.");
			return;
		}

		ccc::Result<ccc::Function*> function = database.functions.create_symbol(std::move(m_name), m_address, *source, nullptr);
		if (!function.success())
		{
			error_message = tr("Cannot create symbol.");
			return;
		}

		(*function)->set_size(m_size);

		ccc::Function* existing_function = database.functions.symbol_from_handle(m_existing_function);
		if (existing_function)
			existing_function->set_size(m_new_existing_function_size);
	});

	if (!error_message.isEmpty())
		QMessageBox::warning(this, tr("Cannot Create Function"), error_message);
}

// *****************************************************************************

NewGlobalVariableDialog::NewGlobalVariableDialog(DebugInterface& cpu, QWidget* parent)
	: NewSymbolDialog(GLOBAL_STORAGE | TYPE_FIELD, 1, cpu, parent)
{
	setWindowTitle("New Global Variable");
}

bool NewGlobalVariableDialog::parseUserInput()
{
	QString error_message;
	m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) {
		m_name = parseName(error_message);
		if (!error_message.isEmpty())
			return;

		m_address = parseAddress(error_message);
		if (!error_message.isEmpty())
			return;

		m_type = stringToType(m_ui.typeLineEdit->text().toStdString(), database, error_message);
		if (!error_message.isEmpty())
			return;
	});

	updateErrorMessage(error_message);
	return error_message.isEmpty();
}

void NewGlobalVariableDialog::createSymbol()
{
	if (!parseUserInput())
		return;

	QString error_message;
	m_cpu.GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		ccc::Result<ccc::SymbolSourceHandle> source = database.get_symbol_source("User-defined");
		if (!source.success())
		{
			error_message = tr("Cannot create symbol source.");
			return;
		}

		ccc::Result<ccc::GlobalVariable*> global_variable = database.global_variables.create_symbol(std::move(m_name), m_address, *source, nullptr);
		if (!global_variable.success())
		{
			error_message = tr("Cannot create symbol.");
			return;
		}

		(*global_variable)->set_type(std::move(m_type));
	});

	if (!error_message.isEmpty())
		QMessageBox::warning(this, tr("Cannot Create Global Variable"), error_message);
}

// *****************************************************************************

NewLocalVariableDialog::NewLocalVariableDialog(DebugInterface& cpu, QWidget* parent)
	: NewSymbolDialog(GLOBAL_STORAGE | REGISTER_STORAGE | STACK_STORAGE | TYPE_FIELD | FUNCTION_FIELD, 1, cpu, parent)
{
	setWindowTitle("New Local Variable");
}

bool NewLocalVariableDialog::parseUserInput()
{
	QString error_message;
	m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) {
		m_name = parseName(error_message);
		if (!error_message.isEmpty())
			return;

		int function_index = m_ui.functionComboBox->currentIndex();
		if (function_index > 0 && function_index < (int)m_functions.size())
			m_function = m_functions[m_ui.functionComboBox->currentIndex()];
		else
			m_function = ccc::FunctionHandle();

		const ccc::Function* function = database.functions.symbol_from_handle(m_function);
		if (!function)
		{
			error_message = tr("Invalid function.");
			return;
		}

		switch (storageType())
		{
			case GLOBAL_STORAGE:
			{
				m_storage.emplace<ccc::GlobalStorage>();

				m_address = parseAddress(error_message);
				if (!error_message.isEmpty())
					return;

				break;
			}
			case REGISTER_STORAGE:
			{
				ccc::RegisterStorage& register_storage = m_storage.emplace<ccc::RegisterStorage>();
				register_storage.dbx_register_number = m_ui.registerComboBox->currentIndex();
				break;
			}
			case STACK_STORAGE:
			{
				ccc::StackStorage& stack_storage = m_storage.emplace<ccc::StackStorage>();
				stack_storage.stack_pointer_offset = m_ui.stackPointerOffsetSpinBox->value();

				// Convert to caller sp relative.
				if (std::optional<u32> stack_frame_size = m_cpu.getStackFrameSize(*function))
					stack_storage.stack_pointer_offset -= *stack_frame_size;
				else
				{
					error_message = tr("Cannot determine stack frame size of selected function.");
					return;
				}

				break;
			}
		}

		std::string type_string = m_ui.typeLineEdit->text().toStdString();
		m_type = stringToType(type_string, database, error_message);
		if (!error_message.isEmpty())
			return;
	});

	updateErrorMessage(error_message);
	return error_message.isEmpty();
}

void NewLocalVariableDialog::createSymbol()
{
	if (!parseUserInput())
		return;

	QString error_message;
	m_cpu.GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		ccc::Function* function = database.functions.symbol_from_handle(m_function);
		if (!function)
		{
			error_message = tr("Invalid function.");
			return;
		}

		ccc::Result<ccc::SymbolSourceHandle> source = database.get_symbol_source("User-defined");
		if (!source.success())
		{
			error_message = tr("Cannot create symbol source.");
			return;
		}

		ccc::Result<ccc::LocalVariable*> local_variable =
			database.local_variables.create_symbol(std::move(m_name), m_address, *source, nullptr);
		if (!local_variable.success())
		{
			error_message = tr("Cannot create symbol.");
			return;
		}

		(*local_variable)->set_type(std::move(m_type));
		(*local_variable)->storage = m_storage;

		std::vector<ccc::LocalVariableHandle> local_variables;
		if (function->local_variables().has_value())
			local_variables = *function->local_variables();
		local_variables.emplace_back((*local_variable)->handle());
		function->set_local_variables(local_variables, database);
	});

	if (!error_message.isEmpty())
		QMessageBox::warning(this, tr("Cannot Create Local Variable"), error_message);
}

// *****************************************************************************

NewParameterVariableDialog::NewParameterVariableDialog(DebugInterface& cpu, QWidget* parent)
	: NewSymbolDialog(REGISTER_STORAGE | STACK_STORAGE | TYPE_FIELD | FUNCTION_FIELD, 1, cpu, parent)
{
	setWindowTitle("New Parameter Variable");
}

bool NewParameterVariableDialog::parseUserInput()
{
	QString error_message;
	m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) {
		m_name = parseName(error_message);
		if (!error_message.isEmpty())
			return;

		int function_index = m_ui.functionComboBox->currentIndex();
		if (function_index > 0 && function_index < (int)m_functions.size())
			m_function = m_functions[m_ui.functionComboBox->currentIndex()];
		else
			m_function = ccc::FunctionHandle();

		const ccc::Function* function = database.functions.symbol_from_handle(m_function);
		if (!function)
		{
			error_message = tr("Invalid function.");
			return;
		}

		std::variant<ccc::RegisterStorage, ccc::StackStorage> storage;
		switch (storageType())
		{
			case GLOBAL_STORAGE:
			{
				error_message = tr("Invalid storage type.");
				return;
			}
			case REGISTER_STORAGE:
			{
				ccc::RegisterStorage& register_storage = storage.emplace<ccc::RegisterStorage>();
				register_storage.dbx_register_number = m_ui.registerComboBox->currentIndex();
				break;
			}
			case STACK_STORAGE:
			{
				ccc::StackStorage& stack_storage = storage.emplace<ccc::StackStorage>();
				stack_storage.stack_pointer_offset = m_ui.stackPointerOffsetSpinBox->value();

				// Convert to caller sp relative.
				if (std::optional<u32> stack_frame_size = m_cpu.getStackFrameSize(*function))
					stack_storage.stack_pointer_offset -= *stack_frame_size;
				else
				{
					error_message = tr("Cannot determine stack frame size of selected function.");
					return;
				}

				break;
			}
		}

		std::string type_string = m_ui.typeLineEdit->text().toStdString();
		m_type = stringToType(type_string, database, error_message);
		if (!error_message.isEmpty())
			return;
	});

	updateErrorMessage(error_message);
	return error_message.isEmpty();
}

void NewParameterVariableDialog::createSymbol()
{
	if (!parseUserInput())
		return;

	QString error_message;
	m_cpu.GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		ccc::Function* function = database.functions.symbol_from_handle(m_function);
		if (!function)
		{
			error_message = tr("Invalid function.");
			return;
		}

		ccc::Result<ccc::SymbolSourceHandle> source = database.get_symbol_source("User-defined");
		if (!source.success())
		{
			error_message = tr("Cannot create symbol source.");
			return;
		}

		ccc::Result<ccc::ParameterVariable*> parameter_variable =
			database.parameter_variables.create_symbol(std::move(m_name), *source, nullptr);
		if (!parameter_variable.success())
		{
			error_message = tr("Cannot create symbol.");
			return;
		}

		(*parameter_variable)->set_type(std::move(m_type));
		(*parameter_variable)->storage = m_storage;

		std::vector<ccc::ParameterVariableHandle> parameter_variables;
		if (function->parameter_variables().has_value())
			parameter_variables = *function->parameter_variables();
		parameter_variables.emplace_back((*parameter_variable)->handle());
		function->set_parameter_variables(parameter_variables, database);
	});

	if (!error_message.isEmpty())
		QMessageBox::warning(this, tr("Cannot Create Parameter Variable"), error_message);
}
