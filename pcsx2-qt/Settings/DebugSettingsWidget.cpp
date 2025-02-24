// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DebugSettingsWidget.h"

#include "DebugAnalysisSettingsWidget.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"

#include "pcsx2/Host.h"

#include <QtWidgets/QMessageBox>

DebugSettingsWidget::DebugSettingsWidget(SettingsWindow* dialog, QWidget* parent)
	: QWidget(parent)
	, m_dialog(dialog)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	//////////////////////////////////////////////////////////////////////////
	// Analysis Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToEnumSetting(
		sif, m_ui.analysisCondition, "Debugger/Analysis", "RunCondition",
		Pcsx2Config::DebugAnalysisOptions::RunConditionNames, DebugAnalysisCondition::IF_DEBUGGER_IS_OPEN);
	SettingWidgetBinder::BindWidgetToBoolSetting(
		sif, m_ui.generateSymbolsForIRXExportTables, "Debugger/Analysis", "GenerateSymbolsForIRXExports", true);

	dialog->registerWidgetHelp(m_ui.analysisCondition, tr("Analyze Program"), tr("If Debugger Is Open"),
		tr("Choose when the analysis passes should be run: Always (to save time when opening the debugger), If "
		   "Debugger Is Open (to save memory if you never open the debugger), or Never."));
	dialog->registerWidgetHelp(m_ui.generateSymbolsForIRXExportTables, tr("Generate Symbols for IRX Export Tables"), tr("Checked"),
		tr("Hook IRX module loading/unloading and generate symbols for exported functions on the fly."));

	m_analysis_settings = new DebugAnalysisSettingsWidget(dialog);

	m_ui.analysisSettings->setLayout(new QVBoxLayout());
	m_ui.analysisSettings->layout()->setContentsMargins(0, 0, 0, 0);
	m_ui.analysisSettings->layout()->addWidget(m_analysis_settings);

	//////////////////////////////////////////////////////////////////////////
	// GS Settings
	//////////////////////////////////////////////////////////////////////////
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.dumpGSData, "EmuCore/GS", "DumpGSData", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.saveRT, "EmuCore/GS", "SaveRT", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.saveFrame, "EmuCore/GS", "SaveFrame", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.saveTexture, "EmuCore/GS", "SaveTexture", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.saveDepth, "EmuCore/GS", "SaveDepth", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.saveAlpha, "EmuCore/GS", "SaveAlpha", false);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.saveInfo, "EmuCore/GS", "SaveInfo", false);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.saveDrawStart, "EmuCore/GS", "SaveDrawStart", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.saveDrawCount, "EmuCore/GS", "SaveDrawCount", 5000);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.saveFrameStart, "EmuCore/GS", "SaveFrameStart", 0);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.saveFrameCount, "EmuCore/GS", "SaveFrameCount", 999999);
	SettingWidgetBinder::BindWidgetToFolderSetting(
		sif, m_ui.hwDumpDirectory, m_ui.hwDumpBrowse, m_ui.hwDumpOpen, nullptr, "EmuCore/GS", "HWDumpDirectory", std::string(), false);
	SettingWidgetBinder::BindWidgetToFolderSetting(
		sif, m_ui.swDumpDirectory, m_ui.swDumpBrowse, m_ui.swDumpOpen, nullptr, "EmuCore/GS", "SWDumpDirectory", std::string(), false);

	connect(m_ui.dumpGSData, &QCheckBox::checkStateChanged, this, &DebugSettingsWidget::onDrawDumpingChanged);
	onDrawDumpingChanged();

