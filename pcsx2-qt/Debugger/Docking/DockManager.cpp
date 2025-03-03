// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DockManager.h"

#include "Debugger/DebuggerWidget.h"
#include "Debugger/DebuggerWindow.h"
#include "Debugger/Docking/DockTables.h"
#include "Debugger/Docking/DockViews.h"
#include "Debugger/Docking/DropIndicators.h"
#include "Debugger/Docking/LayoutEditorDialog.h"
#include "Debugger/Docking/NoLayoutsWidget.h"

#include "common/Assertions.h"
#include "common/FileSystem.h"
#include "common/StringUtil.h"
#include "common/Path.h"

#include <kddockwidgets/Config.h>
#include <kddockwidgets/core/Group.h>
#include <kddockwidgets/core/Stack.h>
#include <kddockwidgets/core/indicators/SegmentedDropIndicatorOverlay.h>
#include <kddockwidgets/qtwidgets/Stack.h>

#include <QtCore/QTimer>
#include <QtCore/QtTranslation>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

DockManager::DockManager(QObject* parent)
	: QObject(parent)
{
	QTimer* autosave_timer = new QTimer(this);
	connect(autosave_timer, &QTimer::timeout, this, &DockManager::saveCurrentLayout);
	autosave_timer->start(60 * 1000);

	m_blink_timer = new QTimer(this);
	connect(m_blink_timer, &QTimer::timeout, this, &DockManager::layoutSwitcherUpdateBlink);
}

void DockManager::configureDockingSystem()
{
	std::string indicator_style = Host::GetBaseStringSettingValue(
		"Debugger/UserInterface", "DropIndicatorStyle", "Classic");

	if (indicator_style == "Segmented" || indicator_style == "Minimalistic")
	{
		KDDockWidgets::Core::ViewFactory::s_dropIndicatorType = KDDockWidgets::DropIndicatorType::Segmented;
		DockSegmentedDropIndicatorOverlay::s_indicator_style = indicator_style;
	}
	else
	{
		KDDockWidgets::Core::ViewFactory::s_dropIndicatorType = KDDockWidgets::DropIndicatorType::Classic;
	}

	static bool done = false;
	if (done)
		return;

	KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtWidgets);

	KDDockWidgets::Config& config = KDDockWidgets::Config::self();

	config.setFlags(
		KDDockWidgets::Config::Flag_HideTitleBarWhenTabsVisible |
		KDDockWidgets::Config::Flag_AlwaysShowTabs |
		KDDockWidgets::Config::Flag_AllowReorderTabs |
		KDDockWidgets::Config::Flag_TitleBarIsFocusable);

	// We set this flag regardless of whether or not the windowing system
	// supports compositing since it's only used by the built-in docking
	// indicator, and we only fall back to that if compositing is disabled.
	config.setInternalFlags(KDDockWidgets::Config::InternalFlag_DisableTranslucency);

	config.setDockWidgetFactoryFunc(&DockManager::dockWidgetFactory);
	config.setViewFactory(new DockViewFactory());
	config.setDragAboutToStartFunc(&DockManager::dragAboutToStart);
	config.setStartDragDistance(std::max(QApplication::startDragDistance(), 32));

	done = true;
}

bool DockManager::deleteLayout(DockLayout::Index layout_index)
{
	pxAssertRel(layout_index != DockLayout::INVALID_INDEX,
		"DockManager::deleteLayout called with INVALID_INDEX.");

	if (layout_index == m_current_layout)
	{
		DockLayout::Index other_layout = DockLayout::INVALID_INDEX;
		if (layout_index + 1 < m_layouts.size())
			other_layout = layout_index + 1;
		else if (layout_index > 0)
			other_layout = layout_index - 1;

		switchToLayout(other_layout);
	}

	m_layouts.at(layout_index).deleteFile();
	m_layouts.erase(m_layouts.begin() + layout_index);

	// All the layouts after the one being deleted have been shifted over by
	// one, so adjust the current layout index accordingly.
	if (m_current_layout > layout_index && m_current_layout != DockLayout::INVALID_INDEX)
		m_current_layout--;

	if (m_layouts.empty() && g_debugger_window)
	{
		NoLayoutsWidget* widget = new NoLayoutsWidget;
		connect(widget->createDefaultLayoutsButton(), &QPushButton::clicked, this, &DockManager::resetAllLayouts);

		KDDockWidgets::QtWidgets::DockWidget* dock = new KDDockWidgets::QtWidgets::DockWidget("placeholder");
		dock->setTitle(tr("No Layouts"));
		dock->setWidget(widget);
		g_debugger_window->addDockWidget(dock, KDDockWidgets::Location_OnTop);
	}

	return true;
}

