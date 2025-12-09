include(CopyBaseTranslations)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(pcsx2-qt)

target_sources(pcsx2-qt PRIVATE
	AboutDialog.cpp
	AboutDialog.h
	AboutDialog.ui
	AutoUpdaterDialog.cpp
	AutoUpdaterDialog.h
	AutoUpdaterDialog.ui
	ColorPickerButton.cpp
	ColorPickerButton.h
	CoverDownloadDialog.cpp
	CoverDownloadDialog.h
	CoverDownloadDialog.ui
	DisplayWidget.cpp
	DisplayWidget.h
	EarlyHardwareCheck.cpp
	LogWindow.cpp
	LogWindow.h
	MainWindow.cpp
	MainWindow.h
	MainWindow.ui
	PrecompiledHeader.cpp
	PrecompiledHeader.h
	SettingWidgetBinder.h
	SetupWizardDialog.cpp
	SetupWizardDialog.h
	SetupWizardDialog.ui
	Themes.cpp
	Translations.cpp
	QtHost.cpp
	QtHost.h
	QtKeyCodes.cpp
	QtProgressCallback.cpp
	QtProgressCallback.h
	QtUtils.cpp
	QtUtils.h
	GameList/EmptyGameListWidget.ui
	GameList/GameListModel.cpp
	GameList/GameListModel.h
	GameList/GameListRefreshThread.cpp
	GameList/GameListRefreshThread.h
	GameList/GameListWidget.cpp
	GameList/GameListWidget.h
	Settings/AchievementLoginDialog.cpp
	Settings/AchievementLoginDialog.h
	Settings/AchievementLoginDialog.ui
	Settings/AchievementSettingsWidget.cpp
	Settings/AchievementSettingsWidget.h
	Settings/AchievementSettingsWidget.ui
	Settings/AdvancedSettingsWidget.cpp
	Settings/AdvancedSettingsWidget.h
	Settings/AdvancedSettingsWidget.ui
	Settings/AudioExpansionSettingsDialog.ui
	Settings/AudioSettingsWidget.cpp
	Settings/AudioSettingsWidget.h
	Settings/AudioSettingsWidget.ui
	Settings/AudioStretchSettingsDialog.ui
	Settings/BIOSSettingsWidget.cpp
	Settings/BIOSSettingsWidget.h
	Settings/BIOSSettingsWidget.ui
	Settings/ControllerBindingWidget_DualShock2.ui
	Settings/ControllerBindingWidget_Guitar.ui
	Settings/ControllerBindingWidget_Popn.ui
	Settings/ControllerBindingWidget.cpp
	Settings/ControllerBindingWidget.h
	Settings/ControllerBindingWidget.ui
	Settings/ControllerLEDSettingsDialog.ui
	Settings/ControllerGlobalSettingsWidget.cpp
	Settings/ControllerGlobalSettingsWidget.h
	Settings/ControllerGlobalSettingsWidget.ui
	Settings/ControllerMacroEditWidget.ui
	Settings/ControllerMacroWidget.ui
	Settings/ControllerMappingSettingsDialog.ui
	Settings/ControllerMouseSettingsDialog.ui
	Settings/ControllerSettingsWindow.cpp
	Settings/ControllerSettingsWindow.h
	Settings/ControllerSettingsWindow.ui
	Settings/ControllerSettingWidgetBinder.h
	Settings/DebugAnalysisSettingsTab.ui
	Settings/DebugAnalysisSettingsWidget.cpp
	Settings/DebugAnalysisSettingsWidget.h
	Settings/DebugAnalysisSettingsWidget.ui
	Settings/DebugGSSettingsTab.ui
	Settings/DebugLoggingSettingsTab.ui
	Settings/DebugUserInterfaceSettingsTab.ui
	Settings/DebugSettingsWidget.cpp
	Settings/DebugSettingsWidget.h
	Settings/EmulationSettingsWidget.cpp
	Settings/EmulationSettingsWidget.h
	Settings/EmulationSettingsWidget.ui
	Settings/FolderSettingsWidget.cpp
	Settings/FolderSettingsWidget.h
	Settings/FolderSettingsWidget.ui
	Settings/GameCheatSettingsWidget.cpp
	Settings/GameCheatSettingsWidget.h
	Settings/GameCheatSettingsWidget.ui
	Settings/GameFixSettingsWidget.cpp
	Settings/GameFixSettingsWidget.h
	Settings/GameFixSettingsWidget.ui
	Settings/GameListSettingsWidget.cpp
	Settings/GameListSettingsWidget.h
	Settings/GameListSettingsWidget.ui
	Settings/GamePatchDetailsWidget.ui
	Settings/GamePatchSettingsWidget.cpp
	Settings/GamePatchSettingsWidget.h
	Settings/GamePatchSettingsWidget.ui
	Settings/GameSummaryWidget.cpp
	Settings/GameSummaryWidget.h
	Settings/GameSummaryWidget.ui
	Settings/GraphicsAdvancedSettingsTab.ui
	Settings/GraphicsDisplaySettingsTab.ui
	Settings/GraphicsHardwareFixesSettingsTab.ui
	Settings/GraphicsHardwareRenderingSettingsTab.ui
	Settings/GraphicsMediaCaptureSettingsTab.ui
	Settings/GraphicsOnScreenDisplaySettingsTab.ui
	Settings/GraphicsPostProcessingSettingsTab.ui
	Settings/GraphicsSettingsHeader.ui
	Settings/GraphicsSettingsWidget.cpp
	Settings/GraphicsSettingsWidget.h
	Settings/GraphicsSoftwareRenderingSettingsTab.ui
	Settings/GraphicsTextureReplacementSettingsTab.ui
	Settings/GraphicsUpscalingFixesSettingsTab.ui
	Settings/HotkeySettingsWidget.cpp
	Settings/HotkeySettingsWidget.h
	Settings/InputBindingDialog.cpp
	Settings/InputBindingDialog.h
	Settings/InputBindingDialog.ui
	Settings/InputBindingWidget.cpp
	Settings/InputBindingWidget.h
	Settings/InterfaceSettingsWidget.cpp
	Settings/InterfaceSettingsWidget.h
	Settings/InterfaceSettingsWidget.ui
	Settings/MemoryCardConvertDialog.cpp
	Settings/MemoryCardConvertDialog.h
	Settings/MemoryCardConvertDialog.ui
	Settings/MemoryCardConvertWorker.cpp
	Settings/MemoryCardConvertWorker.h
	Settings/MemoryCardCreateDialog.cpp
	Settings/MemoryCardCreateDialog.h
	Settings/MemoryCardCreateDialog.ui
	Settings/MemoryCardSettingsWidget.cpp
	Settings/MemoryCardSettingsWidget.h
	Settings/MemoryCardSettingsWidget.ui
	Settings/DEV9DnsHostDialog.cpp
	Settings/DEV9DnsHostDialog.h
	Settings/DEV9DnsHostDialog.ui
	Settings/DEV9SettingsWidget.cpp
	Settings/DEV9SettingsWidget.h
	Settings/DEV9SettingsWidget.ui
	Settings/DEV9UiCommon.cpp
	Settings/DEV9UiCommon.h
	Settings/HddCreateQt.cpp
	Settings/HddCreateQt.h
	Settings/PatchDetailsWidget.ui
	Settings/SettingsWidget.cpp
	Settings/SettingsWidget.h
	Settings/SettingsWindow.cpp
	Settings/SettingsWindow.h
	Settings/SettingsWindow.ui
	Settings/USBBindingWidget_DenshaCon.ui
	Settings/USBBindingWidget_DrivingForce.ui
	Settings/USBBindingWidget_GTForce.ui
	Settings/USBBindingWidget_GunCon2.ui
	Settings/USBBindingWidget_RyojouhenCon.ui
	Settings/USBBindingWidget_ShinkansenCon.ui
	Debugger/AnalysisOptionsDialog.cpp
	Debugger/AnalysisOptionsDialog.h
	Debugger/AnalysisOptionsDialog.ui
	Debugger/DebuggerSettingsManager.cpp
	Debugger/DebuggerSettingsManager.h
	Debugger/DebuggerEvents.h
	Debugger/DebuggerView.cpp
	Debugger/DebuggerView.h
	Debugger/DebuggerWindow.cpp
	Debugger/DebuggerWindow.h
	Debugger/DebuggerWindow.ui
	Debugger/DisassemblyView.cpp
	Debugger/DisassemblyView.h
	Debugger/DisassemblyView.ui
	Debugger/JsonValueWrapper.h
	Debugger/ModuleModel.cpp
	Debugger/ModuleModel.h
	Debugger/ModuleView.cpp
	Debugger/ModuleView.h
	Debugger/RegisterView.cpp
	Debugger/RegisterView.h
	Debugger/RegisterView.ui
	Debugger/StackModel.cpp
	Debugger/StackModel.h
	Debugger/StackView.cpp
	Debugger/StackView.h
	Debugger/ThreadModel.cpp
	Debugger/ThreadModel.h
	Debugger/ThreadView.cpp
	Debugger/ThreadView.h
	Debugger/Breakpoints/BreakpointDialog.cpp
	Debugger/Breakpoints/BreakpointDialog.h
	Debugger/Breakpoints/BreakpointDialog.ui
	Debugger/Breakpoints/BreakpointModel.cpp
	Debugger/Breakpoints/BreakpointModel.h
	Debugger/Breakpoints/BreakpointView.cpp
	Debugger/Breakpoints/BreakpointView.h
	Debugger/Breakpoints/BreakpointView.ui
	Debugger/Docking/DockLayout.cpp
	Debugger/Docking/DockLayout.h
	Debugger/Docking/DockManager.cpp
	Debugger/Docking/DockManager.h
	Debugger/Docking/DockMenuBar.cpp
	Debugger/Docking/DockMenuBar.h
	Debugger/Docking/DockTables.cpp
	Debugger/Docking/DockTables.h
	Debugger/Docking/DockUtils.cpp
	Debugger/Docking/DockUtils.h
	Debugger/Docking/DockViews.cpp
	Debugger/Docking/DockViews.h
	Debugger/Docking/DropIndicators.cpp
	Debugger/Docking/DropIndicators.h
	Debugger/Docking/LayoutEditorDialog.cpp
	Debugger/Docking/LayoutEditorDialog.h
	Debugger/Docking/LayoutEditorDialog.ui
	Debugger/Docking/NoLayoutsWidget.cpp
	Debugger/Docking/NoLayoutsWidget.h
	Debugger/Docking/NoLayoutsWidget.ui
	Debugger/Memory/MemorySearchView.cpp
	Debugger/Memory/MemorySearchView.h
	Debugger/Memory/MemorySearchView.ui
	Debugger/Memory/MemoryView.cpp
	Debugger/Memory/MemoryView.h
	Debugger/Memory/MemoryView.ui
	Debugger/Memory/SavedAddressesModel.cpp
	Debugger/Memory/SavedAddressesModel.h
	Debugger/Memory/SavedAddressesView.cpp
	Debugger/Memory/SavedAddressesView.h
	Debugger/Memory/SavedAddressesView.ui
	Debugger/SymbolTree/NewSymbolDialogs.cpp
	Debugger/SymbolTree/NewSymbolDialogs.h
	Debugger/SymbolTree/NewSymbolDialog.ui
	Debugger/SymbolTree/SymbolTreeLocation.cpp
	Debugger/SymbolTree/SymbolTreeLocation.h
	Debugger/SymbolTree/SymbolTreeModel.cpp
	Debugger/SymbolTree/SymbolTreeModel.h
	Debugger/SymbolTree/SymbolTreeNode.cpp
	Debugger/SymbolTree/SymbolTreeNode.h
	Debugger/SymbolTree/SymbolTreeDelegates.cpp
	Debugger/SymbolTree/SymbolTreeDelegates.h
	Debugger/SymbolTree/SymbolTreeViews.cpp
	Debugger/SymbolTree/SymbolTreeViews.h
	Debugger/SymbolTree/SymbolTreeView.ui
	Debugger/SymbolTree/TypeString.cpp
	Debugger/SymbolTree/TypeString.h
	Tools/InputRecording/NewInputRecordingDlg.cpp
	Tools/InputRecording/NewInputRecordingDlg.h
	Tools/InputRecording/NewInputRecordingDlg.ui
	Tools/InputRecording/InputRecordingViewer.cpp
	Tools/InputRecording/InputRecordingViewer.h
	Tools/InputRecording/InputRecordingViewer.ui
	resources/resources.qrc
)

