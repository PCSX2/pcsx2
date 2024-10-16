// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SymbolTreeNode.h"

#include <ccc/ast.h>

const QVariant& SymbolTreeNode::value() const
{
	return m_value;
}

const QString& SymbolTreeNode::display_value() const
{
	return m_display_value;
}

std::optional<bool> SymbolTreeNode::liveness()
{
	return m_liveness;
}

bool SymbolTreeNode::readFromVM(DebugInterface& cpu, const ccc::SymbolDatabase& database)
{
	QVariant new_value;

	const ccc::ast::Node* logical_type = type.lookup_node(database);
	if (logical_type)
	{
		const ccc::ast::Node& physical_type = *logical_type->physical_type(database).first;
		new_value = readValueAsVariant(physical_type, cpu, database);
	}

	bool data_changed = false;

	if (new_value != m_value)
	{
		m_value = std::move(new_value);
		data_changed = true;
	}

	data_changed |= updateDisplayString(cpu, database);
	data_changed |= updateLiveness(cpu);
	data_changed |= updateMatchesMemory(cpu, database);

	return data_changed;
}

bool SymbolTreeNode::writeToVM(QVariant value, DebugInterface& cpu, const ccc::SymbolDatabase& database)
{
	bool data_changed = false;

	if (value != m_value)
	{
		m_value = std::move(value);
		data_changed = true;
	}

	const ccc::ast::Node* logical_type = type.lookup_node(database);
	if (logical_type)
	{
		const ccc::ast::Node& physical_type = *logical_type->physical_type(database).first;
		writeValueFromVariant(m_value, physical_type, cpu);
	}

	data_changed |= updateDisplayString(cpu, database);
	data_changed |= updateLiveness(cpu);

	return data_changed;
}

QVariant SymbolTreeNode::readValueAsVariant(const ccc::ast::Node& physical_type, DebugInterface& cpu, const ccc::SymbolDatabase& database) const
{
	switch (physical_type.descriptor)
	{
		case ccc::ast::BUILTIN:
		{
			const ccc::ast::BuiltIn& builtIn = physical_type.as<ccc::ast::BuiltIn>();
			switch (builtIn.bclass)
			{
				case ccc::ast::BuiltInClass::UNSIGNED_8:
					return (qulonglong)location.read8(cpu);
				case ccc::ast::BuiltInClass::SIGNED_8:
					return (qlonglong)(s8)location.read8(cpu);
				case ccc::ast::BuiltInClass::UNQUALIFIED_8:
					return (qulonglong)location.read8(cpu);
				case ccc::ast::BuiltInClass::BOOL_8:
					return (bool)location.read8(cpu);
				case ccc::ast::BuiltInClass::UNSIGNED_16:
					return (qulonglong)location.read16(cpu);
				case ccc::ast::BuiltInClass::SIGNED_16:
					return (qlonglong)(s16)location.read16(cpu);
				case ccc::ast::BuiltInClass::UNSIGNED_32:
					return (qulonglong)location.read32(cpu);
				case ccc::ast::BuiltInClass::SIGNED_32:
					return (qlonglong)(s32)location.read32(cpu);
				case ccc::ast::BuiltInClass::FLOAT_32:
				{
					u32 value = location.read32(cpu);
					return *reinterpret_cast<float*>(&value);
				}
				case ccc::ast::BuiltInClass::UNSIGNED_64:
					return (qulonglong)location.read64(cpu);
				case ccc::ast::BuiltInClass::SIGNED_64:
					return (qlonglong)(s64)location.read64(cpu);
				case ccc::ast::BuiltInClass::FLOAT_64:
				{
					u64 value = location.read64(cpu);
					return *reinterpret_cast<double*>(&value);
				}
				default:
				{
				}
			}
			break;
		}
		case ccc::ast::ENUM:
			return location.read32(cpu);
		case ccc::ast::POINTER_OR_REFERENCE:
		case ccc::ast::POINTER_TO_DATA_MEMBER:
			return location.read32(cpu);
		default:
		{
		}
	}

	return QVariant();
}