void DockManager::switchToLayout(DockLayout::Index layout_index, bool blink_tab)
{
	if (layout_index != m_current_layout)
	{
		if (m_current_layout != DockLayout::INVALID_INDEX)
		{
			DockLayout& layout = m_layouts.at(m_current_layout);
			layout.freeze();
			layout.save(m_current_layout);
		}

		// Clear out the existing positions of toolbars so they don't affect
		// where new toolbars appear for other layouts.
		if (g_debugger_window)
			g_debugger_window->clearToolBarState();

		updateToolBarLockState();

		m_current_layout = layout_index;

		if (m_current_layout != DockLayout::INVALID_INDEX)
		{
			DockLayout& layout = m_layouts.at(m_current_layout);
			layout.thaw();

			int tab_index = static_cast<int>(layout_index);
			if (m_switcher && tab_index >= 0 && tab_index < m_plus_tab_index)
			{
				m_ignore_current_tab_changed = true;
				m_switcher->setCurrentIndex(tab_index);
				m_ignore_current_tab_changed = false;
			}
		}
	}

	if (blink_tab)
		layoutSwitcherStartBlink();
}

bool DockManager::switchToLayoutWithCPU(BreakPointCpu cpu, bool blink_tab)
{
	// Don't interrupt the user if the current layout already has the right CPU.
	if (m_current_layout != DockLayout::INVALID_INDEX && m_layouts.at(m_current_layout).cpu() == cpu)
	{
		switchToLayout(m_current_layout, blink_tab);
		return true;
	}

	for (DockLayout::Index i = 0; i < m_layouts.size(); i++)
	{
		if (m_layouts[i].cpu() == cpu)
		{
			switchToLayout(i, blink_tab);
			return true;
		}
	}

	return false;
}

