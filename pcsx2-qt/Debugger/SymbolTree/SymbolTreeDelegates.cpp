// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "SymbolTreeDelegates.h"

#include "Debugger/SymbolTree/SymbolTreeModel.h"
#include "Debugger/SymbolTree/TypeString.h"

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>

SymbolTreeValueDelegate::SymbolTreeValueDelegate(
	DebugInterface& cpu,
	QObject* parent)
	: QStyledItemDelegate(parent)
	, m_cpu(cpu)
{
}

QWidget* SymbolTreeValueDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	if (!index.isValid())
		return nullptr;

	const SymbolTreeModel* tree_model = qobject_cast<const SymbolTreeModel*>(index.model());
	if (!tree_model)
		return nullptr;

	const SymbolTreeDisplayOptions& display_options = tree_model->displayOptions();

	SymbolTreeNode* node = tree_model->nodeFromIndex(index);
	if (!node || !node->type.valid())
		return nullptr;

	QWidget* result = nullptr;
	m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) {
		const ccc::ast::Node* logical_type = node->type.lookup_node(database);
		if (!logical_type)
			return;

		const ccc::ast::Node& physical_type = *logical_type->physical_type(database).first;
		QVariant value = node->readValueAsVariant(physical_type, m_cpu, database);

		const ccc::ast::Node& type = *logical_type->physical_type(database).first;
		switch (type.descriptor)
		{
			case ccc::ast::BUILTIN:
			{
				const ccc::ast::BuiltIn& builtin = type.as<ccc::ast::BuiltIn>();

				switch (builtin.bclass)
				{
					case ccc::ast::BuiltInClass::UNSIGNED_8:
					case ccc::ast::BuiltInClass::UNQUALIFIED_8:
					case ccc::ast::BuiltInClass::UNSIGNED_16:
					case ccc::ast::BuiltInClass::UNSIGNED_32:
					case ccc::ast::BuiltInClass::UNSIGNED_64:
					{
						SymbolTreeIntegerLineEdit* editor = new SymbolTreeIntegerLineEdit(
							display_options, ccc::ast::builtin_class_size(builtin.bclass) * 8, parent);
						editor->setUnsignedValue(value.toULongLong());
						result = editor;

						break;
					}
					case ccc::ast::BuiltInClass::SIGNED_8:
					case ccc::ast::BuiltInClass::SIGNED_16:
					case ccc::ast::BuiltInClass::SIGNED_32:
					case ccc::ast::BuiltInClass::SIGNED_64:
					{
						SymbolTreeIntegerLineEdit* editor = new SymbolTreeIntegerLineEdit(
							display_options, ccc::ast::builtin_class_size(builtin.bclass) * 8, parent);
						editor->setSignedValue(value.toLongLong());
						result = editor;

						break;
					}
					case ccc::ast::BuiltInClass::BOOL_8:
					{
						QCheckBox* editor = new QCheckBox(parent);
						editor->setChecked(value.toBool());
						connect(editor, &QCheckBox::checkStateChanged, this, &SymbolTreeValueDelegate::onCheckBoxStateChanged);
						result = editor;

						break;
					}
					case ccc::ast::BuiltInClass::FLOAT_32:
					{
						QLineEdit* editor = new QLineEdit(parent);
						editor->setText(QString::number(value.toFloat()));
						result = editor;

						break;
					}
					case ccc::ast::BuiltInClass::FLOAT_64:
					{
						QLineEdit* editor = new QLineEdit(parent);
						editor->setText(QString::number(value.toDouble()));
						result = editor;

						break;
					}
					default:
					{
					}
				}
				break;
			}
			case ccc::ast::ENUM:
			{
				const ccc::ast::Enum& enumeration = type.as<ccc::ast::Enum>();

				QComboBox* combo_box = new QComboBox(parent);
				bool named = false;

				for (size_t i = 0; i < enumeration.constants.size(); i++)
				{
					QString text = QString::fromStdString(enumeration.constants[i].second);
					combo_box->addItem(text, enumeration.constants[i].first);
					if (enumeration.constants[i].first == value.toInt())
					{
						combo_box->setCurrentIndex(static_cast<int>(i));
						named = true;
					}
				}

				if (!named)
				{
					// The value isn't equal to any of the named constants, so
					// add an extra item to the combo box representing the
					// current value so that the first named constant isn't
					// written back to VM memory accidentally.
					QString text = display_options.signedIntegerToString(value.toInt(), 32);
					combo_box->insertItem(0, text, value.toInt());
					combo_box->setCurrentIndex(0);
				}

				connect(combo_box, &QComboBox::currentIndexChanged, this, &SymbolTreeValueDelegate::onComboBoxIndexChanged);
				result = combo_box;

				break;
			}
			case ccc::ast::POINTER_OR_REFERENCE:
			case ccc::ast::POINTER_TO_DATA_MEMBER:
			{
				QLineEdit* editor = new QLineEdit(parent);
				editor->setText(QString::number(value.toULongLong(), 16));
				result = editor;

				break;
			}
			default:
			{
			}
		}
	});

	return result;
}

void SymbolTreeValueDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
	// This function is intentionally left blank to prevent the values of
	// editors from constantly being reset every time the model is updated.
}

void SymbolTreeValueDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
	if (!index.isValid())
		return;

	const SymbolTreeModel* tree_model = qobject_cast<const SymbolTreeModel*>(index.model());
	if (!model)
		return;

	SymbolTreeNode* node = tree_model->nodeFromIndex(index);
	if (!node || !node->type.valid())
		return;

	QVariant value;
	m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) {
		const ccc::ast::Node* logical_type = node->type.lookup_node(database);
		if (!logical_type)
			return;

		const ccc::ast::Node& type = *logical_type->physical_type(database).first;
		switch (type.descriptor)
		{
			case ccc::ast::BUILTIN:
			{
				const ccc::ast::BuiltIn& builtin = type.as<ccc::ast::BuiltIn>();

				switch (builtin.bclass)
				{
					case ccc::ast::BuiltInClass::UNSIGNED_8:
					case ccc::ast::BuiltInClass::UNQUALIFIED_8:
					case ccc::ast::BuiltInClass::UNSIGNED_16:
					case ccc::ast::BuiltInClass::UNSIGNED_32:
					case ccc::ast::BuiltInClass::UNSIGNED_64:
					{
						auto line_edit = qobject_cast<SymbolTreeIntegerLineEdit*>(editor);
						Q_ASSERT(line_edit);

						std::optional<u64> i = line_edit->unsignedValue();
						if (i.has_value())
							value = static_cast<quint64>(*i);

						break;
					}
					case ccc::ast::BuiltInClass::SIGNED_8:
					case ccc::ast::BuiltInClass::SIGNED_16:
					case ccc::ast::BuiltInClass::SIGNED_32:
					case ccc::ast::BuiltInClass::SIGNED_64:
					{
						auto line_edit = qobject_cast<SymbolTreeIntegerLineEdit*>(editor);
						Q_ASSERT(line_edit);

						std::optional<s64> i = line_edit->signedValue();
						if (i.has_value())
							value = static_cast<qint64>(*i);

						break;
					}
					case ccc::ast::BuiltInClass::BOOL_8:
					{
						QCheckBox* check_box = qobject_cast<QCheckBox*>(editor);
						value = check_box->isChecked();

						break;
					}
					case ccc::ast::BuiltInClass::FLOAT_32:
					{
						QLineEdit* line_edit = qobject_cast<QLineEdit*>(editor);
						Q_ASSERT(line_edit);

						bool ok;
						float f = line_edit->text().toFloat(&ok);
						if (ok)
							value = f;

						break;
					}
					case ccc::ast::BuiltInClass::FLOAT_64:
					{
						QLineEdit* line_edit = qobject_cast<QLineEdit*>(editor);
						Q_ASSERT(line_edit);

						bool ok;
						double d = line_edit->text().toDouble(&ok);
						if (ok)
							value = d;

						break;
					}
					default:
					{
					}
				}
				break;
			}
			case ccc::ast::ENUM:
			{
				QComboBox* combo_box = qobject_cast<QComboBox*>(editor);
				Q_ASSERT(combo_box);

				value = combo_box->currentData().toInt();

				break;
			}
			case ccc::ast::POINTER_OR_REFERENCE:
			case ccc::ast::POINTER_TO_DATA_MEMBER:
			{
				QLineEdit* line_edit = qobject_cast<QLineEdit*>(editor);
				Q_ASSERT(line_edit);

				bool ok;
				qulonglong address = line_edit->text().toUInt(&ok, 16);
				if (ok)
					value = address;

				break;
			}
			default:
			{
			}
		}
	});

	if (value.isValid())
		model->setData(index, value, SymbolTreeModel::EDIT_ROLE);
}

