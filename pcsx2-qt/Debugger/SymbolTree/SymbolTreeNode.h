// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtCore/QString>
#include <QtCore/QVariant>

#include "SymbolTreeLocation.h"

class DebugInterface;

// A node in a symbol tree model.
struct SymbolTreeNode
{
public:
	enum Tag
	{
		ROOT,
		UNKNOWN_GROUP,
		GROUP,
		OBJECT
	};

	Tag tag = OBJECT;
	ccc::MultiSymbolHandle symbol;
	QString name;
	SymbolTreeLocation location;
	bool is_location_editable = false;
	std::optional<u32> size;
	ccc::NodeHandle type;
	std::unique_ptr<ccc::ast::Node> temporary_type;
	ccc::AddressRange live_range;

	SymbolTreeNode() {}
	~SymbolTreeNode() {}

	SymbolTreeNode(const SymbolTreeNode& rhs) = delete;
	SymbolTreeNode& operator=(const SymbolTreeNode& rhs) = delete;

	SymbolTreeNode(SymbolTreeNode&& rhs) = delete;
	SymbolTreeNode& operator=(SymbolTreeNode&& rhs) = delete;

	// Generated from VM state, to be updated regularly.
	const QVariant& value() const;
	const QString& display_value() const;
	std::optional<bool> liveness();

	// Read the value from the VM memory, update liveness information, and
	// generate a display string. Returns true if the data changed.
	bool readFromVM(DebugInterface& cpu, const ccc::SymbolDatabase& database);

	// Write the value back to the VM memory. Returns true on success.
	bool writeToVM(QVariant value, DebugInterface& cpu, const ccc::SymbolDatabase& database);

	QVariant readValueAsVariant(const ccc::ast::Node& physical_type, DebugInterface& cpu, const ccc::SymbolDatabase& database) const;
	bool writeValueFromVariant(QVariant value, const ccc::ast::Node& physical_type, DebugInterface& cpu) const;

	bool updateDisplayString(DebugInterface& cpu, const ccc::SymbolDatabase& database);
	QString generateDisplayString(const ccc::ast::Node& physical_type, DebugInterface& cpu, const ccc::SymbolDatabase& database, s32 depth) const;

	bool updateLiveness(DebugInterface& cpu);

	bool anySymbolsValid(const ccc::SymbolDatabase& database) const;

	const SymbolTreeNode* parent() const;

	const std::vector<std::unique_ptr<SymbolTreeNode>>& children() const;
	bool childrenFetched() const;
	void setChildren(std::vector<std::unique_ptr<SymbolTreeNode>> new_children);
	void insertChildren(std::vector<std::unique_ptr<SymbolTreeNode>> new_children);
	void emplaceChild(std::unique_ptr<SymbolTreeNode> new_child);
	void clearChildren();

	void sortChildrenRecursively(bool sort_by_if_type_is_known);

protected:
	QVariant m_value;
	QString m_display_value;
	std::optional<bool> m_liveness;

	SymbolTreeNode* m_parent = nullptr;
	std::vector<std::unique_ptr<SymbolTreeNode>> m_children;
	bool m_children_fetched = false;
};
