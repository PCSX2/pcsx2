// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "SymbolTreeViews.h"

#include "Debugger/JsonValueWrapper.h"
#include "Debugger/SymbolTree/NewSymbolDialogs.h"
#include "Debugger/SymbolTree/SymbolTreeDelegates.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QScrollBar>

static bool testName(const QString& name, const QString& filter);

SymbolTreeView::SymbolTreeView(
	u32 flags,
	s32 symbol_address_alignment,
	const DebuggerViewParameters& parameters)
	: DebuggerView(parameters, MONOSPACE_FONT)
	, m_flags(flags)
	, m_symbol_address_alignment(symbol_address_alignment)
	, m_group_by_module(cpu().getCpuType() == BREAKPOINT_IOP)
{
	m_ui.setupUi(this);

	connect(m_ui.refreshButton, &QPushButton::clicked, this, [&]() {
		cpu().GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
			cpu().GetSymbolGuardian().UpdateFunctionHashes(database, cpu());
		});

		reset();
	});

	connect(m_ui.filterBox, &QLineEdit::textEdited, this, &SymbolTreeView::reset);

	connect(m_ui.newButton, &QPushButton::clicked, this, &SymbolTreeView::onNewButtonPressed);
	connect(m_ui.deleteButton, &QPushButton::clicked, this, &SymbolTreeView::onDeleteButtonPressed);

	connect(m_ui.treeView->verticalScrollBar(), &QScrollBar::valueChanged, this, [&]() {
		updateVisibleNodes(false);
	});

	m_ui.treeView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.treeView, &QTreeView::customContextMenuRequested, this, &SymbolTreeView::openContextMenu);

	connect(m_ui.treeView, &QTreeView::expanded, this, [&]() {
		updateVisibleNodes(true);
	});

	receiveEvent<DebuggerEvents::Refresh>([this](const DebuggerEvents::Refresh& event) -> bool {
		updateModel();
		return true;
	});
}

SymbolTreeView::~SymbolTreeView() = default;

void SymbolTreeView::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);

	updateVisibleNodes(false);
}

void SymbolTreeView::toJson(JsonValueWrapper& json)
{
	DebuggerView::toJson(json);

	json.value().AddMember("showSizeColumn", m_show_size_column, json.allocator());
	if (m_flags & ALLOW_GROUPING)
	{
		json.value().AddMember("groupByModule", m_group_by_module, json.allocator());
		json.value().AddMember("groupBySection", m_group_by_section, json.allocator());
		json.value().AddMember("groupBySourceFile", m_group_by_source_file, json.allocator());
	}

	if (m_flags & ALLOW_SORTING_BY_IF_TYPE_IS_KNOWN)
	{
		json.value().AddMember("sortByIfTypeIsKnown", m_sort_by_if_type_is_known, json.allocator());
	}
}

bool SymbolTreeView::fromJson(const JsonValueWrapper& json)
{
	if (!DebuggerView::fromJson(json))
		return false;

	bool needs_reset = false;

	auto show_size_column = json.value().FindMember("showSizeColumn");
	if (show_size_column != json.value().MemberEnd() && show_size_column->value.IsBool())
	{
		needs_reset |= show_size_column->value.GetBool() != m_show_size_column;
		m_show_size_column = show_size_column->value.GetBool();
	}

	if (m_flags & ALLOW_GROUPING)
	{
		auto group_by_module = json.value().FindMember("groupByModule");
		if (group_by_module != json.value().MemberEnd() && group_by_module->value.IsBool())
		{
			needs_reset |= group_by_module->value.GetBool() != m_group_by_module;
			m_group_by_module = group_by_module->value.GetBool();
		}

		auto group_by_section = json.value().FindMember("groupBySection");
		if (group_by_section != json.value().MemberEnd() && group_by_section->value.IsBool())
		{
			needs_reset |= group_by_section->value.GetBool() != m_group_by_section;
			m_group_by_section = group_by_section->value.GetBool();
		}

		auto group_by_source_file = json.value().FindMember("groupBySourceFile");
		if (group_by_source_file != json.value().MemberEnd() && group_by_source_file->value.IsBool())
		{
			needs_reset |= group_by_source_file->value.GetBool() != m_group_by_source_file;
			m_group_by_source_file = group_by_source_file->value.GetBool();
		}
	}

	if (m_flags & ALLOW_SORTING_BY_IF_TYPE_IS_KNOWN)
	{
		auto sort_by_if_type_is_known = json.value().FindMember("sortByIfTypeIsKnown");
		if (sort_by_if_type_is_known != json.value().MemberEnd() && sort_by_if_type_is_known->value.IsBool())
		{
			needs_reset |= sort_by_if_type_is_known->value.GetBool() != m_sort_by_if_type_is_known;
			m_sort_by_if_type_is_known = sort_by_if_type_is_known->value.GetBool();
		}
	}

	if (needs_reset)
		reset();

	return true;
}

void SymbolTreeView::updateModel()
{
	if (needsReset())
		reset();
	else
		updateVisibleNodes(true);
}