void DockManager::loadLayouts()
{
	m_layouts.clear();

	// Load the layouts.
	FileSystem::FindResultsArray files;
	FileSystem::FindFiles(
		EmuFolders::DebuggerLayouts.c_str(),
		"*.json",
		FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES,
		&files);

	bool needs_reset = false;
	bool order_changed = false;
	std::vector<DockLayout::Index> indices_last_session;

	for (const FILESYSTEM_FIND_DATA& ffd : files)
	{
		DockLayout::LoadResult result;
		DockLayout::Index index_last_session = DockLayout::INVALID_INDEX;
		DockLayout::Index index =
			createLayout(ffd.FileName, result, index_last_session);

		DockLayout& layout = m_layouts.at(index);

		// Try to make sure the layout has a unique name.
		const QString& name = layout.name();
		QString new_name = name;
		if (result == DockLayout::SUCCESS || result == DockLayout::DEFAULT_LAYOUT_HASH_MISMATCH)
		{
			for (int i = 2; hasNameConflict(new_name, index) && i < 100; i++)
			{
				if (i == 99)
				{
					result = DockLayout::CONFLICTING_NAME;
					break;
				}

				new_name = QString("%1 #%2").arg(name).arg(i);
			}
		}

		needs_reset |= result != DockLayout::SUCCESS;

		if (result != DockLayout::SUCCESS && result != DockLayout::DEFAULT_LAYOUT_HASH_MISMATCH)
		{
			deleteLayout(index);

			// Only delete the file if we've identified that it's actually a
			// layout file.
			if (result == DockLayout::MAJOR_VERSION_MISMATCH || result == DockLayout::CONFLICTING_NAME)
				FileSystem::DeleteFilePath(ffd.FileName.c_str());

			continue;
		}

		if (new_name != name)
		{
			layout.setName(new_name);
			layout.save(index);
		}

		if (index_last_session != index)
			order_changed = true;

		indices_last_session.emplace_back(index_last_session);
	}

	// Make sure the layouts remain in the same order they were in previously.
	std::vector<DockLayout*> layout_pointers;
	for (DockLayout& layout : m_layouts)
		layout_pointers.emplace_back(&layout);

	std::sort(layout_pointers.begin(), layout_pointers.end(),
		[this, &indices_last_session](const DockLayout* lhs, const DockLayout* rhs) {
			size_t lhs_index = lhs - m_layouts.data();
			size_t rhs_index = rhs - m_layouts.data();
			DockLayout::Index lhs_index_last_session = indices_last_session.at(lhs_index);
			DockLayout::Index rhs_index_last_session = indices_last_session.at(rhs_index);
			return lhs_index_last_session < rhs_index_last_session;
		});

	std::vector<DockLayout> sorted_layouts;
	for (size_t i = 0; i < layout_pointers.size(); i++)
		sorted_layouts.emplace_back(std::move(*layout_pointers[i]));

	m_layouts = std::move(sorted_layouts);

	if (m_layouts.empty() || needs_reset)
		resetDefaultLayouts();
	else
		updateLayoutSwitcher();

	// Make sure the indices in the existing layout files match up with the
	// indices of any new layouts.
	if (order_changed)
		saveLayouts();
}

bool DockManager::saveLayouts()
{
	for (DockLayout::Index i = 0; i < m_layouts.size(); i++)
		if (!m_layouts[i].save(i))
			return false;

	return true;
}

bool DockManager::saveCurrentLayout()
{
	if (m_current_layout == DockLayout::INVALID_INDEX)
		return true;

	return m_layouts.at(m_current_layout).save(m_current_layout);
}

void DockManager::resetAllLayouts()
{
	switchToLayout(DockLayout::INVALID_INDEX);

	for (DockLayout& layout : m_layouts)
		layout.deleteFile();

	m_layouts.clear();

	for (const DockTables::DefaultDockLayout& layout : DockTables::DEFAULT_DOCK_LAYOUTS)
		createLayout(tr(layout.name.c_str()), layout.cpu, true, layout.name);

	switchToLayout(0);
	updateLayoutSwitcher();
	saveLayouts();
}

void DockManager::resetDefaultLayouts()
{
	switchToLayout(DockLayout::INVALID_INDEX);

	std::vector<DockLayout> old_layouts = std::move(m_layouts);
	m_layouts = std::vector<DockLayout>();

	for (const DockTables::DefaultDockLayout& layout : DockTables::DEFAULT_DOCK_LAYOUTS)
		createLayout(tr(layout.name.c_str()), layout.cpu, true, layout.name);

	for (DockLayout& layout : old_layouts)
		if (!layout.isDefault())
			m_layouts.emplace_back(std::move(layout));
		else
			layout.deleteFile();

	switchToLayout(0);
	updateLayoutSwitcher();
	saveLayouts();
}

void DockManager::createToolsMenu(QMenu* menu)
{
	menu->clear();

	if (m_current_layout == DockLayout::INVALID_INDEX || !g_debugger_window)
		return;

	for (QToolBar* widget : g_debugger_window->findChildren<QToolBar*>())
	{
		QAction* action = menu->addAction(widget->windowTitle());
		action->setText(widget->windowTitle());
		action->setCheckable(true);
		action->setChecked(widget->isVisible());
		connect(action, &QAction::triggered, this, [widget]() {
			widget->setVisible(!widget->isVisible());
		});
		menu->addAction(action);
	}
}