void SymbolTreeValueDelegate::onCheckBoxStateChanged(Qt::CheckState state)
{
	QCheckBox* check_box = qobject_cast<QCheckBox*>(sender());
	if (check_box)
		commitData(check_box);
}

void SymbolTreeValueDelegate::onComboBoxIndexChanged(int index)
{
	QComboBox* combo_box = qobject_cast<QComboBox*>(sender());
	if (combo_box)
		commitData(combo_box);
}

// *****************************************************************************

SymbolTreeLocationDelegate::SymbolTreeLocationDelegate(
	DebugInterface& cpu,
	u32 alignment,
	QObject* parent)
	: QStyledItemDelegate(parent)
	, m_cpu(cpu)
	, m_alignment(alignment)
{
}

QWidget* SymbolTreeLocationDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	if (!index.isValid())
		return nullptr;

	const SymbolTreeModel* model = qobject_cast<const SymbolTreeModel*>(index.model());
	if (!model)
		return nullptr;

	SymbolTreeNode* node = model->nodeFromIndex(index);
	if (!node || !node->symbol.valid() || !node->symbol.is_flag_set(ccc::WITH_ADDRESS_MAP))
		return nullptr;

	if (!node->is_location_editable)
		return nullptr;

	return new QLineEdit(parent);
}

void SymbolTreeLocationDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
	if (!index.isValid())
		return;

	const SymbolTreeModel* model = qobject_cast<const SymbolTreeModel*>(index.model());
	if (!model)
		return;

	SymbolTreeNode* node = model->nodeFromIndex(index);
	if (!node || !node->symbol.valid())
		return;

	QLineEdit* line_edit = qobject_cast<QLineEdit*>(editor);
	Q_ASSERT(line_edit);

	m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) {
		const ccc::Symbol* symbol = node->symbol.lookup_symbol(database);
		if (!symbol || !symbol->address().valid())
			return;

		line_edit->setText(QString::number(symbol->address().value, 16));
	});
}

void SymbolTreeLocationDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
	if (!index.isValid())
		return;

	const SymbolTreeModel* tree_model = qobject_cast<const SymbolTreeModel*>(index.model());
	if (!tree_model)
		return;

	SymbolTreeNode* node = tree_model->nodeFromIndex(index);
	if (!node || !node->symbol.valid() || !node->symbol.is_flag_set(ccc::WITH_ADDRESS_MAP))
		return;

	QLineEdit* line_edit = qobject_cast<QLineEdit*>(editor);
	Q_ASSERT(line_edit);

	SymbolTreeModel* symbol_tree_model = qobject_cast<SymbolTreeModel*>(model);
	Q_ASSERT(symbol_tree_model);

	bool ok;
	u32 address = line_edit->text().toUInt(&ok, 16);
	if (!ok)
		return;

	address -= address % m_alignment;

	bool success = false;
	m_cpu.GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		success = node->symbol.move_symbol(address, database);
	});

	if (success)
	{
		node->location = SymbolTreeLocation(SymbolTreeLocation::MEMORY, address);
		symbol_tree_model->setData(index, QVariant(), SymbolTreeModel::UPDATE_FROM_MEMORY_ROLE);
		symbol_tree_model->resetChildren(index);
	}
}