void SymbolTreeView::reset()
{
	if (!m_model)
		setupTree();

	m_ui.treeView->setColumnHidden(SymbolTreeModel::SIZE, !m_show_size_column);

	std::unique_ptr<SymbolTreeNode> root;
	cpu().GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) -> void {
		root = buildTree(database);
	});

	if (root)
	{
		root->sortChildrenRecursively(m_sort_by_if_type_is_known);
		m_model->reset(std::move(root));

		// Read the initial values for visible nodes.
		updateVisibleNodes(true);

		if (!m_ui.filterBox->text().isEmpty())
			expandGroups(QModelIndex());
	}
}

void SymbolTreeView::updateVisibleNodes(bool update_hashes)
{
	if (!m_model)
		return;

	// Enumerate visible symbol nodes.
	std::vector<const SymbolTreeNode*> nodes;
	QModelIndex index = m_ui.treeView->indexAt(m_ui.treeView->rect().topLeft());
	while (m_ui.treeView->visualRect(index).intersects(m_ui.treeView->viewport()->rect()))
	{
		nodes.emplace_back(m_model->nodeFromIndex(index));
		index = m_ui.treeView->indexBelow(index);
	}

	// Hash functions for symbols with visible nodes.
	if (update_hashes)
	{
		cpu().GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
			SymbolTreeNode::updateSymbolHashes(nodes, cpu(), database);
		});
	}

	// Update the values of visible nodes from memory.
	for (const SymbolTreeNode* node : nodes)
		m_model->setData(m_model->indexFromNode(*node), QVariant(), SymbolTreeModel::UPDATE_FROM_MEMORY_ROLE);

	m_ui.treeView->update();
}

void SymbolTreeView::expandGroups(QModelIndex index)
{
	if (!m_model)
		return;

	SymbolTreeNode* node = m_model->nodeFromIndex(index);
	if (node->tag == SymbolTreeNode::OBJECT)
		return;

	m_ui.treeView->expand(index);

	int child_count = m_model->rowCount(index);
	for (int i = 0; i < child_count; i++)
	{
		QModelIndex child = m_model->index(i, 0, index);
		expandGroups(child);
	}
}

void SymbolTreeView::setupTree()
{
	m_model = new SymbolTreeModel(cpu(), this);
	m_ui.treeView->setModel(m_model);

	auto location_delegate = new SymbolTreeLocationDelegate(cpu(), m_symbol_address_alignment, this);
	m_ui.treeView->setItemDelegateForColumn(SymbolTreeModel::LOCATION, location_delegate);

	auto type_delegate = new SymbolTreeTypeDelegate(cpu(), this);
	m_ui.treeView->setItemDelegateForColumn(SymbolTreeModel::TYPE, type_delegate);

	auto value_delegate = new SymbolTreeValueDelegate(cpu(), this);
	m_ui.treeView->setItemDelegateForColumn(SymbolTreeModel::VALUE, value_delegate);

	m_ui.treeView->setAlternatingRowColors(true);
	m_ui.treeView->setEditTriggers(QTreeView::AllEditTriggers);

	configureColumns();

	connect(m_ui.treeView, &QTreeView::pressed, this, &SymbolTreeView::onTreeViewClicked);
}

std::unique_ptr<SymbolTreeNode> SymbolTreeView::buildTree(const ccc::SymbolDatabase& database)
{
	std::vector<SymbolWork> symbols = getSymbols(m_ui.filterBox->text(), database);

	auto source_file_comparator = [](const SymbolWork& lhs, const SymbolWork& rhs) -> bool {
		if (lhs.source_file)
			return rhs.source_file && lhs.source_file->handle() < rhs.source_file->handle();
		else
			return rhs.source_file;
	};

	auto section_comparator = [](const SymbolWork& lhs, const SymbolWork& rhs) -> bool {
		if (lhs.section)
			return rhs.section && lhs.section->handle() < rhs.section->handle();
		else
			return rhs.section;
	};

	auto module_comparator = [](const SymbolWork& lhs, const SymbolWork& rhs) -> bool {
		if (lhs.module_symbol)
			return rhs.module_symbol && lhs.module_symbol->handle() < rhs.module_symbol->handle();
		else
			return rhs.module_symbol;
	};

	// Sort all of the symbols so that we can iterate over them in order and
	// build a tree.
	if (m_group_by_source_file)
		std::stable_sort(symbols.begin(), symbols.end(), source_file_comparator);

	if (m_group_by_section)
		std::stable_sort(symbols.begin(), symbols.end(), section_comparator);

	if (m_group_by_module)
		std::stable_sort(symbols.begin(), symbols.end(), module_comparator);

	std::unique_ptr<SymbolTreeNode> root = std::make_unique<SymbolTreeNode>();
	root->tag = SymbolTreeNode::ROOT;

	SymbolTreeNode* source_file_node = nullptr;
	SymbolTreeNode* section_node = nullptr;
	SymbolTreeNode* module_node = nullptr;

	const SymbolWork* source_file_work = nullptr;
	const SymbolWork* section_work = nullptr;
	const SymbolWork* module_work = nullptr;

	// Build the tree. Whenever we enounter a symbol with a different source
	// file, section or module, because they're all sorted we know that we have
	// to create a new group node (if we're grouping by that attribute).
	for (SymbolWork& work : symbols)
	{
		std::unique_ptr<SymbolTreeNode> node = buildNode(work, database);

		if (m_group_by_source_file)
		{
			node = groupBySourceFile(std::move(node), work, source_file_node, source_file_work);
			if (!node)
				continue;
		}

		if (m_group_by_section)
		{
			node = groupBySection(std::move(node), work, section_node, section_work);
			if (!node)
				continue;
		}

		if (m_group_by_module)
		{
			node = groupByModule(std::move(node), work, module_node, module_work);
			if (!node)
				continue;
		}

		root->emplaceChild(std::move(node));
	}

	return root;
}