void DockManager::createWindowsMenu(QMenu* menu)
{
	menu->clear();

	if (m_current_layout == DockLayout::INVALID_INDEX)
		return;

	DockLayout& layout = m_layouts.at(m_current_layout);

	// Create a menu that allows for multiple dock widgets of the same type to
	// be opened.
	QMenu* add_another_menu = menu->addMenu(tr("Add Another..."));

	std::vector<DebuggerWidget*> add_another_widgets;
	std::set<std::string> add_another_types;
	for (const auto& [unique_name, widget] : layout.debuggerWidgets())
	{
		std::string type = widget->metaObject()->className();

		if (widget->supportsMultipleInstances() && !add_another_types.contains(type))
		{
			add_another_widgets.emplace_back(widget);
			add_another_types.emplace(type);
		}
	}

	std::sort(add_another_widgets.begin(), add_another_widgets.end(),
		[](const DebuggerWidget* lhs, const DebuggerWidget* rhs) {
			if (lhs->displayNameWithoutSuffix() == rhs->displayNameWithoutSuffix())
				return lhs->displayNameSuffixNumber() < rhs->displayNameSuffixNumber();

			return lhs->displayNameWithoutSuffix() < rhs->displayNameWithoutSuffix();
		});

	for (DebuggerWidget* widget : add_another_widgets)
	{
		const char* type = widget->metaObject()->className();

		const auto description_iterator = DockTables::DEBUGGER_WIDGETS.find(type);
		pxAssert(description_iterator != DockTables::DEBUGGER_WIDGETS.end());

		QAction* action = add_another_menu->addAction(description_iterator->second.display_name);
		connect(action, &QAction::triggered, this, [this, type]() {
			if (m_current_layout == DockLayout::INVALID_INDEX)
				return;

			m_layouts.at(m_current_layout).createDebuggerWidget(type);
		});
	}

	if (add_another_widgets.empty())
		add_another_menu->setDisabled(true);

	menu->addSeparator();

	struct DebuggerWidgetToggle
	{
		QString display_name;
		std::optional<int> suffix_number;
		QAction* action;
	};

	std::vector<DebuggerWidgetToggle> toggles;
	std::set<std::string> toggle_types;

	// Create a menu item for each open debugger widget.
	for (const auto& [unique_name, widget] : layout.debuggerWidgets())
	{
		QAction* action = new QAction(menu);
		action->setText(widget->displayName());
		action->setCheckable(true);
		action->setChecked(true);
		connect(action, &QAction::triggered, this, [this, unique_name]() {
			if (m_current_layout == DockLayout::INVALID_INDEX)
				return;

			m_layouts.at(m_current_layout).destroyDebuggerWidget(unique_name);
		});

		DebuggerWidgetToggle& toggle = toggles.emplace_back();
		toggle.display_name = widget->displayNameWithoutSuffix();
		toggle.suffix_number = widget->displayNameSuffixNumber();
		toggle.action = action;

		toggle_types.emplace(widget->metaObject()->className());
	}

	// Create menu items to open debugger widgets without any open instances.
	for (const auto& [type, desc] : DockTables::DEBUGGER_WIDGETS)
	{
		if (!toggle_types.contains(type))
		{
			QString display_name = QCoreApplication::translate("DebuggerWidget", desc.display_name);

			QAction* action = new QAction(menu);
			action->setText(display_name);
			action->setCheckable(true);
			action->setChecked(false);
			connect(action, &QAction::triggered, this, [this, type]() {
				if (m_current_layout == DockLayout::INVALID_INDEX)
					return;

				m_layouts.at(m_current_layout).createDebuggerWidget(type);
			});

			DebuggerWidgetToggle& toggle = toggles.emplace_back();
			toggle.display_name = display_name;
			toggle.suffix_number = std::nullopt;
			toggle.action = action;
		}
	}

	std::sort(toggles.begin(), toggles.end(),
		[](const DebuggerWidgetToggle& lhs, const DebuggerWidgetToggle& rhs) {
			if (lhs.display_name == rhs.display_name)
				return lhs.suffix_number < rhs.suffix_number;

			return lhs.display_name < rhs.display_name;
		});

	for (const DebuggerWidgetToggle& toggle : toggles)
		menu->addAction(toggle.action);
}

