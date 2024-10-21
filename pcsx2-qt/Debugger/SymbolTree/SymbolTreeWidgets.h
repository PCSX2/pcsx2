// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <QtWidgets/QWidget>
#include "SymbolTreeModel.h"

#include "ui_SymbolTreeWidget.h"

struct SymbolFilters;

// A symbol tree widget with its associated refresh button, filter box and
// right-click menu. Supports grouping, sorting and various other settings.
class SymbolTreeWidget : public QWidget
{
	Q_OBJECT

public:
	virtual ~SymbolTreeWidget();

	void updateModel();
	void reset();
	void updateVisibleNodes(bool update_hashes);
	void expandGroups(QModelIndex index);

signals:
	void goToInDisassembly(u32 address);
	void goToInMemoryView(u32 address);
	void nameColumnClicked(u32 address);
	void locationColumnClicked(u32 address);

protected:
	struct SymbolWork
	{
		QString name;
		ccc::SymbolDescriptor descriptor;
		const ccc::Symbol* symbol = nullptr;
		const ccc::Module* module_symbol = nullptr;
		const ccc::Section* section = nullptr;
		const ccc::SourceFile* source_file = nullptr;
	};

	SymbolTreeWidget(u32 flags, s32 symbol_address_alignment, DebugInterface& cpu, QWidget* parent = nullptr);

	void resizeEvent(QResizeEvent* event) override;

	void setupTree();
	std::unique_ptr<SymbolTreeNode> buildTree(const SymbolFilters& filters, const ccc::SymbolDatabase& database);

	std::unique_ptr<SymbolTreeNode> groupBySourceFile(
		std::unique_ptr<SymbolTreeNode> child,
		const SymbolWork& child_work,
		SymbolTreeNode*& prev_group,
		const SymbolWork*& prev_work,
		const SymbolFilters& filters);

	std::unique_ptr<SymbolTreeNode> groupBySection(
		std::unique_ptr<SymbolTreeNode> child,
		const SymbolWork& child_work,
		SymbolTreeNode*& prev_group,
		const SymbolWork*& prev_work,
		const SymbolFilters& filters);

	std::unique_ptr<SymbolTreeNode> groupByModule(
		std::unique_ptr<SymbolTreeNode> child,
		const SymbolWork& child_work,
		SymbolTreeNode*& prev_group,
		const SymbolWork*& prev_work,
		const SymbolFilters& filters);

	void setupMenu();
	void openMenu(QPoint pos);

	virtual bool needsReset() const;

	virtual std::vector<SymbolWork> getSymbols(
		const QString& filter, const ccc::SymbolDatabase& database) = 0;

	virtual std::unique_ptr<SymbolTreeNode> buildNode(
		SymbolWork& work, const ccc::SymbolDatabase& database) const = 0;

	virtual void configureColumns() = 0;

	virtual void onNewButtonPressed() = 0;
	void onDeleteButtonPressed();

	void onCopyName();
	void onCopyMangledName();
	void onCopyLocation();
	void onRenameSymbol();
	void onGoToInDisassembly();
	void onGoToInMemoryView();
	void onResetChildren();
	void onChangeTypeTemporarily();

	void onTreeViewClicked(const QModelIndex& index);

	SymbolTreeNode* currentNode();

	Ui::SymbolTreeWidget m_ui;

	DebugInterface& m_cpu;
	SymbolTreeModel* m_model = nullptr;

	QMenu* m_context_menu = nullptr;
	QAction* m_rename_symbol = nullptr;
	QAction* m_go_to_in_disassembly = nullptr;
	QAction* m_m_go_to_in_memory_view = nullptr;
	QAction* m_show_size_column = nullptr;
	QAction* m_group_by_module = nullptr;
	QAction* m_group_by_section = nullptr;
	QAction* m_group_by_source_file = nullptr;
	QAction* m_sort_by_if_type_is_known = nullptr;
	QAction* m_reset_children = nullptr;
	QAction* m_change_type_temporarily = nullptr;

	enum Flags
	{
		NO_SYMBOL_TREE_FLAGS = 0,
		ALLOW_GROUPING = 1 << 0,
		ALLOW_SORTING_BY_IF_TYPE_IS_KNOWN = 1 << 1,
		ALLOW_TYPE_ACTIONS = 1 << 2,
		ALLOW_MANGLED_NAME_ACTIONS = 1 << 3
	};

	u32 m_flags;
	u32 m_symbol_address_alignment;
};

class FunctionTreeWidget : public SymbolTreeWidget
{
	Q_OBJECT
public:
	explicit FunctionTreeWidget(DebugInterface& cpu, QWidget* parent = nullptr);
	virtual ~FunctionTreeWidget();

protected:
	std::vector<SymbolWork> getSymbols(
		const QString& filter, const ccc::SymbolDatabase& database) override;

	std::unique_ptr<SymbolTreeNode> buildNode(
		SymbolWork& work, const ccc::SymbolDatabase& database) const override;

	void configureColumns() override;

	void onNewButtonPressed() override;
};

class GlobalVariableTreeWidget : public SymbolTreeWidget
{
	Q_OBJECT
public:
	explicit GlobalVariableTreeWidget(DebugInterface& cpu, QWidget* parent = nullptr);
	virtual ~GlobalVariableTreeWidget();

protected:
	std::vector<SymbolWork> getSymbols(
		const QString& filter, const ccc::SymbolDatabase& database) override;

	std::unique_ptr<SymbolTreeNode> buildNode(
		SymbolWork& work, const ccc::SymbolDatabase& database) const override;

	void configureColumns() override;

	void onNewButtonPressed() override;
};

class LocalVariableTreeWidget : public SymbolTreeWidget
{
	Q_OBJECT
public:
	explicit LocalVariableTreeWidget(DebugInterface& cpu, QWidget* parent = nullptr);
	virtual ~LocalVariableTreeWidget();

protected:
	bool needsReset() const override;

	std::vector<SymbolWork> getSymbols(
		const QString& filter, const ccc::SymbolDatabase& database) override;

	std::unique_ptr<SymbolTreeNode> buildNode(
		SymbolWork& work, const ccc::SymbolDatabase& database) const override;

	void configureColumns() override;

	void onNewButtonPressed() override;

	ccc::FunctionHandle m_function;
	std::optional<u32> m_caller_stack_pointer;
};

class ParameterVariableTreeWidget : public SymbolTreeWidget
{
	Q_OBJECT
public:
	explicit ParameterVariableTreeWidget(DebugInterface& cpu, QWidget* parent = nullptr);
	virtual ~ParameterVariableTreeWidget();

protected:
	bool needsReset() const override;

	std::vector<SymbolWork> getSymbols(
		const QString& filter, const ccc::SymbolDatabase& database) override;

	std::unique_ptr<SymbolTreeNode> buildNode(
		SymbolWork& work, const ccc::SymbolDatabase& database) const override;

	void configureColumns() override;

	void onNewButtonPressed() override;

	ccc::FunctionHandle m_function;
	std::optional<u32> m_caller_stack_pointer;
};

struct SymbolFilters
{
	bool group_by_module = false;
	bool group_by_section = false;
	bool group_by_source_file = false;
	QString string;
};
