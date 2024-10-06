// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SymbolTreeModel.h"

#include <QtWidgets/QApplication>
#include <QtGui/QBrush>
#include <QtGui/QPalette>

#include "common/Assertions.h"

#include "TypeString.h"

SymbolTreeModel::SymbolTreeModel(DebugInterface& cpu, QObject* parent)
	: QAbstractItemModel(parent)
	, m_cpu(cpu)
{
}

QModelIndex SymbolTreeModel::index(int row, int column, const QModelIndex& parent) const
{
	SymbolTreeNode* parent_node = nodeFromIndex(parent);
	if (!parent_node)
		return QModelIndex();

	if (row < 0 || row >= (int)parent_node->children().size())
		return QModelIndex();

	const SymbolTreeNode* child_node = parent_node->children()[row].get();
	if (!child_node)
		return QModelIndex();

	return createIndex(row, column, child_node);
}

QModelIndex SymbolTreeModel::parent(const QModelIndex& index) const
{
	if (!index.isValid())
		return QModelIndex();

	SymbolTreeNode* child_node = nodeFromIndex(index);
	if (!child_node)
		return QModelIndex();

	const SymbolTreeNode* parent_node = child_node->parent();
	if (!parent_node || parent_node == m_root.get())
		return QModelIndex();

	return indexFromNode(*parent_node);
}

int SymbolTreeModel::rowCount(const QModelIndex& parent) const
{
	if (parent.column() > 0)
		return 0;

	SymbolTreeNode* node = nodeFromIndex(parent);
	if (!node)
		return 0;

	return (int)node->children().size();
}

int SymbolTreeModel::columnCount(const QModelIndex& parent) const
{
	return COLUMN_COUNT;
}

bool SymbolTreeModel::hasChildren(const QModelIndex& parent) const
{
	if (!parent.isValid())
		return true;

	SymbolTreeNode* parent_node = nodeFromIndex(parent);
	if (!parent_node)
		return true;

	// If a node doesn't have a type, it can't generate any children, so all the
	// children that will exist must already be there.
	if (!parent_node->type.valid())
		return !parent_node->children().empty();

	bool result = true;
	m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) -> void {
		const ccc::ast::Node* type = parent_node->type.lookup_node(database);
		if (!type)
			return;

		result = nodeHasChildren(*type, database);
	});

	return result;
}

QVariant SymbolTreeModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
		return QVariant();

	SymbolTreeNode* node = nodeFromIndex(index);
	if (!node)
		return QVariant();

	if (role == Qt::ForegroundRole)
	{
		bool active = true;

		// Gray out the names of symbols that have been overwritten in memory.
		if (index.column() == NAME && node->symbol.valid())
			active = symbolMatchesMemory(node->symbol);

		// Gray out the values of variables that are dead.
		if (index.column() == VALUE && node->liveness().has_value())
			active = *node->liveness();

		QPalette::ColorGroup group = active ? QPalette::Active : QPalette::Disabled;
		return QBrush(QApplication::palette().color(group, QPalette::Text));
	}

	if (role != Qt::DisplayRole)
		return QVariant();

	switch (index.column())
	{
		case NAME:
		{
			return node->name;
		}
		case VALUE:
		{
			if (node->tag != SymbolTreeNode::OBJECT)
				return QVariant();

			return node->display_value();
		}
		case LOCATION:
		{
			return node->location.toString(m_cpu).rightJustified(8);
		}
		case SIZE:
		{
			if (!node->size.has_value())
				return QVariant();

			return QString::number(*node->size);
		}
		case TYPE:
		{
			QVariant result;
			m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) -> void {
				const ccc::ast::Node* type = node->type.lookup_node(database);
				if (!type)
					return;

				result = typeToString(type, database);
			});

			return result;
		}
		case LIVENESS:
		{
			if (!node->liveness().has_value())
				return QVariant();

			return *node->liveness() ? tr("Alive") : tr("Dead");
		}
	}

	return QVariant();
}