std::unique_ptr<SymbolTreeNode> SymbolTreeView::groupBySourceFile(
	std::unique_ptr<SymbolTreeNode> child,
	const SymbolWork& child_work,
	SymbolTreeNode*& prev_group,
	const SymbolWork*& prev_work)
{
	bool group_exists =
		prev_group &&
		child_work.source_file == prev_work->source_file &&
		(!m_group_by_section || child_work.section == prev_work->section) &&
		(!m_group_by_module || child_work.module_symbol == prev_work->module_symbol);
	if (group_exists)
	{
		prev_group->emplaceChild(std::move(child));
		return nullptr;
	}

	std::unique_ptr<SymbolTreeNode> group_node = std::make_unique<SymbolTreeNode>();
	if (child_work.source_file)
	{
		group_node->tag = SymbolTreeNode::GROUP;
		if (!child_work.source_file->command_line_path.empty())
			group_node->name = QString::fromStdString(child_work.source_file->command_line_path);
		else
			group_node->name = QString::fromStdString(child_work.source_file->name());
		if (child_work.source_file->address().valid())
			group_node->location = SymbolTreeLocation(SymbolTreeLocation::MEMORY, child_work.source_file->address().value);
	}
	else
	{
		group_node->tag = SymbolTreeNode::UNKNOWN_GROUP;
		group_node->name = tr("(unknown source file)");
	}

	group_node->emplaceChild(std::move(child));
	child = std::move(group_node);

	prev_group = child.get();
	prev_work = &child_work;

	return child;
}

std::unique_ptr<SymbolTreeNode> SymbolTreeView::groupBySection(
	std::unique_ptr<SymbolTreeNode> child,
	const SymbolWork& child_work,
	SymbolTreeNode*& prev_group,
	const SymbolWork*& prev_work)
{
	bool group_exists =
		prev_group &&
		child_work.section == prev_work->section &&
		(!m_group_by_module || child_work.module_symbol == prev_work->module_symbol);
	if (group_exists)
	{
		prev_group->emplaceChild(std::move(child));
		return nullptr;
	}

	std::unique_ptr<SymbolTreeNode> group_node = std::make_unique<SymbolTreeNode>();
	if (child_work.section)
	{
		group_node->tag = SymbolTreeNode::GROUP;
		group_node->name = QString::fromStdString(child_work.section->name());
		if (child_work.section->address().valid())
			group_node->location = SymbolTreeLocation(SymbolTreeLocation::MEMORY, child_work.section->address().value);
	}
	else
	{
		group_node->tag = SymbolTreeNode::UNKNOWN_GROUP;
		group_node->name = tr("(unknown section)");
	}

	group_node->emplaceChild(std::move(child));
	child = std::move(group_node);

	prev_group = child.get();
	prev_work = &child_work;

	return child;
}

std::unique_ptr<SymbolTreeNode> SymbolTreeView::groupByModule(
	std::unique_ptr<SymbolTreeNode> child,
	const SymbolWork& child_work,
	SymbolTreeNode*& prev_group,
	const SymbolWork*& prev_work)
{
	bool group_exists =
		prev_group &&
		child_work.module_symbol == prev_work->module_symbol;
	if (group_exists)
	{
		prev_group->emplaceChild(std::move(child));
		return nullptr;
	}

	std::unique_ptr<SymbolTreeNode> group_node = std::make_unique<SymbolTreeNode>();
	if (child_work.module_symbol)
	{

		group_node->tag = SymbolTreeNode::GROUP;
		group_node->name = QString::fromStdString(child_work.module_symbol->name());
		if (child_work.module_symbol->address().valid())
			group_node->location = SymbolTreeLocation(SymbolTreeLocation::MEMORY, child_work.module_symbol->address().value);
		if (child_work.module_symbol->is_irx)
		{
			s32 major = child_work.module_symbol->version_major;
			s32 minor = child_work.module_symbol->version_minor;
			group_node->name += QString(" v%1.%2").arg(major).arg(minor);
		}
	}
	else
	{
		group_node->tag = SymbolTreeNode::UNKNOWN_GROUP;
		group_node->name = tr("(unknown module)");
	}

	group_node->emplaceChild(std::move(child));
	child = std::move(group_node);

	prev_group = child.get();
	prev_work = &child_work;

	return child;
}