if (NOT APPLE)
    target_sources(pcsx2-qt PRIVATE
        ShortcutCreationDialog.cpp
        ShortcutCreationDialog.h
        ShortcutCreationDialog.ui
    )
endif()

file(GLOB TS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/Translations/pcsx2-qt_*-*.ts)

target_precompile_headers(pcsx2-qt PRIVATE PrecompiledHeader.h)
set_source_files_properties(PrecompiledHeader.cpp PROPERTIES HEADER_FILE_ONLY TRUE)

target_include_directories(pcsx2-qt PRIVATE
	${Qt6Gui_PRIVATE_INCLUDE_DIRS}
	"${CMAKE_BINARY_DIR}/common/include"
	"${CMAKE_SOURCE_DIR}/pcsx2"
	"${CMAKE_SOURCE_DIR}/pcsx2-qt"
)

target_link_libraries(pcsx2-qt PRIVATE
	PCSX2_FLAGS
	PCSX2
	Qt6::Core
	Qt6::Gui
	Qt6::GuiPrivate
	Qt6::Widgets
	KDAB::kddockwidgets
)

# Our Qt builds may have exceptions on, so force them off.
target_compile_definitions(pcsx2-qt PRIVATE QT_NO_EXCEPTIONS)

if(WIN32)
	target_sources(pcsx2-qt PRIVATE VCRuntimeChecker.cpp)
	set_source_files_properties(${TS_FILES} PROPERTIES OUTPUT_LOCATION "${CMAKE_SOURCE_DIR}/bin/translations")
	qt_add_lrelease(pcsx2-qt TS_FILES ${TS_FILES})
	copy_base_translations(pcsx2-qt)