bool SymbolTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	if (!index.isValid())
		return false;

	SymbolTreeNode* node = nodeFromIndex(index);
	if (!node)
		return false;

	bool data_changed = false;
	m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) -> void {
		switch (role)
		{
			case EDIT_ROLE:
				data_changed = node->writeToVM(value, m_cpu, database);
				break;
			case UPDATE_FROM_MEMORY_ROLE:
				data_changed = node->readFromVM(m_cpu, database);
				break;
		}
	});

	if (data_changed)
		emit dataChanged(index.siblingAtColumn(0), index.siblingAtColumn(COLUMN_COUNT - 1));

	return data_changed;
}

void SymbolTreeModel::fetchMore(const QModelIndex& parent)
{
	if (!parent.isValid())
		return;

	SymbolTreeNode* parent_node = nodeFromIndex(parent);
	if (!parent_node || !parent_node->type.valid())
		return;

	if (!parent_node->children().empty())
		return;

	std::vector<std::unique_ptr<SymbolTreeNode>> children;
	m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) -> void {
		const ccc::ast::Node* logical_parent_type = parent_node->type.lookup_node(database);
		if (!logical_parent_type)
			return;

		children = populateChildren(
			parent_node->name, parent_node->location, *logical_parent_type, parent_node->type, m_cpu, database);
	});

	bool insert_children = !children.empty();
	if (insert_children)
		beginInsertRows(parent, 0, children.size() - 1);
	parent_node->setChildren(std::move(children));
	if (insert_children)
		endInsertRows();
}

bool SymbolTreeModel::canFetchMore(const QModelIndex& parent) const
{
	if (!parent.isValid())
		return false;

	SymbolTreeNode* parent_node = nodeFromIndex(parent);
	if (!parent_node || !parent_node->type.valid())
		return false;

	bool result = false;
	m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) -> void {
		const ccc::ast::Node* parent_type = parent_node->type.lookup_node(database);
		if (!parent_type)
			return;

		result = nodeHasChildren(*parent_type, database) && !parent_node->childrenFetched();
	});

	return result;
}

Qt::ItemFlags SymbolTreeModel::flags(const QModelIndex& index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;

	Qt::ItemFlags flags = QAbstractItemModel::flags(index);

	if (index.column() == LOCATION || index.column() == TYPE || index.column() == VALUE)
		flags |= Qt::ItemIsEditable;

	return flags;
}

QVariant SymbolTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
		return QVariant();

	switch (section)
	{
		case NAME:
			return tr("Name");
		case VALUE:
			return tr("Value");
		case LOCATION:
			return tr("Location");
		case SIZE:
			return tr("Size");
		case TYPE:
			return tr("Type");
		case LIVENESS:
			return tr("Liveness");
	}

	return QVariant();
}

QModelIndex SymbolTreeModel::indexFromNode(const SymbolTreeNode& node) const
{
	int row = 0;
	if (node.parent())
	{
		for (int i = 0; i < (int)node.parent()->children().size(); i++)
			if (node.parent()->children()[i].get() == &node)
				row = i;
	}
	else
		row = 0;

	return createIndex(row, 0, &node);
}

SymbolTreeNode* SymbolTreeModel::nodeFromIndex(const QModelIndex& index) const
{
	if (!index.isValid())
		return m_root.get();

	SymbolTreeNode* node = static_cast<SymbolTreeNode*>(index.internalPointer());
	if (!node)
		return m_root.get();

	return node;
}

void SymbolTreeModel::reset(std::unique_ptr<SymbolTreeNode> new_root)
{
	beginResetModel();
	m_root = std::move(new_root);
	endResetModel();
}

void SymbolTreeModel::resetChildren(QModelIndex index)
{
	pxAssertRel(index.isValid(), "Invalid model index.");

	SymbolTreeNode* node = nodeFromIndex(index);
	if (!node || node->tag != SymbolTreeNode::OBJECT)
		return;

	resetChildrenRecursive(*node);
}