void SymbolTreeView::openContextMenu(QPoint pos)
{
	SymbolTreeNode* node = currentNode();
	if (!node)
		return;

	bool node_is_object = node->tag == SymbolTreeNode::OBJECT;
	bool node_is_symbol = node->symbol.valid();
	bool node_is_memory = node->location.type == SymbolTreeLocation::MEMORY;

	QMenu* menu = new QMenu(this);
	menu->setAttribute(Qt::WA_DeleteOnClose);

	QAction* copy_name = menu->addAction(tr("Copy Name"));
	connect(copy_name, &QAction::triggered, this, &SymbolTreeView::onCopyName);

	if (m_flags & ALLOW_MANGLED_NAME_ACTIONS)
	{
		QAction* copy_mangled_name = menu->addAction(tr("Copy Mangled Name"));
		connect(copy_mangled_name, &QAction::triggered, this, &SymbolTreeView::onCopyMangledName);
	}

	QAction* copy_location = menu->addAction(tr("Copy Location"));
	connect(copy_location, &QAction::triggered, this, &SymbolTreeView::onCopyLocation);

	menu->addSeparator();

	QAction* rename_symbol = menu->addAction(tr("Rename Symbol"));
	rename_symbol->setEnabled(node_is_symbol);
	connect(rename_symbol, &QAction::triggered, this, &SymbolTreeView::onRenameSymbol);

	menu->addSeparator();

	std::vector<QAction*> go_to_actions = createEventActions<DebuggerEvents::GoToAddress>(
		menu, [this]() -> std::optional<DebuggerEvents::GoToAddress> {
			SymbolTreeNode* node = currentNode();
			if (!node)
				return std::nullopt;

			DebuggerEvents::GoToAddress event;
			event.address = node->location.address;
			return event;
		});

	for (QAction* action : go_to_actions)
		action->setEnabled(node_is_memory);

	QAction* show_size_column = menu->addAction(tr("Show Size Column"));
	show_size_column->setCheckable(true);
	show_size_column->setChecked(m_show_size_column);
	connect(show_size_column, &QAction::triggered, this, [this](bool checked) {
		m_show_size_column = checked;
		m_ui.treeView->setColumnHidden(SymbolTreeModel::SIZE, !m_show_size_column);
	});

	if (m_flags & ALLOW_GROUPING)
	{
		menu->addSeparator();

		QAction* group_by_module = menu->addAction(tr("Group by Module"));
		group_by_module->setCheckable(true);
		group_by_module->setChecked(m_group_by_module);
		connect(group_by_module, &QAction::toggled, this, [this](bool checked) {
			m_group_by_module = checked;
			reset();
		});

		QAction* group_by_section = menu->addAction(tr("Group by Section"));
		group_by_section->setCheckable(true);
		group_by_section->setChecked(m_group_by_section);
		connect(group_by_section, &QAction::toggled, this, [this](bool checked) {
			m_group_by_section = checked;
			reset();
		});

		QAction* group_by_source_file = menu->addAction(tr("Group by Source File"));
		group_by_source_file->setCheckable(true);
		group_by_source_file->setChecked(m_group_by_source_file);
		connect(group_by_source_file, &QAction::toggled, this, [this](bool checked) {
			m_group_by_source_file = checked;
			reset();
		});
	}

	if (m_flags & ALLOW_SORTING_BY_IF_TYPE_IS_KNOWN)
	{
		menu->addSeparator();

		QAction* sort_by_if_type_is_known = menu->addAction(tr("Sort by if type is known"));
		sort_by_if_type_is_known->setCheckable(true);
		sort_by_if_type_is_known->setChecked(m_sort_by_if_type_is_known);
		connect(sort_by_if_type_is_known, &QAction::toggled, this, [this](bool checked) {
			m_sort_by_if_type_is_known = checked;
			reset();
		});
	}

	if (m_flags & ALLOW_TYPE_ACTIONS)
	{
		menu->addSeparator();

		QAction* reset_children = menu->addAction(tr("Reset Children"));
		reset_children->setEnabled(node_is_object);
		connect(reset_children, &QAction::triggered, this, &SymbolTreeView::onResetChildren);

		QAction* change_type_temporarily = menu->addAction(tr("Change Type Temporarily"));
		change_type_temporarily->setEnabled(node_is_object);
		connect(change_type_temporarily, &QAction::triggered, this, &SymbolTreeView::onChangeTypeTemporarily);
	}

	menu->popup(m_ui.treeView->viewport()->mapToGlobal(pos));
}

bool SymbolTreeView::needsReset() const
{
	return !m_model || m_model->needsReset();
}

void SymbolTreeView::onDeleteButtonPressed()
{
	SymbolTreeNode* node = currentNode();
	if (!node)
		return;

	if (!node->symbol.valid())
		return;

	if (QMessageBox::question(this, tr("Confirm Deletion"), tr("Delete '%1'?").arg(node->name)) != QMessageBox::Yes)
		return;

	cpu().GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		node->symbol.destroy_symbol(database, true);
	});

	reset();
}

void SymbolTreeView::onCopyName()
{
	SymbolTreeNode* node = currentNode();
	if (!node)
		return;

	QApplication::clipboard()->setText(node->name);
}

void SymbolTreeView::onCopyMangledName()
{
	SymbolTreeNode* node = currentNode();
	if (!node)
		return;

	if (!node->mangled_name.isEmpty())
		QApplication::clipboard()->setText(node->mangled_name);
	else
		QApplication::clipboard()->setText(node->name);
}

