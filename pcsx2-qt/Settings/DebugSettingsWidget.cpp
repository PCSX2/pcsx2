// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DebugSettingsWidget.h"

#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "Debugger/DebuggerWindow.h"
#include "Settings/DebugAnalysisSettingsWidget.h"
#include "Settings/SettingsWindow.h"

#include "pcsx2/Host.h"

#include <QtWidgets/QMessageBox>

static const char* s_drop_indicators[] = {
	QT_TRANSLATE_NOOP("DebugSettingsWidget", "Classic"),
	QT_TRANSLATE_NOOP("DebugSettingsWidget", "Segmented"),
	QT_TRANSLATE_NOOP("DebugSettingsWidget", "Minimalistic"),
	nullptr,
};

DebugSettingsWidget::DebugSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent)
	: SettingsWidget(settings_dialog, parent)
{
	SettingsInterface* sif = dialog()->getSettingsInterface();

	m_user_interface_tab = setupTab(m_user_interface, tr("User Interface"));
	setupTab(m_analysis, tr("Analysis"));
	setupTab(m_gs, tr("GS"));
	m_logging_tab = setupTab(m_logging, tr("Logging"));

	//////////////////////////////////////////////////////////////////////////
	// User Interface Settings
	//////////////////////////////////////////////////////////////////////////
	if (!dialog()->isPerGameSettings())
	{
		SettingWidgetBinder::BindWidgetToIntSetting(
			sif, m_user_interface.refreshInterval, "Debugger/UserInterface", "RefreshInterval", 1000);
		connect(m_user_interface.refreshInterval, &QSpinBox::valueChanged, this, []() {
			if (g_debugger_window)
				g_debugger_window->updateFromSettings();
		});
		dialog()->registerWidgetHelp(
			m_user_interface.refreshInterval, tr("Refresh Interval"), tr("1000ms"),
			tr("The amount of time to wait between subsequent attempts to update the user interface to reflect the state "
			   "of the virtual machine."));

		SettingWidgetBinder::BindWidgetToBoolSetting(
			sif, m_user_interface.showOnStartup, "Debugger/UserInterface", "ShowOnStartup", false);
		dialog()->registerWidgetHelp(
			m_user_interface.showOnStartup, tr("Show On Startup"), tr("Unchecked"),
			tr("Open the debugger window automatically when PCSX2 starts."));

		SettingWidgetBinder::BindWidgetToBoolSetting(
			sif, m_user_interface.saveWindowGeometry, "Debugger/UserInterface", "SaveWindowGeometry", true);
		dialog()->registerWidgetHelp(
			m_user_interface.saveWindowGeometry, tr("Save Window Geometry"), tr("Checked"),
			tr("Save the position and size of the debugger window when it is closed so that it can be restored later."));

		SettingWidgetBinder::BindWidgetToEnumSetting(
			sif,
			m_user_interface.dropIndicator,
			"Debugger/UserInterface",
			"DropIndicatorStyle",
			s_drop_indicators,
			s_drop_indicators,
			s_drop_indicators[0],
			"DebugUserInterfaceSettingsWidget");
		dialog()->registerWidgetHelp(
			m_user_interface.dropIndicator, tr("Drop Indicator Style"), tr("Classic"),
			tr("Choose how the drop indicators that appear when you drag dock windows in the debugger are styled. "
			   "You will have to restart the debugger for this option to take effect."));
	}
	else
	{
		setTabVisible(m_user_interface_tab, false);
	}

	//////////////////////////////////////////////////////////////////////////
	// Analysis Settings
	//////////////////////////////////////////////////////////////////////////

	SettingWidgetBinder::BindWidgetToEnumSetting(
		sif, m_analysis.analysisCondition, "Debugger/Analysis", "RunCondition",
		Pcsx2Config::DebugAnalysisOptions::RunConditionNames, DebugAnalysisCondition::IF_DEBUGGER_IS_OPEN);
	SettingWidgetBinder::BindWidgetToBoolSetting(
		sif, m_analysis.generateSymbolsForIRXExportTables, "Debugger/Analysis", "GenerateSymbolsForIRXExports", true);

	dialog()->registerWidgetHelp(m_analysis.analysisCondition, tr("Analyze Program"), tr("If Debugger Is Open"),
		tr("Choose when the analysis passes should be run: Always (to save time when opening the debugger), If "
		   "Debugger Is Open (to save memory if you never open the debugger), or Never."));
	dialog()->registerWidgetHelp(m_analysis.generateSymbolsForIRXExportTables, tr("Generate Symbols for IRX Export Tables"), tr("Checked"),
		tr("Hook IRX module loading/unloading and generate symbols for exported functions on the fly."));

	m_analysis_settings = new DebugAnalysisSettingsWidget(dialog());

	m_analysis.analysisSettings->setLayout(new QVBoxLayout());
	m_analysis.analysisSettings->layout()->setContentsMargins(0, 0, 0, 0);
	m_analysis.analysisSettings->layout()->addWidget(m_analysis_settings);

	//////////////////////////////////////////////////////////////////////////
	// GS Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_gs.dumpGSData, "EmuCore/GS", "DumpGSData", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_gs.saveRT, "EmuCore/GS", "SaveRT", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_gs.saveFrame, "EmuCore/GS", "SaveFrame", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_gs.saveTexture, "EmuCore/GS", "SaveTexture", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_gs.saveDepth, "EmuCore/GS", "SaveDepth", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_gs.saveAlpha, "EmuCore/GS", "SaveAlpha", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_gs.saveInfo, "EmuCore/GS", "SaveInfo", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_gs.saveTransferImages, "EmuCore/GS", "SaveTransferImages", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_gs.saveDrawStats, "EmuCore/GS", "SaveDrawStats", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_gs.saveFrameStats, "EmuCore/GS", "SaveFrameStats", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_gs.saveHWConfig, "EmuCore/GS", "SaveHWConfig", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_gs.saveDrawStart, "EmuCore/GS", "SaveDrawStart", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_gs.saveDrawCount, "EmuCore/GS", "SaveDrawCount", 5000);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_gs.saveFrameStart, "EmuCore/GS", "SaveFrameStart", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_gs.saveFrameCount, "EmuCore/GS", "SaveFrameCount", 999999);
	SettingWidgetBinder::BindWidgetToFolderSetting(
		sif, m_gs.hwDumpDirectory, m_gs.hwDumpBrowse, m_gs.hwDumpOpen, nullptr, "EmuCore/GS", "HWDumpDirectory", std::string(), false);
	SettingWidgetBinder::BindWidgetToFolderSetting(
		sif, m_gs.swDumpDirectory, m_gs.swDumpBrowse, m_gs.swDumpOpen, nullptr, "EmuCore/GS", "SWDumpDirectory", std::string(), false);

	connect(m_gs.dumpGSData, &QCheckBox::checkStateChanged, this, &DebugSettingsWidget::onDrawDumpingChanged);
	onDrawDumpingChanged();

#ifdef PCSX2_DEVBUILD
	//////////////////////////////////////////////////////////////////////////
	// Logging Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEnable, "EmuCore/TraceLog", "Enabled", false);
	dialog()->registerWidgetHelp(m_logging.chkEnable, tr("Enable Trace Logging"), tr("Unchecked"), tr("Globally enable / disable trace logging."));

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEEBIOS, "EmuCore/TraceLog", "EE.bios", false);
	dialog()->registerWidgetHelp(m_logging.chkEEBIOS, tr("EE BIOS"), tr("Unchecked"), tr("Log SYSCALL and DECI2 activity."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEEMemory, "EmuCore/TraceLog", "EE.memory", false);
	dialog()->registerWidgetHelp(m_logging.chkEEMemory, tr("EE Memory"), tr("Unchecked"), tr("Log memory access to unknown or unmapped EE memory."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEER5900, "EmuCore/TraceLog", "EE.r5900", false);
	dialog()->registerWidgetHelp(m_logging.chkEER5900, tr("EE R5900"), tr("Unchecked"), tr("Log R5900 core instructions (excluding COPs). Requires modifying the PCSX2 source and enabling the interpreter."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEECOP0, "EmuCore/TraceLog", "EE.cop0", false);
	dialog()->registerWidgetHelp(m_logging.chkEECOP0, tr("EE COP0"), tr("Unchecked"), tr("Log COP0 (MMU, CPU status, etc) instructions."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEECOP1, "EmuCore/TraceLog", "EE.cop1", false);
	dialog()->registerWidgetHelp(m_logging.chkEECOP1, tr("EE COP1"), tr("Unchecked"), tr("Log COP1 (FPU) instructions."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEECOP2, "EmuCore/TraceLog", "EE.cop2", false);
	dialog()->registerWidgetHelp(m_logging.chkEECOP2, tr("EE COP2"), tr("Unchecked"), tr("Log COP2 (VU0 Macro mode) instructions."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEECache, "EmuCore/TraceLog", "EE.cache", false);
	dialog()->registerWidgetHelp(m_logging.chkEECache, tr("EE Cache"), tr("Unchecked"), tr("Log EE cache activity."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEEMMIO, "EmuCore/TraceLog", "EE.knownhw", false);
	dialog()->registerWidgetHelp(m_logging.chkEEMMIO, tr("EE Known MMIO"), tr("Unchecked"), tr("Log known MMIO accesses."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEEUNKNWNMMIO, "EmuCore/TraceLog", "EE.unknownhw", false);
	dialog()->registerWidgetHelp(m_logging.chkEEUNKNWNMMIO, tr("EE Unknown MMIO"), tr("Unchecked"), tr("Log unknown or unimplemented MMIO accesses."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEEDMARegs, "EmuCore/TraceLog", "EE.dmahw", false);
	dialog()->registerWidgetHelp(m_logging.chkEEDMARegs, tr("EE DMA Registers"), tr("Unchecked"), tr("Log DMA-related MMIO accesses."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEEIPU, "EmuCore/TraceLog", "EE.ipu", false);
	dialog()->registerWidgetHelp(m_logging.chkEEIPU, tr("EE IPU"), tr("Unchecked"), tr("Log IPU activity; MMIO, decoding operations, DMA status, etc."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEEGIFTags, "EmuCore/TraceLog", "EE.giftag", false);
	dialog()->registerWidgetHelp(m_logging.chkEEGIFTags, tr("EE GIF Tags"), tr("Unchecked"), tr("Log GIFtag parsing activity."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEEVIFCodes, "EmuCore/TraceLog", "EE.vifcode", false);
	dialog()->registerWidgetHelp(m_logging.chkEEVIFCodes, tr("EE VIF Codes"), tr("Unchecked"), tr("Log VIFcode processing; command, tag style, interrupts."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEEMSKPATH3, "EmuCore/TraceLog", "EE.mskpath3", false);
	dialog()->registerWidgetHelp(m_logging.chkEEMSKPATH3, tr("EE MSKPATH3"), tr("Unchecked"), tr("Log Path3 Masking processing."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEEMFIFO, "EmuCore/TraceLog", "EE.spr", false);
	dialog()->registerWidgetHelp(m_logging.chkEEMFIFO, tr("EE MFIFO"), tr("Unchecked"), tr("Log Scratchpad MFIFO activity."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEEDMACTRL, "EmuCore/TraceLog", "EE.dmac", false);
	dialog()->registerWidgetHelp(m_logging.chkEEDMACTRL, tr("EE DMA Controller"), tr("Unchecked"), tr("Log DMA transfer activity. Stalls, bus right arbitration, etc."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEECounters, "EmuCore/TraceLog", "EE.counters", false);
	dialog()->registerWidgetHelp(m_logging.chkEECounters, tr("EE Counters"), tr("Unchecked"), tr("Log all EE counters events and some counter register activity."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEEVIF, "EmuCore/TraceLog", "EE.vif", false);
	dialog()->registerWidgetHelp(m_logging.chkEEVIF, tr("EE VIF"), tr("Unchecked"), tr("Log various VIF and VIFcode processing data."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEEGIF, "EmuCore/TraceLog", "EE.gif", false);
	dialog()->registerWidgetHelp(m_logging.chkEEGIF, tr("EE GIF"), tr("Unchecked"), tr("Log various GIF and GIFtag parsing data."));

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkIOPBIOS, "EmuCore/TraceLog", "IOP.Bios", false);
	dialog()->registerWidgetHelp(m_logging.chkIOPBIOS, tr("IOP BIOS"), tr("Unchecked"), tr("Log SYSCALL and IRX activity."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkIOPMemcards, "EmuCore/TraceLog", "IOP.memcards", false);
	dialog()->registerWidgetHelp(m_logging.chkIOPMemcards, tr("IOP Memcards"), tr("Unchecked"), tr("Log memory card activity. Reads, Writes, erases, etc."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkIOPR3000A, "EmuCore/TraceLog", "IOP.r3000a", false);
	dialog()->registerWidgetHelp(m_logging.chkIOPR3000A, tr("IOP R3000A"), tr("Unchecked"), tr("Log R3000A core instructions (excluding COPs)."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkIOPCOP2, "EmuCore/TraceLog", "IOP.cop2", false);
	dialog()->registerWidgetHelp(m_logging.chkIOPCOP2, tr("IOP COP2"), tr("Unchecked"), tr("Log IOP GPU co-processor instructions."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkIOPMMIO, "EmuCore/TraceLog", "IOP.knownhw", false);
	dialog()->registerWidgetHelp(m_logging.chkIOPMMIO, tr("IOP Known MMIO"), tr("Unchecked"), tr("Log known MMIO accesses."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkIOPUNKNWNMMIO, "EmuCore/TraceLog", "IOP.unknownhw", false);
	dialog()->registerWidgetHelp(m_logging.chkIOPUNKNWNMMIO, tr("IOP Unknown MMIO"), tr("Unchecked"), tr("Log unknown or unimplemented MMIO accesses."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkIOPDMARegs, "EmuCore/TraceLog", "IOP.dmahw", false);
	dialog()->registerWidgetHelp(m_logging.chkIOPDMARegs, tr("IOP DMA Registers"), tr("Unchecked"), tr("Log DMA-related MMIO accesses."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkIOPPad, "EmuCore/TraceLog", "IOP.pad", false);
	dialog()->registerWidgetHelp(m_logging.chkIOPPad, tr("IOP PAD"), tr("Unchecked"), tr("Log PAD activity."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkIOPDMACTRL, "EmuCore/TraceLog", "IOP.dmac", false);
	dialog()->registerWidgetHelp(m_logging.chkIOPDMACTRL, tr("IOP DMA Controller"), tr("Unchecked"), tr("Log DMA transfer activity. Stalls, bus right arbitration, etc."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkIOPCounters, "EmuCore/TraceLog", "IOP.counters", false);
	dialog()->registerWidgetHelp(m_logging.chkIOPCounters, tr("IOP Counters"), tr("Unchecked"), tr("Log all IOP counters events and some counter register activity."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkIOPCDVD, "EmuCore/TraceLog", "IOP.cdvd", false);
	dialog()->registerWidgetHelp(m_logging.chkIOPCDVD, tr("IOP CDVD"), tr("Unchecked"), tr("Log CDVD hardware activity."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkIOPMDEC, "EmuCore/TraceLog", "IOP.mdec", false);
	dialog()->registerWidgetHelp(m_logging.chkIOPMDEC, tr("IOP MDEC"), tr("Unchecked"), tr("Log Motion (FMV) Decoder hardware unit activity."));

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_logging.chkEESIF, "EmuCore/TraceLog", "MISC.sif", false);
	dialog()->registerWidgetHelp(m_logging.chkEESIF, tr("EE SIF"), tr("Unchecked"), tr("Log SIF (EE <-> IOP) activity."));

	connect(m_logging.chkEnable, &QCheckBox::checkStateChanged, this, &DebugSettingsWidget::onLoggingEnableChanged);
	onLoggingEnableChanged();
#else
	setTabVisible(m_logging_tab, false);
#endif
}

DebugSettingsWidget::~DebugSettingsWidget() = default;

void DebugSettingsWidget::onDrawDumpingChanged()
{
	const bool enabled = dialog()->getEffectiveBoolValue("EmuCore/GS", "DumpGSData", false);
	m_gs.saveRT->setEnabled(enabled);
	m_gs.saveFrame->setEnabled(enabled);
	m_gs.saveTexture->setEnabled(enabled);
	m_gs.saveDepth->setEnabled(enabled);
	m_gs.saveAlpha->setEnabled(enabled);
	m_gs.saveInfo->setEnabled(enabled);
	m_gs.saveTransferImages->setEnabled(enabled);
	m_gs.saveDrawStats->setEnabled(enabled);
	m_gs.saveFrameStats->setEnabled(enabled);
	m_gs.saveHWConfig->setEnabled(enabled);
	m_gs.saveDrawStart->setEnabled(enabled);
	m_gs.saveDrawCount->setEnabled(enabled);
	m_gs.saveFrameStart->setEnabled(enabled);
	m_gs.saveFrameCount->setEnabled(enabled);
	m_gs.hwDumpDirectory->setEnabled(enabled);
	m_gs.hwDumpBrowse->setEnabled(enabled);
	m_gs.hwDumpOpen->setEnabled(enabled);
	m_gs.swDumpDirectory->setEnabled(enabled);
	m_gs.swDumpBrowse->setEnabled(enabled);
	m_gs.swDumpOpen->setEnabled(enabled);
}

#ifdef PCSX2_DEVBUILD
void DebugSettingsWidget::onLoggingEnableChanged()
{
	const bool enabled = dialog()->getEffectiveBoolValue("EmuCore/TraceLog", "Enabled", false);

	m_logging.chkEEBIOS->setEnabled(enabled);
	m_logging.chkEEMemory->setEnabled(enabled);
	m_logging.chkEER5900->setEnabled(enabled);
	m_logging.chkEECOP0->setEnabled(enabled);
	m_logging.chkEECOP1->setEnabled(enabled);
	m_logging.chkEECOP2->setEnabled(enabled);
	m_logging.chkEECache->setEnabled(enabled);
	m_logging.chkEEMMIO->setEnabled(enabled);
	m_logging.chkEEUNKNWNMMIO->setEnabled(enabled);
	m_logging.chkEEDMARegs->setEnabled(enabled);
	m_logging.chkEEIPU->setEnabled(enabled);
	m_logging.chkEEGIFTags->setEnabled(enabled);
	m_logging.chkEEVIFCodes->setEnabled(enabled);
	m_logging.chkEEMSKPATH3->setEnabled(enabled);
	m_logging.chkEEMFIFO->setEnabled(enabled);
	m_logging.chkEEDMACTRL->setEnabled(enabled);
	m_logging.chkEECounters->setEnabled(enabled);
	m_logging.chkEEVIF->setEnabled(enabled);
	m_logging.chkEEGIF->setEnabled(enabled);
	m_logging.chkEESIF->setEnabled(enabled);

	m_logging.chkIOPBIOS->setEnabled(enabled);
	m_logging.chkIOPMemcards->setEnabled(enabled);
	m_logging.chkIOPR3000A->setEnabled(enabled);
	m_logging.chkIOPCOP2->setEnabled(enabled);
	m_logging.chkIOPMMIO->setEnabled(enabled);
	m_logging.chkIOPUNKNWNMMIO->setEnabled(enabled);
	m_logging.chkIOPDMARegs->setEnabled(enabled);
	m_logging.chkIOPMemcards->setEnabled(enabled);
	m_logging.chkIOPPad->setEnabled(enabled);
	m_logging.chkIOPDMACTRL->setEnabled(enabled);
	m_logging.chkIOPCounters->setEnabled(enabled);
	m_logging.chkIOPCDVD->setEnabled(enabled);
	m_logging.chkIOPMDEC->setEnabled(enabled);

	g_emu_thread->applySettings();
}
#endif