bool SymbolTreeNode::writeValueFromVariant(QVariant value, const ccc::ast::Node& physical_type, DebugInterface& cpu) const
{
	switch (physical_type.descriptor)
	{
		case ccc::ast::BUILTIN:
		{
			const ccc::ast::BuiltIn& built_in = physical_type.as<ccc::ast::BuiltIn>();

			switch (built_in.bclass)
			{
				case ccc::ast::BuiltInClass::UNSIGNED_8:
					location.write8((u8)value.toULongLong(), cpu);
					break;
				case ccc::ast::BuiltInClass::SIGNED_8:
					location.write8((u8)(s8)value.toLongLong(), cpu);
					break;
				case ccc::ast::BuiltInClass::UNQUALIFIED_8:
					location.write8((u8)value.toULongLong(), cpu);
					break;
				case ccc::ast::BuiltInClass::BOOL_8:
					location.write8((u8)value.toBool(), cpu);
					break;
				case ccc::ast::BuiltInClass::UNSIGNED_16:
					location.write16((u16)value.toULongLong(), cpu);
					break;
				case ccc::ast::BuiltInClass::SIGNED_16:
					location.write16((u16)(s16)value.toLongLong(), cpu);
					break;
				case ccc::ast::BuiltInClass::UNSIGNED_32:
					location.write32((u32)value.toULongLong(), cpu);
					break;
				case ccc::ast::BuiltInClass::SIGNED_32:
					location.write32((u32)(s32)value.toLongLong(), cpu);
					break;
				case ccc::ast::BuiltInClass::FLOAT_32:
				{
					float f = value.toFloat();
					location.write32(*reinterpret_cast<u32*>(&f), cpu);
					break;
				}
				case ccc::ast::BuiltInClass::UNSIGNED_64:
					location.write64((u64)value.toULongLong(), cpu);
					break;
				case ccc::ast::BuiltInClass::SIGNED_64:
					location.write64((u64)(s64)value.toLongLong(), cpu);
					break;
				case ccc::ast::BuiltInClass::FLOAT_64:
				{
					double d = value.toDouble();
					location.write64(*reinterpret_cast<u64*>(&d), cpu);
					break;
				}
				default:
				{
					return false;
				}
			}
			break;
		}
		case ccc::ast::ENUM:
			location.write32((u32)value.toULongLong(), cpu);
			break;
		case ccc::ast::POINTER_OR_REFERENCE:
		case ccc::ast::POINTER_TO_DATA_MEMBER:
			location.write32((u32)value.toULongLong(), cpu);
			break;
		default:
		{
			return false;
		}
	}

	return true;
}

bool SymbolTreeNode::updateDisplayString(DebugInterface& cpu, const ccc::SymbolDatabase& database)
{
	QString result;

	const ccc::ast::Node* logical_type = type.lookup_node(database);
	if (logical_type)
	{
		const ccc::ast::Node& physical_type = *logical_type->physical_type(database).first;
		result = generateDisplayString(physical_type, cpu, database, 0);
	}

	if (result.isEmpty())
	{
		// We don't know how to display objects of this type, so just show the
		// first 4 bytes of it as a hex dump.
		u32 value = location.read32(cpu);
		result = QString("%1 %2 %3 %4")
					 .arg(value & 0xff, 2, 16, QChar('0'))
					 .arg((value >> 8) & 0xff, 2, 16, QChar('0'))
					 .arg((value >> 16) & 0xff, 2, 16, QChar('0'))
					 .arg((value >> 24) & 0xff, 2, 16, QChar('0'));
	}

	if (result == m_display_value)
		return false;

	m_display_value = std::move(result);

	return true;
}