elseif(APPLE)
	qt_add_lrelease(pcsx2-qt TS_FILES ${TS_FILES} QM_FILES_OUTPUT_VARIABLE QM_FILES)
	set(PCSX2_MACOS_LOCALIZATIONS)
	foreach (TS_FILE IN LISTS TS_FILES)
		get_filename_component(TS_FILE_NAME ${TS_FILE} NAME)
		set(regex "^pcsx2-qt_([a-zA-Z0-9\\-]+)\\.ts$")
		if (TS_FILE_NAME MATCHES ${regex})
			string(REGEX REPLACE ${regex} "\\1" language ${TS_FILE_NAME})
			string(REPLACE "-" "_" language ${language})
			set(PCSX2_MACOS_LOCALIZATIONS "${PCSX2_MACOS_LOCALIZATIONS}\n\t\t<string>${language}</string>")
		else()
			message(WARNING "Unrecognized ts file ${TS_FILE_NAME}")
		endif()
	endforeach()
	foreach (QM_FILE IN LISTS QM_FILES)
		target_sources(pcsx2-qt PRIVATE ${QM_FILE})
		set_source_files_properties(${QM_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/translations/)
	endforeach()
	copy_base_translations(pcsx2-qt)
	extract_translation_from_ts(Translations/pcsx2-qt_en.ts source PermissionsDialogMicrophone PCSX2_MICROPHONE_USAGE_DESCRIPTION)
else()
	qt_add_lrelease(pcsx2-qt TS_FILES ${TS_FILES} QM_FILES_OUTPUT_VARIABLE QM_FILES)
	set(QM_OUTPUT_DIR "$<TARGET_FILE_DIR:pcsx2-qt>/translations")
	add_custom_command(TARGET pcsx2-qt POST_BUILD COMMAND "${CMAKE_COMMAND}" -E make_directory "${QM_OUTPUT_DIR}")
	foreach (QM_FILE IN LISTS QM_FILES)
		get_filename_component(QM_FILE_NAME ${QM_FILE} NAME)
		add_custom_command(TARGET pcsx2-qt POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${QM_FILE}" "${QM_OUTPUT_DIR}/${QM_FILE_NAME}")
	endforeach()
	copy_base_translations(pcsx2-qt)
endif()


# Currently, 7z is only needed for the Windows updater.
if(WIN32)
	target_link_libraries(pcsx2-qt PRIVATE
		LZMA::LZMA
	)
endif()

fixup_file_properties(pcsx2-qt)
setup_main_executable(pcsx2-qt)