QWidget* DockManager::createLayoutSwitcher(QWidget* menu_bar)
{
	QWidget* container = new QWidget;

	QHBoxLayout* layout = new QHBoxLayout;
	layout->setContentsMargins(0, 2, 2, 0);
	container->setLayout(layout);

	QWidget* menu_wrapper = new QWidget;
	menu_wrapper->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
	layout->addWidget(menu_wrapper);

	QHBoxLayout* menu_layout = new QHBoxLayout;
	menu_layout->setContentsMargins(0, 4, 0, 4);
	menu_wrapper->setLayout(menu_layout);

	menu_layout->addWidget(menu_bar);

	m_switcher = new QTabBar;
	m_switcher->setContentsMargins(0, 0, 0, 0);
	m_switcher->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
	m_switcher->setContextMenuPolicy(Qt::CustomContextMenu);
	m_switcher->setMovable(true);
	layout->addWidget(m_switcher);

	updateLayoutSwitcher();

	connect(m_switcher, &QTabBar::tabMoved, this, &DockManager::layoutSwitcherTabMoved);
	connect(m_switcher, &QTabBar::customContextMenuRequested, this, &DockManager::layoutSwitcherContextMenu);

	QWidget* spacer = new QWidget;
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	layout->addWidget(spacer);

	bool layout_locked = Host::GetBaseBoolSettingValue("Debugger/UserInterface", "LayoutLocked", true);

	QPushButton* lock_layout_toggle = new QPushButton;
	lock_layout_toggle->setCheckable(true);
	lock_layout_toggle->setChecked(layout_locked);
	lock_layout_toggle->setFlat(true);
	connect(lock_layout_toggle, &QPushButton::toggled, this, [this, lock_layout_toggle](bool checked) {
		setLayoutLocked(checked, lock_layout_toggle, true);
	});
	layout->addWidget(lock_layout_toggle);

	setLayoutLocked(layout_locked, lock_layout_toggle, false);

	return container;
}

void DockManager::updateLayoutSwitcher()
{
	if (!m_switcher)
		return;

	disconnect(m_tab_connection);

	for (int i = m_switcher->count(); i > 0; i--)
		m_switcher->removeTab(i - 1);

	for (DockLayout& layout : m_layouts)
	{
		const char* cpu_name = DebugInterface::cpuName(layout.cpu());
		QString tab_name = QString("%1 (%2)").arg(layout.name()).arg(cpu_name);
		m_switcher->addTab(tab_name);
	}

	m_plus_tab_index = m_switcher->addTab("+");
	m_current_tab_index = m_current_layout;

	if (m_current_layout != DockLayout::INVALID_INDEX)
		m_switcher->setCurrentIndex(m_current_layout);

	// If we don't have any layouts, the currently selected tab will never be
	// changed, so we respond to all clicks instead.
	if (!m_layouts.empty())
		m_tab_connection = connect(m_switcher, &QTabBar::currentChanged, this, &DockManager::layoutSwitcherTabChanged);
	else
		m_tab_connection = connect(m_switcher, &QTabBar::tabBarClicked, this, &DockManager::layoutSwitcherTabChanged);

	layoutSwitcherStopBlink();
}