QString SymbolTreeNode::generateDisplayString(
	const ccc::ast::Node& physical_type, DebugInterface& cpu, const ccc::SymbolDatabase& database, s32 depth) const
{
	s32 max_elements_to_display = 0;
	switch (depth)
	{
		case 0:
			max_elements_to_display = 8;
			break;
		case 1:
			max_elements_to_display = 2;
			break;
	}

	switch (physical_type.descriptor)
	{
		case ccc::ast::ARRAY:
		{
			const ccc::ast::Array& array = physical_type.as<ccc::ast::Array>();
			const ccc::ast::Node& element_type = *array.element_type->physical_type(database).first;

			if (element_type.name == "char" && location.type == SymbolTreeLocation::MEMORY)
			{
				char* string = cpu.stringFromPointer(location.address);
				if (string)
					return QString("\"%1\"").arg(string);
			}

			QString result;
			result += "{";

			s32 elements_to_display = std::min(array.element_count, max_elements_to_display);
			for (s32 i = 0; i < elements_to_display; i++)
			{
				SymbolTreeNode node;
				node.location = location.addOffset(i * array.element_type->size_bytes);

				QString element = node.generateDisplayString(element_type, cpu, database, depth + 1);
				if (element.isEmpty())
					element = QString("(%1)").arg(ccc::ast::node_type_to_string(element_type));
				result += element;

				if (i + 1 != array.element_count)
					result += ",";
			}

			if (elements_to_display != array.element_count)
				result += "...";

			result += "}";
			return result;
		}
		case ccc::ast::BUILTIN:
		{
			const ccc::ast::BuiltIn& builtIn = physical_type.as<ccc::ast::BuiltIn>();

			QString result;
			switch (builtIn.bclass)
			{
				case ccc::ast::BuiltInClass::UNSIGNED_8:
					result = QString::number(location.read8(cpu));
					break;
				case ccc::ast::BuiltInClass::SIGNED_8:
					result = QString::number((s8)location.read8(cpu));
					break;
				case ccc::ast::BuiltInClass::UNQUALIFIED_8:
					result = QString::number(location.read8(cpu));
					break;
				case ccc::ast::BuiltInClass::BOOL_8:
					result = location.read8(cpu) ? "true" : "false";
					break;
				case ccc::ast::BuiltInClass::UNSIGNED_16:
					result = QString::number(location.read16(cpu));
					break;
				case ccc::ast::BuiltInClass::SIGNED_16:
					result = QString::number((s16)location.read16(cpu));
					break;
				case ccc::ast::BuiltInClass::UNSIGNED_32:
					result = QString::number(location.read32(cpu));
					break;
				case ccc::ast::BuiltInClass::SIGNED_32:
					result = QString::number((s32)location.read32(cpu));
					break;
				case ccc::ast::BuiltInClass::FLOAT_32:
				{
					u32 value = location.read32(cpu);
					result = QString::number(*reinterpret_cast<float*>(&value));
					break;
				}
				case ccc::ast::BuiltInClass::UNSIGNED_64:
					result = QString::number(location.read64(cpu));
					break;
				case ccc::ast::BuiltInClass::SIGNED_64:
					result = QString::number((s64)location.read64(cpu));
					break;
				case ccc::ast::BuiltInClass::FLOAT_64:
				{
					u64 value = location.read64(cpu);
					result = QString::number(*reinterpret_cast<double*>(&value));
					break;
				}
				case ccc::ast::BuiltInClass::UNSIGNED_128:
				case ccc::ast::BuiltInClass::SIGNED_128:
				case ccc::ast::BuiltInClass::UNQUALIFIED_128:
				case ccc::ast::BuiltInClass::FLOAT_128:
				{
					if (depth > 0)
					{
						result = "(128-bit value)";
						break;
					}

					for (s32 i = 0; i < 16; i++)
					{
						u8 value = location.addOffset(i).read8(cpu);
						result += QString("%1 ").arg(value, 2, 16, QChar('0'));
						if ((i + 1) % 4 == 0)
							result += " ";
					}

					break;
				}
				default:
				{
				}
			}

			if (result.isEmpty())
				break;

			if (builtIn.name == "char")
			{
				char c = location.read8(cpu);
				if (QChar::fromLatin1(c).isPrint())
				{
					if (depth == 0)
						result = result.leftJustified(3);
					result += QString(" '%1'").arg(c);
				}
			}

			return result;
		}
		case ccc::ast::ENUM:
		{
			s32 value = (s32)location.read32(cpu);
			const auto& enum_type = physical_type.as<ccc::ast::Enum>();
			for (auto [test_value, name] : enum_type.constants)
			{
				if (test_value == value)
					return QString::fromStdString(name);
			}

			break;
		}
		case ccc::ast::POINTER_OR_REFERENCE:
		{
			const auto& pointer_or_reference = physical_type.as<ccc::ast::PointerOrReference>();
			const ccc::ast::Node& value_type =
				*pointer_or_reference.value_type->physical_type(database).first;

			u32 address = location.read32(cpu);
			if (address == 0)
				return "NULL";

			QString result = QString::number(address, 16);

			if (pointer_or_reference.is_pointer && value_type.name == "char")
			{
				const char* string = cpu.stringFromPointer(address);
				if (string)
					result += QString(" \"%1\"").arg(string);
			}
			else if (depth == 0)
			{
				QString pointee = generateDisplayString(value_type, cpu, database, depth + 1);
				if (!pointee.isEmpty())
					result += QString(" -> %1").arg(pointee);
			}

			return result;
		}
		case ccc::ast::POINTER_TO_DATA_MEMBER:
		{
			return QString::number(location.read32(cpu), 16);
		}
		case ccc::ast::STRUCT_OR_UNION:
		{
			const ccc::ast::StructOrUnion& struct_or_union = physical_type.as<ccc::ast::StructOrUnion>();

			QString result;
			result += "{";

			std::vector<ccc::ast::StructOrUnion::FlatField> fields;
			bool all_fields = struct_or_union.flatten_fields(fields, nullptr, database, true, 0, max_elements_to_display);

			for (size_t i = 0; i < fields.size(); i++)
			{
				const ccc::ast::StructOrUnion::FlatField& field = fields[i];

				SymbolTreeNode node;
				node.location = location.addOffset(field.base_offset + field.node->offset_bytes);

				const ccc::ast::Node& field_type = *field.node->physical_type(database).first;
				QString field_value = node.generateDisplayString(field_type, cpu, database, depth + 1);
				if (field_value.isEmpty())
					field_value = QString("(%1)").arg(ccc::ast::node_type_to_string(field_type));

				QString field_name = QString::fromStdString(field.node->name);
				result += QString(".%1=%2").arg(field_name).arg(field_value);

				if (i + 1 != fields.size() || !all_fields)
					result += ",";
			}

			if (!all_fields)
				result += "...";

			result += "}";
			return result;
		}
		default:
		{
		}
	}

	return QString();
}