// *****************************************************************************

SymbolTreeTypeDelegate::SymbolTreeTypeDelegate(
	DebugInterface& cpu,
	QObject* parent)
	: QStyledItemDelegate(parent)
	, m_cpu(cpu)
{
}

QWidget* SymbolTreeTypeDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	if (!index.isValid())
		return nullptr;

	const SymbolTreeModel* tree_model = qobject_cast<const SymbolTreeModel*>(index.model());
	if (!tree_model)
		return nullptr;

	SymbolTreeNode* node = tree_model->nodeFromIndex(index);
	if (!node || !node->symbol.valid())
		return nullptr;

	return new QLineEdit(parent);
}

void SymbolTreeTypeDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
	if (!index.isValid())
		return;

	const SymbolTreeModel* tree_model = qobject_cast<const SymbolTreeModel*>(index.model());
	if (!tree_model)
		return;

	SymbolTreeNode* node = tree_model->nodeFromIndex(index);
	if (!node || !node->symbol.valid())
		return;

	QLineEdit* line_edit = qobject_cast<QLineEdit*>(editor);
	Q_ASSERT(line_edit);

	m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) {
		const ccc::Symbol* symbol = node->symbol.lookup_symbol(database);
		if (!symbol || !symbol->type())
			return;

		line_edit->setText(typeToString(symbol->type(), database));
	});
}

void SymbolTreeTypeDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
	if (!index.isValid())
		return;

	const SymbolTreeModel* tree_model = qobject_cast<const SymbolTreeModel*>(index.model());
	if (!tree_model)
		return;

	SymbolTreeNode* node = tree_model->nodeFromIndex(index);
	if (!node || !node->symbol.valid())
		return;

	QLineEdit* line_edit = qobject_cast<QLineEdit*>(editor);
	Q_ASSERT(line_edit);

	SymbolTreeModel* symbol_tree_model = qobject_cast<SymbolTreeModel*>(model);
	Q_ASSERT(symbol_tree_model);

	QString error_message;
	m_cpu.GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		ccc::Symbol* symbol = node->symbol.lookup_symbol(database);
		if (!symbol)
		{
			error_message = tr("Symbol no longer exists.");
			return;
		}

		std::unique_ptr<ccc::ast::Node> type = stringToType(line_edit->text().toStdString(), database, error_message);
		if (!error_message.isEmpty())
			return;

		symbol->set_type(std::move(type));
		node->type = ccc::NodeHandle(node->symbol.descriptor(), *symbol, symbol->type());
	});

	if (error_message.isEmpty())
	{
		symbol_tree_model->setData(index, QVariant(), SymbolTreeModel::UPDATE_FROM_MEMORY_ROLE);
		symbol_tree_model->resetChildren(index);
	}
	else
		QMessageBox::warning(editor, tr("Cannot Change Type"), error_message);
}

// *****************************************************************************

SymbolTreeIntegerLineEdit::SymbolTreeIntegerLineEdit(
	SymbolTreeDisplayOptions display_options, s32 size_bits, QWidget* parent)
	: QLineEdit(parent)
	, m_display_options(display_options)
	, m_size_bits(size_bits)
{
}

std::optional<u64> SymbolTreeIntegerLineEdit::unsignedValue()
{
	return m_display_options.stringToUnsignedInteger(text());
}

void SymbolTreeIntegerLineEdit::setUnsignedValue(u64 value)
{
	setText(m_display_options.unsignedIntegerToString(value, m_size_bits));
}

std::optional<s64> SymbolTreeIntegerLineEdit::signedValue()
{
	return m_display_options.stringToSignedInteger(text());
}

void SymbolTreeIntegerLineEdit::setSignedValue(s64 value)
{
	setText(m_display_options.signedIntegerToString(value, m_size_bits));
}