void DockManager::layoutSwitcherTabChanged(int index)
{
	// Prevent recursion.
	if (m_ignore_current_tab_changed)
		return;

	if (index == m_plus_tab_index)
	{
		if (m_current_tab_index >= 0 && m_current_tab_index < m_plus_tab_index)
		{
			m_ignore_current_tab_changed = true;
			m_switcher->setCurrentIndex(m_current_tab_index);
			m_ignore_current_tab_changed = false;
		}

		auto name_validator = [this](const QString& name) {
			return !hasNameConflict(name, DockLayout::INVALID_INDEX);
		};

		bool can_clone_current_layout = m_current_layout != DockLayout::INVALID_INDEX;

		QPointer<LayoutEditorDialog> dialog = new LayoutEditorDialog(
			name_validator, can_clone_current_layout, g_debugger_window);

		if (dialog->exec() == QDialog::Accepted && name_validator(dialog->name()))
		{
			DockLayout::Index new_layout = DockLayout::INVALID_INDEX;

			const auto [mode, index] = dialog->initialState();
			switch (mode)
			{
				case LayoutEditorDialog::DEFAULT_LAYOUT:
				{
					const DockTables::DefaultDockLayout& default_layout = DockTables::DEFAULT_DOCK_LAYOUTS.at(index);
					new_layout = createLayout(dialog->name(), dialog->cpu(), false, default_layout.name);
					break;
				}
				case LayoutEditorDialog::BLANK_LAYOUT:
				{
					new_layout = createLayout(dialog->name(), dialog->cpu(), false);
					break;
				}
				case LayoutEditorDialog::CLONE_LAYOUT:
				{
					if (m_current_layout == DockLayout::INVALID_INDEX)
						return;

					DockLayout::Index old_layout = m_current_layout;

					// Freeze the current layout so we can copy the geometry.
					switchToLayout(DockLayout::INVALID_INDEX);

					new_layout = createLayout(dialog->name(), dialog->cpu(), false, m_layouts.at(old_layout));
					break;
				}
			}

			updateLayoutSwitcher();
			switchToLayout(new_layout);
		}

		delete dialog.get();
	}
	else
	{
		DockLayout::Index layout_index = static_cast<DockLayout::Index>(index);
		if (layout_index < 0 || layout_index >= m_layouts.size())
			return;

		switchToLayout(layout_index);
		m_current_tab_index = index;
	}
}

void DockManager::layoutSwitcherTabMoved(int from, int to)
{
	DockLayout::Index from_index = static_cast<DockLayout::Index>(from);
	DockLayout::Index to_index = static_cast<DockLayout::Index>(to);

	if (from_index >= m_layouts.size() || to_index >= m_layouts.size())
	{
		// This happens when the user tries to move a layout to the right of the
		// plus button.
		updateLayoutSwitcher();
		return;
	}

	DockLayout& from_layout = m_layouts[from_index];
	DockLayout& to_layout = m_layouts[to_index];

	std::swap(from_layout, to_layout);

	from_layout.save(from_index);
	to_layout.save(to_index);

	if (from_index == m_current_layout)
		m_current_layout = to_index;
	else if (to_index == m_current_layout)
		m_current_layout = from_index;
}

void DockManager::layoutSwitcherContextMenu(QPoint pos)
{
	DockLayout::Index layout_index = static_cast<DockLayout::Index>(m_switcher->tabAt(pos));
	if (layout_index >= m_layouts.size())
		return;

	DockLayout& layout = m_layouts[layout_index];

	QMenu* menu = new QMenu(m_switcher);
	menu->setAttribute(Qt::WA_DeleteOnClose);

	QAction* edit_action = menu->addAction(tr("Edit Layout"));
	connect(edit_action, &QAction::triggered, [this, layout_index]() {
		if (layout_index >= m_layouts.size())
			return;

		DockLayout& layout = m_layouts[layout_index];

		auto name_validator = [this, layout_index](const QString& name) {
			return !hasNameConflict(name, layout_index);
		};

		QPointer<LayoutEditorDialog> dialog = new LayoutEditorDialog(
			layout.name(), layout.cpu(), name_validator, g_debugger_window);

		if (dialog->exec() != QDialog::Accepted || !name_validator(dialog->name()))
			return;

		layout.setName(dialog->name());
		layout.setCpu(dialog->cpu());

		layout.save(layout_index);

		delete dialog.get();

		updateLayoutSwitcher();
	});

	QAction* reset_action = menu->addAction(tr("Reset Layout"));
	reset_action->setEnabled(layout.canReset());
	reset_action->connect(reset_action, &QAction::triggered, [this, layout_index]() {
		if (layout_index >= m_layouts.size())
			return;

		DockLayout& layout = m_layouts[layout_index];
		if (!layout.canReset())
			return;

		QString text = tr("Are you sure you want to reset layout '%1'?").arg(layout.name());
		if (QMessageBox::question(g_debugger_window, tr("Confirmation"), text) != QMessageBox::Yes)
			return;

		bool current_layout = layout_index == m_current_layout;

		if (current_layout)
			switchToLayout(DockLayout::INVALID_INDEX);

		layout.reset();
		layout.save(layout_index);

		if (current_layout)
			switchToLayout(layout_index);
	});

	QAction* delete_action = menu->addAction(tr("Delete Layout"));
	connect(delete_action, &QAction::triggered, [this, layout_index]() {
		if (layout_index >= m_layouts.size())
			return;

		DockLayout& layout = m_layouts[layout_index];

		QString text = tr("Are you sure you want to delete layout '%1'?").arg(layout.name());
		if (QMessageBox::question(g_debugger_window, tr("Confirmation"), text) != QMessageBox::Yes)
			return;

		deleteLayout(layout_index);
		updateLayoutSwitcher();
	});

	menu->popup(m_switcher->mapToGlobal(pos));
}