bool SymbolTreeNode::updateLiveness(DebugInterface& cpu)
{
	std::optional<bool> new_liveness;
	if (live_range.low.valid() && live_range.high.valid())
	{
		u32 pc = cpu.getPC();
		new_liveness = pc >= live_range.low && pc < live_range.high;
	}

	if (new_liveness == m_liveness)
		return false;

	m_liveness = new_liveness;

	return true;
}

bool SymbolTreeNode::updateMatchesMemory(DebugInterface& cpu, const ccc::SymbolDatabase& database)
{
	bool matching = true;

	switch (symbol.descriptor())
	{
		case ccc::SymbolDescriptor::FUNCTION:
		{
			const ccc::Function* function = database.functions.symbol_from_handle(symbol.handle());
			if (!function || function->current_hash() == 0 || function->original_hash() == 0)
				return false;

			matching = function->current_hash() == function->original_hash();

			break;
		}
		case ccc::SymbolDescriptor::GLOBAL_VARIABLE:
		{
			const ccc::GlobalVariable* global_variable = database.global_variables.symbol_from_handle(symbol.handle());
			if (!global_variable)
				return false;

			const ccc::SourceFile* source_file = database.source_files.symbol_from_handle(global_variable->source_file());
			if (!source_file)
				return false;

			matching = source_file->functions_match();

			break;
		}
		case ccc::SymbolDescriptor::LOCAL_VARIABLE:
		{
			const ccc::LocalVariable* local_variable = database.local_variables.symbol_from_handle(symbol.handle());
			if (!local_variable)
				return false;

			const ccc::Function* function = database.functions.symbol_from_handle(local_variable->function());
			if (!function || function->current_hash() == 0 || function->original_hash() == 0)
				return false;

			matching = function->current_hash() == function->original_hash();

			break;
		}
		case ccc::SymbolDescriptor::PARAMETER_VARIABLE:
		{
			const ccc::ParameterVariable* parameter_variable = database.parameter_variables.symbol_from_handle(symbol.handle());
			if (!parameter_variable)
				return false;

			const ccc::Function* function = database.functions.symbol_from_handle(parameter_variable->function());
			if (!function || function->current_hash() == 0 || function->original_hash() == 0)
				return false;

			matching = function->current_hash() == function->original_hash();

			break;
		}
		default:
		{
		}
	}

	if (matching == m_matches_memory)
		return false;

	m_matches_memory = matching;

	return true;
}

bool SymbolTreeNode::matchesMemory() const
{
	return m_matches_memory;
}