#ifdef PCSX2_DEVBUILD
	//////////////////////////////////////////////////////////////////////////
	// Trace Logging Settings
	//////////////////////////////////////////////////////////////////////////

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEnable, "EmuCore/TraceLog", "Enabled", false);
	dialog->registerWidgetHelp(m_ui.chkEnable, tr("Enable Trace Logging"), tr("Unchecked"), tr("Globally enable / disable trace logging."));

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEEBIOS, "EmuCore/TraceLog", "EE.bios", false);
	dialog->registerWidgetHelp(m_ui.chkEEBIOS, tr("EE BIOS"), tr("Unchecked"), tr("Log SYSCALL and DECI2 activity."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEEMemory, "EmuCore/TraceLog", "EE.memory", false);
	dialog->registerWidgetHelp(m_ui.chkEEMemory, tr("EE Memory"), tr("Unchecked"), tr("Log memory access to unknown or unmapped EE memory."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEER5900, "EmuCore/TraceLog", "EE.r5900", false);
	dialog->registerWidgetHelp(m_ui.chkEER5900, tr("EE R5900"), tr("Unchecked"), tr("Log R5900 core instructions (excluding COPs). Requires modifying the PCSX2 source and enabling the interpreter."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEECOP0, "EmuCore/TraceLog", "EE.cop0", false);
	dialog->registerWidgetHelp(m_ui.chkEECOP0, tr("EE COP0"), tr("Unchecked"), tr("Log COP0 (MMU, CPU status, etc) instructions."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEECOP1, "EmuCore/TraceLog", "EE.cop1", false);
	dialog->registerWidgetHelp(m_ui.chkEECOP1, tr("EE COP1"), tr("Unchecked"), tr("Log COP1 (FPU) instructions."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEECOP2, "EmuCore/TraceLog", "EE.cop2", false);
	dialog->registerWidgetHelp(m_ui.chkEECOP2, tr("EE COP2"), tr("Unchecked"), tr("Log COP2 (VU0 Macro mode) instructions."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEECache, "EmuCore/TraceLog", "EE.cache", false);
	dialog->registerWidgetHelp(m_ui.chkEECache, tr("EE Cache"), tr("Unchecked"), tr("Log EE cache activity."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEEMMIO, "EmuCore/TraceLog", "EE.knownhw", false);
	dialog->registerWidgetHelp(m_ui.chkEEMMIO, tr("EE Known MMIO"), tr("Unchecked"), tr("Log known MMIO accesses."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEEUNKNWNMMIO, "EmuCore/TraceLog", "EE.unknownhw", false);
	dialog->registerWidgetHelp(m_ui.chkEEUNKNWNMMIO, tr("EE Unknown MMIO"), tr("Unchecked"), tr("Log unknown or unimplemented MMIO accesses."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEEDMARegs, "EmuCore/TraceLog", "EE.dmahw", false);
	dialog->registerWidgetHelp(m_ui.chkEEDMARegs, tr("EE DMA Registers"), tr("Unchecked"), tr("Log DMA-related MMIO accesses."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEEIPU, "EmuCore/TraceLog", "EE.ipu", false);
	dialog->registerWidgetHelp(m_ui.chkEEIPU, tr("EE IPU"), tr("Unchecked"), tr("Log IPU activity; MMIO, decoding operations, DMA status, etc."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEEGIFTags, "EmuCore/TraceLog", "EE.giftag", false);
	dialog->registerWidgetHelp(m_ui.chkEEGIFTags, tr("EE GIF Tags"), tr("Unchecked"), tr("Log GIFtag parsing activity."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEEVIFCodes, "EmuCore/TraceLog", "EE.vifcode", false);
	dialog->registerWidgetHelp(m_ui.chkEEVIFCodes, tr("EE VIF Codes"), tr("Unchecked"), tr("Log VIFcode processing; command, tag style, interrupts."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEEMSKPATH3, "EmuCore/TraceLog", "EE.mskpath3", false);
	dialog->registerWidgetHelp(m_ui.chkEEMSKPATH3, tr("EE MSKPATH3"), tr("Unchecked"), tr("Log Path3 Masking processing."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEEMFIFO, "EmuCore/TraceLog", "EE.spr", false);
	dialog->registerWidgetHelp(m_ui.chkEEMFIFO, tr("EE MFIFO"), tr("Unchecked"), tr("Log Scratchpad MFIFO activity."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEEDMACTRL, "EmuCore/TraceLog", "EE.dmac", false);
	dialog->registerWidgetHelp(m_ui.chkEEDMACTRL, tr("EE DMA Controller"), tr("Unchecked"), tr("Log DMA transfer activity. Stalls, bus right arbitration, etc."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEECounters, "EmuCore/TraceLog", "EE.counters", false);
	dialog->registerWidgetHelp(m_ui.chkEECounters, tr("EE Counters"), tr("Unchecked"), tr("Log all EE counters events and some counter register activity."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEEVIF, "EmuCore/TraceLog", "EE.vif", false);
	dialog->registerWidgetHelp(m_ui.chkEEVIF, tr("EE VIF"), tr("Unchecked"), tr("Log various VIF and VIFcode processing data."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEEGIF, "EmuCore/TraceLog", "EE.gif", false);
	dialog->registerWidgetHelp(m_ui.chkEEGIF, tr("EE GIF"), tr("Unchecked"), tr("Log various GIF and GIFtag parsing data."));

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkIOPBIOS, "EmuCore/TraceLog", "IOP.Bios", false);
	dialog->registerWidgetHelp(m_ui.chkIOPBIOS, tr("IOP BIOS"), tr("Unchecked"), tr("Log SYSCALL and IRX activity."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkIOPMemcards, "EmuCore/TraceLog", "IOP.memcards", false);
	dialog->registerWidgetHelp(m_ui.chkIOPMemcards, tr("IOP Memcards"), tr("Unchecked"), tr("Log memory card activity. Reads, Writes, erases, etc."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkIOPR3000A, "EmuCore/TraceLog", "IOP.r3000a", false);
	dialog->registerWidgetHelp(m_ui.chkIOPR3000A, tr("IOP R3000A"), tr("Unchecked"), tr("Log R3000A core instructions (excluding COPs)."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkIOPCOP2, "EmuCore/TraceLog", "IOP.cop2", false);
	dialog->registerWidgetHelp(m_ui.chkIOPCOP2, tr("IOP COP2"), tr("Unchecked"), tr("Log IOP GPU co-processor instructions."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkIOPMMIO, "EmuCore/TraceLog", "IOP.knownhw", false);
	dialog->registerWidgetHelp(m_ui.chkIOPMMIO, tr("IOP Known MMIO"), tr("Unchecked"), tr("Log known MMIO accesses."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkIOPUNKNWNMMIO, "EmuCore/TraceLog", "IOP.unknownhw", false);
	dialog->registerWidgetHelp(m_ui.chkIOPUNKNWNMMIO, tr("IOP Unknown MMIO"), tr("Unchecked"), tr("Log unknown or unimplemented MMIO accesses."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkIOPDMARegs, "EmuCore/TraceLog", "IOP.dmahw", false);
	dialog->registerWidgetHelp(m_ui.chkIOPDMARegs, tr("IOP DMA Registers"), tr("Unchecked"), tr("Log DMA-related MMIO accesses."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkIOPPad, "EmuCore/TraceLog", "IOP.pad", false);
	dialog->registerWidgetHelp(m_ui.chkIOPPad, tr("IOP PAD"), tr("Unchecked"), tr("Log PAD activity."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkIOPDMACTRL, "EmuCore/TraceLog", "IOP.dmac", false);
	dialog->registerWidgetHelp(m_ui.chkIOPDMACTRL, tr("IOP DMA Controller"), tr("Unchecked"), tr("Log DMA transfer activity. Stalls, bus right arbitration, etc."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkIOPCounters, "EmuCore/TraceLog", "IOP.counters", false);
	dialog->registerWidgetHelp(m_ui.chkIOPCounters, tr("IOP Counters"), tr("Unchecked"), tr("Log all IOP counters events and some counter register activity."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkIOPCDVD, "EmuCore/TraceLog", "IOP.cdvd", false);
	dialog->registerWidgetHelp(m_ui.chkIOPCDVD, tr("IOP CDVD"), tr("Unchecked"), tr("Log CDVD hardware activity."));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkIOPMDEC, "EmuCore/TraceLog", "IOP.mdec", false);
	dialog->registerWidgetHelp(m_ui.chkIOPMDEC, tr("IOP MDEC"), tr("Unchecked"), tr("Log Motion (FMV) Decoder hardware unit activity."));

	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.chkEESIF, "EmuCore/TraceLog", "MISC.sif", false);
	dialog->registerWidgetHelp(m_ui.chkEESIF, tr("EE SIF"), tr("Unchecked"), tr("Log SIF (EE <-> IOP) activity."));

	connect(m_ui.chkEnable, &QCheckBox::checkStateChanged, this, &DebugSettingsWidget::onLoggingEnableChanged);
	onLoggingEnableChanged();

	#else
		m_ui.debugTabs->removeTab(m_ui.debugTabs->indexOf(m_ui.traceLogTabWidget));
	#endif
}

DebugSettingsWidget::~DebugSettingsWidget() = default;

void DebugSettingsWidget::onDrawDumpingChanged()
{
	const bool enabled = m_dialog->getEffectiveBoolValue("EmuCore/GS", "DumpGSData", false);
	m_ui.saveRT->setEnabled(enabled);
	m_ui.saveFrame->setEnabled(enabled);
	m_ui.saveTexture->setEnabled(enabled);
	m_ui.saveDepth->setEnabled(enabled);
	m_ui.saveAlpha->setEnabled(enabled);
	m_ui.saveInfo->setEnabled(enabled);
	m_ui.saveDrawStart->setEnabled(enabled);
	m_ui.saveDrawCount->setEnabled(enabled);
	m_ui.saveFrameStart->setEnabled(enabled);
	m_ui.saveFrameCount->setEnabled(enabled);
	m_ui.hwDumpDirectory->setEnabled(enabled);
	m_ui.hwDumpBrowse->setEnabled(enabled);
	m_ui.hwDumpOpen->setEnabled(enabled);
	m_ui.swDumpDirectory->setEnabled(enabled);
	m_ui.swDumpBrowse->setEnabled(enabled);
	m_ui.swDumpOpen->setEnabled(enabled);
}

#ifdef PCSX2_DEVBUILD
void DebugSettingsWidget::onLoggingEnableChanged()
{
	const bool enabled = m_dialog->getEffectiveBoolValue("EmuCore/TraceLog", "Enabled", false);

	m_ui.chkEEBIOS->setEnabled(enabled);
	m_ui.chkEEMemory->setEnabled(enabled);
	m_ui.chkEER5900->setEnabled(enabled);
	m_ui.chkEECOP0->setEnabled(enabled);
	m_ui.chkEECOP1->setEnabled(enabled);
	m_ui.chkEECOP2->setEnabled(enabled);
	m_ui.chkEECache->setEnabled(enabled);
	m_ui.chkEEMMIO->setEnabled(enabled);
	m_ui.chkEEUNKNWNMMIO->setEnabled(enabled);
	m_ui.chkEEDMARegs->setEnabled(enabled);
	m_ui.chkEEIPU->setEnabled(enabled);
	m_ui.chkEEGIFTags->setEnabled(enabled);
	m_ui.chkEEVIFCodes->setEnabled(enabled);
	m_ui.chkEEMSKPATH3->setEnabled(enabled);
	m_ui.chkEEMFIFO->setEnabled(enabled);
	m_ui.chkEEDMACTRL->setEnabled(enabled);
	m_ui.chkEECounters->setEnabled(enabled);
	m_ui.chkEEVIF->setEnabled(enabled);
	m_ui.chkEEGIF->setEnabled(enabled);
	m_ui.chkEESIF->setEnabled(enabled);

	m_ui.chkIOPBIOS->setEnabled(enabled);
	m_ui.chkIOPMemcards->setEnabled(enabled);
	m_ui.chkIOPR3000A->setEnabled(enabled);
	m_ui.chkIOPCOP2->setEnabled(enabled);
	m_ui.chkIOPMMIO->setEnabled(enabled);
	m_ui.chkIOPUNKNWNMMIO->setEnabled(enabled);
	m_ui.chkIOPDMARegs->setEnabled(enabled);
	m_ui.chkIOPMemcards->setEnabled(enabled);
	m_ui.chkIOPPad->setEnabled(enabled);
	m_ui.chkIOPDMACTRL->setEnabled(enabled);
	m_ui.chkIOPCounters->setEnabled(enabled);
	m_ui.chkIOPCDVD->setEnabled(enabled);
	m_ui.chkIOPMDEC->setEnabled(enabled);

	g_emu_thread->applySettings();
}
#endif
