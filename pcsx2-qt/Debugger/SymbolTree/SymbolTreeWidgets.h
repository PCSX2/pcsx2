// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "ui_SymbolTreeWidget.h"

#include "Debugger/DebuggerWidget.h"
#include "Debugger/SymbolTree/SymbolTreeModel.h"

// A symbol tree widget with its associated refresh button, filter box and
// right-click menu. Supports grouping, sorting and various other settings.
class SymbolTreeWidget : public DebuggerWidget
{
	Q_OBJECT

public:
	virtual ~SymbolTreeWidget();

	void updateModel();
	void reset();
	void updateVisibleNodes(bool update_hashes);
	void expandGroups(QModelIndex index);

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

	SymbolTreeWidget(
		u32 flags,
		s32 symbol_address_alignment,
		const DebuggerWidgetParameters& parameters);

	void resizeEvent(QResizeEvent* event) override;

	void toJson(JsonValueWrapper& json) override;
	bool fromJson(const JsonValueWrapper& json) override;

	void setupTree();
	std::unique_ptr<SymbolTreeNode> buildTree(const ccc::SymbolDatabase& database);

	std::unique_ptr<SymbolTreeNode> groupBySourceFile(
		std::unique_ptr<SymbolTreeNode> child,
		const SymbolWork& child_work,
		SymbolTreeNode*& prev_group,
		const SymbolWork*& prev_work);

	std::unique_ptr<SymbolTreeNode> groupBySection(
		std::unique_ptr<SymbolTreeNode> child,
		const SymbolWork& child_work,
		SymbolTreeNode*& prev_group,
		const SymbolWork*& prev_work);

	std::unique_ptr<SymbolTreeNode> groupByModule(
		std::unique_ptr<SymbolTreeNode> child,
		const SymbolWork& child_work,
		SymbolTreeNode*& prev_group,
		const SymbolWork*& prev_work);

	void openContextMenu(QPoint pos);

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
	void onResetChildren();
	void onChangeTypeTemporarily();

	void onTreeViewClicked(const QModelIndex& index);

	SymbolTreeNode* currentNode();

	Ui::SymbolTreeWidget m_ui;

	SymbolTreeModel* m_model = nullptr;

	enum Flags
	{
		NO_SYMBOL_TREE_FLAGS = 0,
		ALLOW_GROUPING = 1 << 0,
		ALLOW_SORTING_BY_IF_TYPE_IS_KNOWN = 1 << 1,
		ALLOW_TYPE_ACTIONS = 1 << 2,
		ALLOW_MANGLED_NAME_ACTIONS = 1 << 3,
		CLICK_TO_GO_TO_IN_DISASSEMBLER = 1 << 4
	};

	u32 m_flags;
	u32 m_symbol_address_alignment;

	bool m_show_size_column = false;
	bool m_group_by_module = false;
	bool m_group_by_section = false;
	bool m_group_by_source_file = false;
	bool m_sort_by_if_type_is_known = false;
};

class FunctionTreeWidget : public SymbolTreeWidget
{
	Q_OBJECT
public:
	explicit FunctionTreeWidget(const DebuggerWidgetParameters& parameters);
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
	explicit GlobalVariableTreeWidget(const DebuggerWidgetParameters& parameters);
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
	explicit LocalVariableTreeWidget(const DebuggerWidgetParameters& parameters);
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
	explicit ParameterVariableTreeWidget(const DebuggerWidgetParameters& parameters);
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