void SymbolTreeView::onCopyLocation()
{
	SymbolTreeNode* node = currentNode();
	if (!node)
		return;

	QApplication::clipboard()->setText(node->location.toString(cpu()));
}

void SymbolTreeView::onRenameSymbol()
{
	SymbolTreeNode* node = currentNode();
	if (!node || !node->symbol.valid())
		return;

	QString title = tr("Rename Symbol");
	QString label = tr("Name:");

	QString text;
	cpu().GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) {
		const ccc::Symbol* symbol = node->symbol.lookup_symbol(database);
		if (!symbol || !symbol->address().valid())
			return;

		text = QString::fromStdString(symbol->name());
	});

	bool ok;
	std::string name = QInputDialog::getText(this, title, label, QLineEdit::Normal, text, &ok).toStdString();
	if (!ok)
		return;

	cpu().GetSymbolGuardian().ReadWrite([&](ccc::SymbolDatabase& database) {
		node->symbol.rename_symbol(name, database);
	});
}

void SymbolTreeView::onResetChildren()
{
	if (!m_model)
		return;

	QModelIndex index = m_ui.treeView->currentIndex();
	if (!index.isValid())
		return;

	m_model->resetChildren(index);
}

void SymbolTreeView::onChangeTypeTemporarily()
{
	if (!m_model)
		return;

	QModelIndex index = m_ui.treeView->currentIndex();
	if (!index.isValid())
		return;

	QString title = tr("Change Type To");
	QString label = tr("Type:");
	std::optional<QString> old_type = m_model->typeFromModelIndexToString(index);
	if (!old_type.has_value())
	{
		QMessageBox::warning(this, tr("Cannot Change Type"), tr("That node cannot have a type."));
		return;
	}

	bool ok;
	QString type_string = QInputDialog::getText(this, title, label, QLineEdit::Normal, *old_type, &ok);
	if (!ok)
		return;

	std::optional<QString> error_message = m_model->changeTypeTemporarily(index, type_string.toStdString());
	if (error_message.has_value() && !error_message->isEmpty())
		QMessageBox::warning(this, tr("Cannot Change Type"), *error_message);
}

void SymbolTreeView::onTreeViewClicked(const QModelIndex& index)
{
	if (!index.isValid())
		return;

	if ((m_flags & CLICK_TO_GO_TO_IN_DISASSEMBLER) == 0)
		return;

	if ((QGuiApplication::mouseButtons() & Qt::LeftButton) == 0)
		return;

	SymbolTreeNode* node = m_model->nodeFromIndex(index);
	if (!node || node->location.type != SymbolTreeLocation::MEMORY)
		return;

	goToInDisassembler(node->location.address, false);
}

SymbolTreeNode* SymbolTreeView::currentNode()
{
	if (!m_model)
		return nullptr;

	QModelIndex index = m_ui.treeView->currentIndex();
	return m_model->nodeFromIndex(index);
}

// *****************************************************************************

FunctionTreeView::FunctionTreeView(const DebuggerViewParameters& parameters)
	: SymbolTreeView(
		  ALLOW_GROUPING | ALLOW_MANGLED_NAME_ACTIONS | CLICK_TO_GO_TO_IN_DISASSEMBLER,
		  4,
		  parameters)
{
}

FunctionTreeView::~FunctionTreeView() = default;

std::vector<SymbolTreeView::SymbolWork> FunctionTreeView::getSymbols(
	const QString& filter, const ccc::SymbolDatabase& database)
{
	std::vector<SymbolTreeView::SymbolWork> symbols;

	for (const ccc::Function& function : database.functions)
	{
		if (!function.address().valid())
			continue;

		QString name = QString::fromStdString(function.name());
		if (!testName(name, filter))
			continue;

		SymbolWork& work = symbols.emplace_back();

		work.name = std::move(name);
		work.descriptor = ccc::FUNCTION;
		work.symbol = &function;

		work.module_symbol = database.modules.symbol_from_handle(function.module_handle());
		work.section = database.sections.symbol_overlapping_address(function.address());
		work.source_file = database.source_files.symbol_from_handle(function.source_file());
	}

	return symbols;
}

std::unique_ptr<SymbolTreeNode> FunctionTreeView::buildNode(
	SymbolWork& work, const ccc::SymbolDatabase& database) const
{
	const ccc::Function& function = static_cast<const ccc::Function&>(*work.symbol);

	std::unique_ptr<SymbolTreeNode> node = std::make_unique<SymbolTreeNode>();
	node->name = std::move(work.name);
	node->mangled_name = QString::fromStdString(function.mangled_name());
	node->location = SymbolTreeLocation(SymbolTreeLocation::MEMORY, function.address().value);
	node->size = function.size();
	node->symbol = ccc::MultiSymbolHandle(function);

	for (auto address_handle : database.labels.handles_from_address_range(function.address_range()))
	{
		const ccc::Label* label = database.labels.symbol_from_handle(address_handle.second);
		if (!label || label->address() == function.address())
			continue;

		std::unique_ptr<SymbolTreeNode> label_node = std::make_unique<SymbolTreeNode>();
		label_node->name = QString::fromStdString(label->name());
		label_node->location = SymbolTreeLocation(SymbolTreeLocation::MEMORY, label->address().value);
		node->emplaceChild(std::move(label_node));
	}

	return node;
}