void SymbolTreeNode::updateSymbolHashes(std::span<const SymbolTreeNode*> nodes, DebugInterface& cpu, ccc::SymbolDatabase& database)
{
	std::set<ccc::FunctionHandle> functions;
	std::set<ccc::SourceFile*> source_files;

	// Determine which functions we need to hash again, and in the case of
	// global variables, which source files are associated with those functions
	// so that we can check if they still match.
	for (const SymbolTreeNode* node : nodes)
	{
		switch (node->symbol.descriptor())
		{
			case ccc::SymbolDescriptor::FUNCTION:
			{
				functions.emplace(node->symbol.handle());
				break;
			}
			case ccc::SymbolDescriptor::GLOBAL_VARIABLE:
			{
				const ccc::GlobalVariable* global_variable = database.global_variables.symbol_from_handle(node->symbol.handle());
				if (!global_variable)
					continue;

				ccc::SourceFile* source_file = database.source_files.symbol_from_handle(global_variable->source_file());
				if (!source_file)
					continue;

				for (ccc::FunctionHandle function : source_file->functions())
					functions.emplace(function);

				source_files.emplace(source_file);

				break;
			}
			case ccc::SymbolDescriptor::LOCAL_VARIABLE:
			{
				const ccc::LocalVariable* local_variable = database.local_variables.symbol_from_handle(node->symbol.handle());
				if (!local_variable)
					continue;

				functions.emplace(local_variable->function());

				break;
			}
			case ccc::SymbolDescriptor::PARAMETER_VARIABLE:
			{
				const ccc::ParameterVariable* parameter_variable = database.parameter_variables.symbol_from_handle(node->symbol.handle());
				if (!parameter_variable)
					continue;

				functions.emplace(parameter_variable->function());

				break;
			}
			default:
			{
			}
		}
	}

	// Update the hashes for the enumerated functions.
	for (ccc::FunctionHandle function_handle : functions)
	{
		ccc::Function* function = database.functions.symbol_from_handle(function_handle);
		if (!function || function->original_hash() == 0)
			continue;

		std::optional<ccc::FunctionHash> hash = SymbolGuardian::HashFunction(*function, cpu);
		if (!hash.has_value())
			continue;

		function->set_current_hash(*hash);
	}

	// Check that the enumerated source files still have matching functions.
	for (ccc::SourceFile* source_file : source_files)
		source_file->check_functions_match(database);
}

bool SymbolTreeNode::anySymbolsValid(const ccc::SymbolDatabase& database) const
{
	if (symbol.lookup_symbol(database))
		return true;

	for (const std::unique_ptr<SymbolTreeNode>& child : children())
		if (child->anySymbolsValid(database))
			return true;

	return false;
}

const SymbolTreeNode* SymbolTreeNode::parent() const
{
	return m_parent;
}

const std::vector<std::unique_ptr<SymbolTreeNode>>& SymbolTreeNode::children() const
{
	return m_children;
}

bool SymbolTreeNode::childrenFetched() const
{
	return m_children_fetched;
}

void SymbolTreeNode::setChildren(std::vector<std::unique_ptr<SymbolTreeNode>> new_children)
{
	for (std::unique_ptr<SymbolTreeNode>& child : new_children)
		child->m_parent = this;
	m_children = std::move(new_children);
	m_children_fetched = true;
}

void SymbolTreeNode::insertChildren(std::vector<std::unique_ptr<SymbolTreeNode>> new_children)
{
	for (std::unique_ptr<SymbolTreeNode>& child : new_children)
		child->m_parent = this;
	m_children.insert(m_children.end(),
		std::make_move_iterator(new_children.begin()),
		std::make_move_iterator(new_children.end()));
	m_children_fetched = true;
}

void SymbolTreeNode::emplaceChild(std::unique_ptr<SymbolTreeNode> new_child)
{
	new_child->m_parent = this;
	m_children.emplace_back(std::move(new_child));
	m_children_fetched = true;
}

void SymbolTreeNode::clearChildren()
{
	m_children.clear();
	m_children_fetched = false;
}

void SymbolTreeNode::sortChildrenRecursively(bool sort_by_if_type_is_known)
{
	auto comparator = [&](const std::unique_ptr<SymbolTreeNode>& lhs, const std::unique_ptr<SymbolTreeNode>& rhs) -> bool {
		if (lhs->tag != rhs->tag)
			return lhs->tag < rhs->tag;
		if (sort_by_if_type_is_known && lhs->type.valid() != rhs->type.valid())
			return lhs->type.valid() > rhs->type.valid();
		if (lhs->location != rhs->location)
			return lhs->location < rhs->location;
		return lhs->name < rhs->name;
	};

	std::sort(m_children.begin(), m_children.end(), comparator);

	for (std::unique_ptr<SymbolTreeNode>& child : m_children)
		child->sortChildrenRecursively(sort_by_if_type_is_known);
}