void SymbolTreeModel::resetChildrenRecursive(SymbolTreeNode& node)
{
	for (const std::unique_ptr<SymbolTreeNode>& child : node.children())
		resetChildrenRecursive(*child);

	bool remove_rows = !node.children().empty();
	if (remove_rows)
		beginRemoveRows(indexFromNode(node), 0, node.children().size() - 1);
	node.clearChildren();
	if (remove_rows)
		endRemoveRows();
}

bool SymbolTreeModel::needsReset() const
{
	if (!m_root)
		return true;

	bool needs_reset = false;
	m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) {
		needs_reset = !m_root->anySymbolsValid(database);
	});

	return needs_reset;
}

std::optional<QString> SymbolTreeModel::changeTypeTemporarily(QModelIndex index, std::string_view type_string)
{
	SymbolTreeNode* node = nodeFromIndex(index);
	if (!node)
		return std::nullopt;

	resetChildren(index);

	QString error_message;
	m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) -> void {
		std::unique_ptr<ccc::ast::Node> type = stringToType(type_string, database, error_message);
		if (!error_message.isEmpty())
			return;

		node->temporary_type = std::move(type);
		node->type = ccc::NodeHandle(node->temporary_type.get());
	});

	setData(index, QVariant(), UPDATE_FROM_MEMORY_ROLE);

	return error_message;
}

std::optional<QString> SymbolTreeModel::typeFromModelIndexToString(QModelIndex index)
{
	SymbolTreeNode* node = nodeFromIndex(index);
	if (!node || node->tag != SymbolTreeNode::OBJECT)
		return std::nullopt;

	QString result;
	m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) -> void {
		const ccc::ast::Node* type = node->type.lookup_node(database);
		if (!type)
			return;

		result = typeToString(type, database);
	});

	return result;
}

std::vector<std::unique_ptr<SymbolTreeNode>> SymbolTreeModel::populateChildren(
	const QString& name,
	SymbolTreeLocation location,
	const ccc::ast::Node& logical_type,
	ccc::NodeHandle parent_handle,
	DebugInterface& cpu,
	const ccc::SymbolDatabase& database)
{
	auto [physical_type, symbol] = logical_type.physical_type(database);

	// If we went through a type name, we need to make the node handles for the
	// children point to the new symbol instead of the original one.
	if (symbol)
		parent_handle = ccc::NodeHandle(*symbol, nullptr);

	std::vector<std::unique_ptr<SymbolTreeNode>> children;

	switch (physical_type->descriptor)
	{
		case ccc::ast::ARRAY:
		{
			const ccc::ast::Array& array = physical_type->as<ccc::ast::Array>();

			for (s32 i = 0; i < array.element_count; i++)
			{
				SymbolTreeLocation element_location = location.addOffset(i * array.element_type->size_bytes);
				if (element_location.type == SymbolTreeLocation::NONE)
					continue;

				std::unique_ptr<SymbolTreeNode> element = std::make_unique<SymbolTreeNode>();
				element->name = QString("[%1]").arg(i);
				element->type = parent_handle.handle_for_child(array.element_type.get());
				element->location = element_location;
				children.emplace_back(std::move(element));
			}

			break;
		}
		case ccc::ast::POINTER_OR_REFERENCE:
		{
			const ccc::ast::PointerOrReference& pointer_or_reference = physical_type->as<ccc::ast::PointerOrReference>();

			u32 address = location.read32(cpu);
			if (!cpu.isValidAddress(address))
				break;

			std::unique_ptr<SymbolTreeNode> pointee = std::make_unique<SymbolTreeNode>();
			pointee->name = QString("*%1").arg(name);
			pointee->type = parent_handle.handle_for_child(pointer_or_reference.value_type.get());
			pointee->location = SymbolTreeLocation(SymbolTreeLocation::MEMORY, address);
			children.emplace_back(std::move(pointee));

			break;
		}
		case ccc::ast::STRUCT_OR_UNION:
		{
			const ccc::ast::StructOrUnion& struct_or_union = physical_type->as<ccc::ast::StructOrUnion>();

			std::vector<ccc::ast::StructOrUnion::FlatField> fields;
			struct_or_union.flatten_fields(fields, nullptr, database, true);

			for (const ccc::ast::StructOrUnion::FlatField& field : fields)
			{
				if (symbol)
					parent_handle = ccc::NodeHandle(*symbol, nullptr);

				SymbolTreeLocation field_location = location.addOffset(field.base_offset + field.node->offset_bytes);
				if (field_location.type == SymbolTreeLocation::NONE)
					continue;

				std::unique_ptr<SymbolTreeNode> child_node = std::make_unique<SymbolTreeNode>();
				if (!field.node->name.empty())
					child_node->name = QString::fromStdString(field.node->name);
				else
					child_node->name = QString("(anonymous %1)").arg(ccc::ast::node_type_to_string(*field.node));
				child_node->type = parent_handle.handle_for_child(field.node);
				child_node->location = field_location;
				children.emplace_back(std::move(child_node));
			}

			break;
		}
		default:
		{
		}
	}

	for (std::unique_ptr<SymbolTreeNode>& child : children)
		child->readFromVM(cpu, database);

	return children;
}