void DockManager::layoutSwitcherStartBlink()
{
	if (!m_switcher)
		return;

	layoutSwitcherStopBlink();

	if (m_current_layout == DockLayout::INVALID_INDEX)
		return;

	m_blink_tab = m_current_layout;
	m_blink_stage = 0;
	m_blink_timer->start(500);

	layoutSwitcherUpdateBlink();
}

void DockManager::layoutSwitcherUpdateBlink()
{
	if (!m_switcher)
		return;

	if (m_blink_tab < m_switcher->count())
	{
		if (m_blink_stage % 2 == 0)
			m_switcher->setTabTextColor(m_blink_tab, Qt::red);
		else
			m_switcher->setTabTextColor(m_blink_tab, m_switcher->palette().text().color());
	}

	m_blink_stage++;

	if (m_blink_stage > 7)
		m_blink_timer->stop();
}

void DockManager::layoutSwitcherStopBlink()
{
	if (m_blink_timer->isActive())
	{
		if (m_blink_tab < m_switcher->count())
			m_switcher->setTabTextColor(m_blink_tab, m_switcher->palette().text().color());

		m_blink_timer->stop();
	}
}

bool DockManager::hasNameConflict(const QString& name, DockLayout::Index layout_index)
{
	std::string safe_name = Path::SanitizeFileName(name.toStdString());
	for (DockLayout::Index i = 0; i < m_layouts.size(); i++)
	{
		std::string other_safe_name = Path::SanitizeFileName(m_layouts[i].name().toStdString());
		if (i != layout_index && StringUtil::compareNoCase(other_safe_name, safe_name))
			return true;
	}

	return false;
}

void DockManager::updateDockWidgetTitles()
{
	if (m_current_layout == DockLayout::INVALID_INDEX)
		return;

	m_layouts.at(m_current_layout).updateDockWidgetTitles();
}

const std::map<QString, QPointer<DebuggerWidget>>& DockManager::debuggerWidgets()
{
	static std::map<QString, QPointer<DebuggerWidget>> dummy;
	if (m_current_layout == DockLayout::INVALID_INDEX)
		return dummy;

	return m_layouts.at(m_current_layout).debuggerWidgets();
}

size_t DockManager::countDebuggerWidgetsOfType(const char* type)
{
	if (m_current_layout == DockLayout::INVALID_INDEX)
		return 0;

	return m_layouts.at(m_current_layout).countDebuggerWidgetsOfType(type);
}

void DockManager::recreateDebuggerWidget(const QString& unique_name)
{
	if (m_current_layout == DockLayout::INVALID_INDEX)
		return;

	m_layouts.at(m_current_layout).recreateDebuggerWidget(unique_name);
}

