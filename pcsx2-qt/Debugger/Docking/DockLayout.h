// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Debugger/Docking/DockTables.h"

#include "DebugTools/DebugInterface.h"

#include <kddockwidgets/MainWindow.h>
#include <kddockwidgets/DockWidget.h>

#include <QtCore/QPointer>

class DebuggerWidget;
class DebuggerWindow;

extern const char* DEBUGGER_LAYOUT_FILE_FORMAT;

// Increment this whenever the JSON format changes (excluding the contents of
// the geometry object which is managed by KDDockWidgets).
extern const u32 DEBUGGER_LAYOUT_FILE_VERSION_MAJOR;

// Increment this whenever you want all the default layouts to be reset. If you
// modify the DockTables::DEFAULT_DOCK_LAYOUTS table you won't need to increment
// this as they will be reset automatically.
extern const u32 DEBUGGER_LAYOUT_FILE_VERSION_MINOR;

class DockLayout
{
public:
	using Index = size_t;
	static const constexpr Index INVALID_INDEX = SIZE_MAX;

	enum LoadResult
	{
		SUCCESS,
		FILE_NOT_FOUND,
		INVALID_FORMAT,
		MAJOR_VERSION_MISMATCH,
		MINOR_VERSION_MISMATCH,
		CONFLICTING_NAME
	};

	// Create a layout based on a default layout.
	DockLayout(
		std::string name,
		BreakPointCpu cpu,
		bool is_default,
		const DockTables::DefaultDockLayout& default_layout,
		DockLayout::Index index);

	// Create a new blank layout.
	DockLayout(
		std::string name,
		BreakPointCpu cpu,
		bool is_default,
		DockLayout::Index index);

	// Clone an existing layout.
	DockLayout(
		std::string name,
		BreakPointCpu cpu,
		bool is_default,
		const DockLayout& layout_to_clone,
		DockLayout::Index index);

	// Load a layout from a file.
	DockLayout(
		const std::string& path,
		LoadResult& result,
		DockLayout::Index& index_last_session,
		DockLayout::Index index);

	~DockLayout();

	DockLayout(const DockLayout& rhs) = delete;
	DockLayout& operator=(const DockLayout& rhs) = delete;

	DockLayout(DockLayout&& rhs) = default;
	DockLayout& operator=(DockLayout&&) = default;

	const std::string& name() const;
	void setName(std::string name);

	BreakPointCpu cpu() const;
	void setCpu(BreakPointCpu cpu);

	bool isDefault() const;

	// Tear down and save the state of all the dock widgets from this layout.
	void freeze();

	// Restore the state of all the dock widgets from this layout.
	void thaw();

	KDDockWidgets::Core::DockWidget* createDockWidget(const QString& name);
	void retranslateDockWidgets();
	void retranslateDockWidget(KDDockWidgets::Core::DockWidget* dock_widget);
	void dockWidgetClosed(KDDockWidgets::Core::DockWidget* dock_widget);

	const std::map<QString, QPointer<DebuggerWidget>>& debuggerWidgets();
	bool hasDebuggerWidget(QString unique_name);
	void toggleDebuggerWidget(QString unique_name);
	void recreateDebuggerWidget(QString unique_name);

	void deleteFile();

	bool save(DockLayout::Index layout_index);

private:
	void load(
		const std::string& path,
		DockLayout::LoadResult& result,
		DockLayout::Index& index_last_session);

	void setupDefaultLayout();

	// The name displayed in the user interface. Also used to determine the
	// file name for the layout file.
	std::string m_name;

	// The default target for dock widgets in this layout. This can be
	// overriden on a per-widget basis.
	BreakPointCpu m_cpu;

	// Is this one of the default layouts?
	bool m_is_default = false;

	// The name of the default layout which this layout was based on. This will
	// be used if the m_geometry variable above is empty.
	std::string m_base_layout;

	// The state of all the toolbars, saved and restored using
	// QMainWindow::saveState and QMainWindow::restoreState respectively.
	QByteArray m_toolbars;

	// All the dock widgets currently open in this layout. If this is the active
	// layout then these will be owned by the docking system, otherwise they
	// won't be and will need to be cleaned up separately.
	std::map<QString, QPointer<DebuggerWidget>> m_widgets;

	// The geometry of all the dock widgets, converted to JSON by the
	// LayoutSaver class from KDDockWidgets.
	QByteArray m_geometry;

	// The absolute file path of the corresponding layout file as it currently
	// exists exists on disk, or empty if no such file exists.
	std::string m_layout_file_path;

	// If this layout is the currently selected layout this will be false,
	// otherwise it will be true.
	bool m_is_frozen = true;
};