void FunctionTreeView::configureColumns()
{
	m_ui.treeView->setColumnHidden(SymbolTreeModel::NAME, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::LOCATION, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::TYPE, true);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::LIVENESS, true);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::VALUE, true);

	m_ui.treeView->header()->setSectionResizeMode(SymbolTreeModel::NAME, QHeaderView::Stretch);

	m_ui.treeView->header()->setStretchLastSection(false);
}

void FunctionTreeView::onNewButtonPressed()
{
	NewFunctionDialog* dialog = new NewFunctionDialog(cpu(), this);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	if (dialog->exec() == QDialog::Accepted)
		reset();
}

// *****************************************************************************

GlobalVariableTreeView::GlobalVariableTreeView(const DebuggerViewParameters& parameters)
	: SymbolTreeView(
		  ALLOW_GROUPING | ALLOW_SORTING_BY_IF_TYPE_IS_KNOWN | ALLOW_TYPE_ACTIONS | ALLOW_MANGLED_NAME_ACTIONS,
		  1,
		  parameters)
{
}

GlobalVariableTreeView::~GlobalVariableTreeView() = default;

std::vector<SymbolTreeView::SymbolWork> GlobalVariableTreeView::getSymbols(
	const QString& filter, const ccc::SymbolDatabase& database)
{
	std::vector<SymbolTreeView::SymbolWork> symbols;

	for (const ccc::GlobalVariable& global_variable : database.global_variables)
	{
		if (!global_variable.address().valid())
			continue;

		QString name = QString::fromStdString(global_variable.name());
		if (!testName(name, filter))
			continue;

		SymbolWork& work = symbols.emplace_back();

		work.name = std::move(name);
		work.descriptor = ccc::GLOBAL_VARIABLE;
		work.symbol = &global_variable;

		work.module_symbol = database.modules.symbol_from_handle(global_variable.module_handle());
		work.section = database.sections.symbol_overlapping_address(global_variable.address());
		work.source_file = database.source_files.symbol_from_handle(global_variable.source_file());
	}

	// We also include static local variables in the global variable tree
	// because they have global storage. Why not.
	for (const ccc::LocalVariable& local_variable : database.local_variables)
	{
		if (!std::holds_alternative<ccc::GlobalStorage>(local_variable.storage))
			continue;

		if (!local_variable.address().valid())
			continue;

		ccc::FunctionHandle function_handle = local_variable.function();
		const ccc::Function* function = database.functions.symbol_from_handle(function_handle);

		QString function_name;
		if (function)
			function_name = QString::fromStdString(function->name());
		else
			function_name = tr("unknown function");

		QString name = QString("%1 (%2)")
						   .arg(QString::fromStdString(local_variable.name()))
						   .arg(function_name);
		if (!testName(name, filter))
			continue;

		SymbolWork& work = symbols.emplace_back();

		work.name = std::move(name);
		work.descriptor = ccc::LOCAL_VARIABLE;
		work.symbol = &local_variable;

		work.module_symbol = database.modules.symbol_from_handle(local_variable.module_handle());
		work.section = database.sections.symbol_overlapping_address(local_variable.address());
		if (function)
			work.source_file = database.source_files.symbol_from_handle(function->source_file());
	}

	return symbols;
}

std::unique_ptr<SymbolTreeNode> GlobalVariableTreeView::buildNode(
	SymbolWork& work, const ccc::SymbolDatabase& database) const
{
	std::unique_ptr<SymbolTreeNode> node = std::make_unique<SymbolTreeNode>();
	node->name = std::move(work.name);

	switch (work.descriptor)
	{
		case ccc::GLOBAL_VARIABLE:
		{
			const ccc::GlobalVariable& global_variable = static_cast<const ccc::GlobalVariable&>(*work.symbol);

			node->mangled_name = QString::fromStdString(global_variable.mangled_name());
			if (global_variable.type())
				node->type = ccc::NodeHandle(global_variable, global_variable.type());
			node->location = SymbolTreeLocation(SymbolTreeLocation::MEMORY, global_variable.address().value);
			node->is_location_editable = true;
			node->size = global_variable.size();
			node->symbol = ccc::MultiSymbolHandle(global_variable);

			break;
		}
		case ccc::LOCAL_VARIABLE:
		{
			const ccc::LocalVariable& local_variable = static_cast<const ccc::LocalVariable&>(*work.symbol);

			if (local_variable.type())
				node->type = ccc::NodeHandle(local_variable, local_variable.type());
			node->location = SymbolTreeLocation(SymbolTreeLocation::MEMORY, local_variable.address().value);
			node->is_location_editable = true;
			node->size = local_variable.size();
			node->symbol = ccc::MultiSymbolHandle(local_variable);

			break;
		}
		default:
		{
		}
	}

	return node;
}

void GlobalVariableTreeView::configureColumns()
{
	m_ui.treeView->setColumnHidden(SymbolTreeModel::NAME, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::LOCATION, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::TYPE, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::LIVENESS, true);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::VALUE, false);

	m_ui.treeView->header()->setSectionResizeMode(SymbolTreeModel::NAME, QHeaderView::Stretch);
	m_ui.treeView->header()->setSectionResizeMode(SymbolTreeModel::TYPE, QHeaderView::Stretch);
	m_ui.treeView->header()->setSectionResizeMode(SymbolTreeModel::VALUE, QHeaderView::Stretch);

	m_ui.treeView->header()->setStretchLastSection(false);
}