void DockManager::destroyDebuggerWidget(const QString& unique_name)
{
	if (m_current_layout == DockLayout::INVALID_INDEX)
		return;

	m_layouts.at(m_current_layout).destroyDebuggerWidget(unique_name);
}

void DockManager::setPrimaryDebuggerWidget(DebuggerWidget* widget, bool is_primary)
{
	if (m_current_layout == DockLayout::INVALID_INDEX)
		return;

	m_layouts.at(m_current_layout).setPrimaryDebuggerWidget(widget, is_primary);
}

void DockManager::switchToDebuggerWidget(DebuggerWidget* widget)
{
	if (m_current_layout == DockLayout::INVALID_INDEX)
		return;

	for (const auto& [unique_name, test_widget] : m_layouts.at(m_current_layout).debuggerWidgets())
	{
		if (widget == test_widget)
		{
			auto [controller, view] = DockUtils::dockWidgetFromName(unique_name);
			controller->setAsCurrentTab();
			break;
		}
	}
}

void DockManager::updateStyleSheets()
{
	for (DockLayout& layout : m_layouts)
		for (const auto& [unique_name, widget] : layout.debuggerWidgets())
			widget->updateStyleSheet();
}

bool DockManager::isLayoutLocked()
{
	return m_layout_locked;
}

void DockManager::setLayoutLocked(bool locked, QPushButton* lock_layout_toggle, bool write_back)
{
	m_layout_locked = locked;

	if (lock_layout_toggle)
	{
		if (m_layout_locked)
		{
			lock_layout_toggle->setText(tr("Layout Locked"));
			lock_layout_toggle->setIcon(QIcon::fromTheme(QString::fromUtf8("padlock-lock")));
		}
		else
		{
			lock_layout_toggle->setText(tr("Layout Unlocked"));
			lock_layout_toggle->setIcon(QIcon::fromTheme(QString::fromUtf8("padlock-unlock")));
		}
	}

	updateToolBarLockState();

	for (KDDockWidgets::Core::Group* group : KDDockWidgets::DockRegistry::self()->groups())
	{
		auto stack = static_cast<KDDockWidgets::QtWidgets::Stack*>(group->stack()->view());
		stack->setTabsClosable(!m_layout_locked);

		// HACK: Make sure the sizes of the tabs get updated.
		if (stack->tabBar()->count() > 0)
			stack->tabBar()->setTabText(0, stack->tabBar()->tabText(0));
	}

	if (write_back)
	{
		Host::SetBaseBoolSettingValue("Debugger/UserInterface", "LayoutLocked", m_layout_locked);
		Host::CommitBaseSettingChanges();
	}
}

void DockManager::updateToolBarLockState()
{
	if (!g_debugger_window)
		return;

	for (QToolBar* toolbar : g_debugger_window->findChildren<QToolBar*>())
		toolbar->setMovable(!m_layout_locked || toolbar->isFloating());
}

std::optional<BreakPointCpu> DockManager::cpu()
{
	if (m_current_layout == DockLayout::INVALID_INDEX)
		return std::nullopt;

	return m_layouts.at(m_current_layout).cpu();
}

KDDockWidgets::Core::DockWidget* DockManager::dockWidgetFactory(const QString& name)
{
	if (!g_debugger_window)
		return nullptr;

	DockManager& manager = g_debugger_window->dockManager();
	if (manager.m_current_layout == DockLayout::INVALID_INDEX)
		return nullptr;

	return manager.m_layouts.at(manager.m_current_layout).createDockWidget(name);
}

bool DockManager::dragAboutToStart(KDDockWidgets::Core::Draggable* draggable)
{
	bool locked = true;
	if (g_debugger_window)
		locked = g_debugger_window->dockManager().isLayoutLocked();

	KDDockWidgets::Config::self().setDropIndicatorsInhibited(locked);

	if (draggable->isInProgrammaticDrag())
		return true;

	// Allow floating windows to be dragged around even if the layout is locked.
	if (draggable->isWindow())
		return true;

	if (!g_debugger_window)
		return false;

	return !locked;
}