bool SymbolTreeModel::nodeHasChildren(const ccc::ast::Node& logical_type, const ccc::SymbolDatabase& database)
{
	const ccc::ast::Node& type = *logical_type.physical_type(database).first;

	bool result = false;
	switch (type.descriptor)
	{
		case ccc::ast::ARRAY:
		{
			const ccc::ast::Array& array = type.as<ccc::ast::Array>();
			result = array.element_count > 0;
			break;
		}
		case ccc::ast::POINTER_OR_REFERENCE:
		{
			result = true;
			break;
		}
		case ccc::ast::STRUCT_OR_UNION:
		{
			const ccc::ast::StructOrUnion& struct_or_union = type.as<ccc::ast::StructOrUnion>();
			result = !struct_or_union.base_classes.empty() || !struct_or_union.fields.empty();
			break;
		}
		default:
		{
		}
	}

	return result;
}

bool SymbolTreeModel::symbolMatchesMemory(ccc::MultiSymbolHandle& symbol) const
{
	bool matching = true;
	switch (symbol.descriptor())
	{
		case ccc::SymbolDescriptor::FUNCTION:
		{
			m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) -> void {
				const ccc::Function* function = database.functions.symbol_from_handle(symbol.handle());
				if (!function || function->original_hash() == 0)
					return;

				matching = function->current_hash() == function->original_hash();
			});
			break;
		}
		case ccc::SymbolDescriptor::GLOBAL_VARIABLE:
		{
			m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) -> void {
				const ccc::GlobalVariable* global_variable = database.global_variables.symbol_from_handle(symbol.handle());
				if (!global_variable)
					return;

				const ccc::SourceFile* source_file = database.source_files.symbol_from_handle(global_variable->source_file());
				if (!source_file)
					return;

				matching = source_file->functions_match();
			});
			break;
		}
		case ccc::SymbolDescriptor::LOCAL_VARIABLE:
		{
			m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) -> void {
				const ccc::LocalVariable* local_variable = database.local_variables.symbol_from_handle(symbol.handle());
				if (!local_variable)
					return;

				const ccc::Function* function = database.functions.symbol_from_handle(local_variable->function());
				if (!function)
					return;

				const ccc::SourceFile* source_file = database.source_files.symbol_from_handle(function->source_file());
				if (!source_file)
					return;

				matching = source_file->functions_match();
			});
			break;
		}
		case ccc::SymbolDescriptor::PARAMETER_VARIABLE:
		{
			m_cpu.GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) -> void {
				const ccc::ParameterVariable* parameter_variable = database.parameter_variables.symbol_from_handle(symbol.handle());
				if (!parameter_variable)
					return;

				const ccc::Function* function = database.functions.symbol_from_handle(parameter_variable->function());
				if (!function)
					return;

				const ccc::SourceFile* source_file = database.source_files.symbol_from_handle(function->source_file());
				if (!source_file)
					return;

				matching = source_file->functions_match();
			});
			break;
		}
		default:
		{
		}
	}

	return matching;
}