void GlobalVariableTreeView::onNewButtonPressed()
{
	NewGlobalVariableDialog* dialog = new NewGlobalVariableDialog(cpu(), this);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	if (dialog->exec() == QDialog::Accepted)
		reset();
}

// *****************************************************************************

LocalVariableTreeView::LocalVariableTreeView(const DebuggerViewParameters& parameters)
	: SymbolTreeView(
		  ALLOW_TYPE_ACTIONS,
		  1,
		  parameters)
{
}

LocalVariableTreeView::~LocalVariableTreeView() = default;

bool LocalVariableTreeView::needsReset() const
{
	if (!m_function.valid())
		return true;

	u32 program_counter = cpu().getPC();

	bool left_function = true;
	cpu().GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) {
		const ccc::Function* function = database.functions.symbol_from_handle(m_function);
		if (!function || !function->address().valid())
			return;

		u32 begin = function->address().value;
		u32 end = function->address().value + function->size();

		left_function = program_counter < begin || program_counter >= end;
	});

	if (left_function)
		return true;

	return SymbolTreeView::needsReset();
}

std::vector<SymbolTreeView::SymbolWork> LocalVariableTreeView::getSymbols(
	const QString& filter, const ccc::SymbolDatabase& database)
{
	u32 program_counter = cpu().getPC();
	const ccc::Function* function = database.functions.symbol_overlapping_address(program_counter);
	if (!function || !function->local_variables().has_value())
	{
		m_function = ccc::FunctionHandle();
		return std::vector<SymbolWork>();
	}

	m_function = function->handle();
	m_caller_stack_pointer = cpu().getCallerStackPointer(*function);

	std::vector<SymbolTreeView::SymbolWork> symbols;

	for (const ccc::LocalVariableHandle local_variable_handle : *function->local_variables())
	{
		const ccc::LocalVariable* local_variable = database.local_variables.symbol_from_handle(local_variable_handle);
		if (!local_variable)
			continue;

		if (std::holds_alternative<ccc::GlobalStorage>(local_variable->storage) && !local_variable->address().valid())
			continue;

		if (std::holds_alternative<ccc::StackStorage>(local_variable->storage) && !m_caller_stack_pointer.has_value())
			continue;

		QString name = QString::fromStdString(local_variable->name());
		if (!testName(name, filter))
			continue;

		SymbolWork& work = symbols.emplace_back();

		work.name = std::move(name);
		work.descriptor = ccc::LOCAL_VARIABLE;
		work.symbol = local_variable;

		work.module_symbol = database.modules.symbol_from_handle(local_variable->module_handle());
		work.section = database.sections.symbol_overlapping_address(local_variable->address());
		work.source_file = database.source_files.symbol_from_handle(function->source_file());
	}

	return symbols;
}

std::unique_ptr<SymbolTreeNode> LocalVariableTreeView::buildNode(
	SymbolWork& work, const ccc::SymbolDatabase& database) const
{
	const ccc::LocalVariable& local_variable = static_cast<const ccc::LocalVariable&>(*work.symbol);

	std::unique_ptr<SymbolTreeNode> node = std::make_unique<SymbolTreeNode>();
	node->name = QString::fromStdString(local_variable.name());
	if (local_variable.type())
		node->type = ccc::NodeHandle(local_variable, local_variable.type());
	node->live_range = local_variable.live_range;
	node->symbol = ccc::MultiSymbolHandle(local_variable);

	if (std::get_if<ccc::GlobalStorage>(&local_variable.storage))
		node->location = SymbolTreeLocation(SymbolTreeLocation::MEMORY, local_variable.address().value);
	else if (const ccc::RegisterStorage* storage = std::get_if<ccc::RegisterStorage>(&local_variable.storage))
		node->location = SymbolTreeLocation(SymbolTreeLocation::REGISTER, storage->dbx_register_number);
	else if (const ccc::StackStorage* storage = std::get_if<ccc::StackStorage>(&local_variable.storage))
		node->location = SymbolTreeLocation(SymbolTreeLocation::MEMORY, *m_caller_stack_pointer + storage->stack_pointer_offset);
	node->size = local_variable.size();

	return node;
}

void LocalVariableTreeView::configureColumns()
{
	m_ui.treeView->setColumnHidden(SymbolTreeModel::NAME, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::LOCATION, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::TYPE, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::LIVENESS, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::VALUE, false);

	m_ui.treeView->header()->setSectionResizeMode(SymbolTreeModel::NAME, QHeaderView::Stretch);
	m_ui.treeView->header()->setSectionResizeMode(SymbolTreeModel::TYPE, QHeaderView::Stretch);
	m_ui.treeView->header()->setSectionResizeMode(SymbolTreeModel::VALUE, QHeaderView::Stretch);

	m_ui.treeView->header()->setStretchLastSection(false);
}

