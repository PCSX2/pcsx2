// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtCore/QAbstractItemModel>

#include <ccc/ast.h>
#include <ccc/symbol_database.h>

#include "common/Pcsx2Defs.h"
#include "DebugTools/DebugInterface.h"
#include "SymbolTreeNode.h"

// Model for the symbol trees. It will dynamically grow itself as the user
// chooses to expand different nodes.
class SymbolTreeModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	enum Column
	{
		NAME = 0,
		VALUE = 1,
		LOCATION = 2,
		SIZE = 3,
		TYPE = 4,
		LIVENESS = 5,
		COLUMN_COUNT = 6
	};

	enum SetDataRole
	{
		EDIT_ROLE = Qt::EditRole,
		UPDATE_FROM_MEMORY_ROLE = Qt::UserRole
	};

	SymbolTreeModel(DebugInterface& cpu, QObject* parent = nullptr);

	QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex& index) const override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	bool setData(const QModelIndex& index, const QVariant& value, int role = EDIT_ROLE) override;
	void fetchMore(const QModelIndex& parent) override;
	bool canFetchMore(const QModelIndex& parent) const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

	QModelIndex indexFromNode(const SymbolTreeNode& node) const;
	SymbolTreeNode* nodeFromIndex(const QModelIndex& index) const;

	// Reset the whole model.
	void reset(std::unique_ptr<SymbolTreeNode> new_root);

	// Remove all the children of a given node, and allow fetching again.
	void resetChildren(QModelIndex index);
	void resetChildrenRecursive(SymbolTreeNode& node);

	bool needsReset() const;

	std::optional<QString> changeTypeTemporarily(QModelIndex index, std::string_view type_string);
	std::optional<QString> typeFromModelIndexToString(QModelIndex index);

protected:
	static std::vector<std::unique_ptr<SymbolTreeNode>> populateChildren(
		const QString& name,
		SymbolTreeLocation location,
		const ccc::ast::Node& logical_type,
		ccc::NodeHandle parent_handle,
		DebugInterface& cpu,
		const ccc::SymbolDatabase& database);

	static bool nodeHasChildren(const ccc::ast::Node& logical_type, const ccc::SymbolDatabase& database);

	std::unique_ptr<SymbolTreeNode> m_root;
	QString m_filter;
	DebugInterface& m_cpu;
};