void LocalVariableTreeView::onNewButtonPressed()
{
	NewLocalVariableDialog* dialog = new NewLocalVariableDialog(cpu(), this);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	if (dialog->exec() == QDialog::Accepted)
		reset();
}

// *****************************************************************************

ParameterVariableTreeView::ParameterVariableTreeView(const DebuggerViewParameters& parameters)
	: SymbolTreeView(
		  ALLOW_TYPE_ACTIONS,
		  1,
		  parameters)
{
}

ParameterVariableTreeView::~ParameterVariableTreeView() = default;

bool ParameterVariableTreeView::needsReset() const
{
	if (!m_function.valid())
		return true;

	u32 program_counter = cpu().getPC();

	bool left_function = true;
	cpu().GetSymbolGuardian().Read([&](const ccc::SymbolDatabase& database) {
		const ccc::Function* function = database.functions.symbol_from_handle(m_function);
		if (!function || !function->address().valid())
			return;

		u32 begin = function->address().value;
		u32 end = function->address().value + function->size();

		left_function = program_counter < begin || program_counter >= end;
	});

	if (left_function)
		return true;

	return SymbolTreeView::needsReset();
}

std::vector<SymbolTreeView::SymbolWork> ParameterVariableTreeView::getSymbols(
	const QString& filter, const ccc::SymbolDatabase& database)
{
	std::vector<SymbolTreeView::SymbolWork> symbols;

	u32 program_counter = cpu().getPC();
	const ccc::Function* function = database.functions.symbol_overlapping_address(program_counter);
	if (!function || !function->parameter_variables().has_value())
	{
		m_function = ccc::FunctionHandle();
		return std::vector<SymbolWork>();
	}

	m_function = function->handle();
	m_caller_stack_pointer = cpu().getCallerStackPointer(*function);

	for (const ccc::ParameterVariableHandle parameter_variable_handle : *function->parameter_variables())
	{
		const ccc::ParameterVariable* parameter_variable = database.parameter_variables.symbol_from_handle(parameter_variable_handle);
		if (!parameter_variable)
			continue;

		if (std::holds_alternative<ccc::StackStorage>(parameter_variable->storage) && !m_caller_stack_pointer.has_value())
			continue;

		QString name = QString::fromStdString(parameter_variable->name());
		if (!testName(name, filter))
			continue;

		ccc::FunctionHandle function_handle = parameter_variable->function();
		const ccc::Function* function = database.functions.symbol_from_handle(function_handle);

		SymbolWork& work = symbols.emplace_back();

		work.name = std::move(name);
		work.descriptor = ccc::PARAMETER_VARIABLE;
		work.symbol = parameter_variable;

		work.module_symbol = database.modules.symbol_from_handle(parameter_variable->module_handle());
		work.section = database.sections.symbol_overlapping_address(parameter_variable->address());
		if (function)
			work.source_file = database.source_files.symbol_from_handle(function->source_file());
	}

	return symbols;
}

std::unique_ptr<SymbolTreeNode> ParameterVariableTreeView::buildNode(
	SymbolWork& work, const ccc::SymbolDatabase& database) const
{
	const ccc::ParameterVariable& parameter_variable = static_cast<const ccc::ParameterVariable&>(*work.symbol);

	std::unique_ptr<SymbolTreeNode> node = std::make_unique<SymbolTreeNode>();
	node->name = QString::fromStdString(parameter_variable.name());
	if (parameter_variable.type())
		node->type = ccc::NodeHandle(parameter_variable, parameter_variable.type());
	node->symbol = ccc::MultiSymbolHandle(parameter_variable);

	if (const ccc::RegisterStorage* storage = std::get_if<ccc::RegisterStorage>(&parameter_variable.storage))
		node->location = SymbolTreeLocation(SymbolTreeLocation::REGISTER, storage->dbx_register_number);
	else if (const ccc::StackStorage* storage = std::get_if<ccc::StackStorage>(&parameter_variable.storage))
		node->location = SymbolTreeLocation(SymbolTreeLocation::MEMORY, *m_caller_stack_pointer + storage->stack_pointer_offset);
	node->size = parameter_variable.size();

	return node;
}

void ParameterVariableTreeView::configureColumns()
{
	m_ui.treeView->setColumnHidden(SymbolTreeModel::NAME, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::LOCATION, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::TYPE, false);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::LIVENESS, true);
	m_ui.treeView->setColumnHidden(SymbolTreeModel::VALUE, false);

	m_ui.treeView->header()->setSectionResizeMode(SymbolTreeModel::NAME, QHeaderView::Stretch);
	m_ui.treeView->header()->setSectionResizeMode(SymbolTreeModel::TYPE, QHeaderView::Stretch);
	m_ui.treeView->header()->setSectionResizeMode(SymbolTreeModel::VALUE, QHeaderView::Stretch);

	m_ui.treeView->header()->setStretchLastSection(false);
}

void ParameterVariableTreeView::onNewButtonPressed()
{
	NewParameterVariableDialog* dialog = new NewParameterVariableDialog(cpu(), this);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	if (dialog->exec() == QDialog::Accepted)
		reset();
}

static bool testName(const QString& name, const QString& filter)
{
	return filter.isEmpty() || name.contains(filter, Qt::CaseInsensitive);
}
