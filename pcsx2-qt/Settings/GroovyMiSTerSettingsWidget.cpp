// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GroovyMiSTerSettingsWidget.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"

#include "pcsx2/Config.h"

#include <QtCore/QDir>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>

#include <algorithm>
#include <array>

static const char* SECTION = "GroovyMiSTer";

// QString::arg() has no u16 overload; keep the casts in one place.
static constexpr int MAX_SAFE_W = static_cast<int>(Pcsx2Config::GroovyMiSTerOptions::MAX_SAFE_H_ACTIVE);
static constexpr int MAX_SAFE_H = static_cast<int>(Pcsx2Config::GroovyMiSTerOptions::MAX_SAFE_V_ACTIVE);

// Codec combo row -> the value actually written to the INI.
//
// These are Groovy wire values (Lz4FramesCode), not display indices, and are not
// contiguous: Raw=0, LZ4=1, LZ4HC=3, NLC=7. SettingWidgetBinder's combo accessor stores
// currentIndex(), so BindWidgetToIntSetting would write 2 for "LZ4 HC" and 3 for "NLC",
// which mean something else to the FPGA and produce a garbage picture rather than an
// error. Hand-mapped instead, in the same shape as eeCycleRate in
// EmulationSettingsWidget.
static constexpr std::array<int, 4> CODEC_VALUES = {
	static_cast<int>(GroovyMiSTerCodec::Raw), // 0
	static_cast<int>(GroovyMiSTerCodec::LZ4), // 1
	static_cast<int>(GroovyMiSTerCodec::LZ4HC), // 3
	static_cast<int>(GroovyMiSTerCodec::NLC), // 7
};

// Interlace combo row -> the Interlace enum value written to the INI. Like CODEC_VALUES,
// not in row order and not contiguous: row 0 "Automatic" = ProgressiveFB(2), row 1 "Force
// progressive" = Progressive(0). Storing currentIndex() would write 0/1, and 1 means
// GroovyMiSTerInterlace::Field, so this box is hand-mapped too.
//
// "Automatic" tracks the game per frame (interlaced source -> 480i, progressive -> 480p);
// "Force progressive" always deinterlaces to 480p. GroovyMiSTerInterlace::Field is absent
// because its capture path is not implemented.
// TODO(true-fields): add a third "Interlaced fields" row (Field == 1) here and in the .ui, in the
// matching position, once per-field capture works.
static constexpr std::array<int, 2> INTERLACE_VALUES = {
	static_cast<int>(GroovyMiSTerInterlace::ProgressiveFB), // row 0: Automatic
	static_cast<int>(GroovyMiSTerInterlace::Progressive), // row 1: Force progressive
};

// Every compiled-in switchres monitor preset (3rdparty/switchres/monitor.cpp,
// monitor_set_preset), none of which needs an ini. Labels are switchres's own descriptions
// from that file. Ordered most-useful-first for PS2 on an arcade CRT rather than
// alphabetically, and null-terminated for BindWidgetToEnumSetting. Keep in sync with
// IsKnownMonitorPreset in GroovyMiSTerOutput.cpp.
//
// "d9400" (== d9800) and "polo" (== h9110) are omitted as pure aliases of entries below.
// The allowlist still accepts them, so a hand-edited INI using them keeps working.
static const char* s_monitor_names[] = {
	// Arcade multi-sync
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "Arcade 15.7/25.0/31.5 kHz - tri-sync (default)"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "Arcade 15.7/31.5 kHz - dual-sync"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "Arcade 15.7/25.0 kHz - dual-sync"),
	// Arcade single-sync
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "Arcade 15.7 kHz - standard resolution"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "Arcade 15.7-16.5 kHz - extended resolution"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "Arcade 25.0 kHz - medium resolution"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "Arcade 31.5 kHz - medium resolution"),
	// Generic / broadcast
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "Generic 15.7 kHz"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "NTSC TV - 60 Hz / 525 lines"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "PAL TV - 50 Hz / 625 lines"),
	// Specific monitor models
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "Wells Gardner D9800 / D9400 - multi-sync"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "Wells Gardner D9200 - multi-sync"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "Wells Gardner K7000"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "Wells Gardner 25K7131"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "Wei-Ya M3129"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "Makvision 2929D"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "Hantarex MTC 9110 (Polo)"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "Hantarex Polostar 25"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "Nanao MS-2930 / MS-2931"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "Nanao MS9-29"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "Rodotron 666B-29"),
	// PC CRT / VESA
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "PC CRT - 31.5 kHz / 120 Hz"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "PC CRT - 70 kHz / 120 Hz"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "VESA GTF - up to 480 lines"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "VESA GTF - up to 600 lines"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "VESA GTF - up to 768 lines"),
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "VESA GTF - up to 1024 lines"),
	// User-supplied timings
	QT_TRANSLATE_NOOP("GroovyMiSTerSettingsWidget", "Custom (use INI file below)"),
	nullptr};
static const char* s_monitor_values[] = {
	"arcade_15_25_31", "arcade_15_31", "arcade_15_25",
	"arcade_15", "arcade_15ex", "arcade_25", "arcade_31",
	"generic_15", "ntsc", "pal",
	"d9800", "d9200", "k7000", "k7131", "m3129", "m2929",
	"h9110", "pstar", "ms2930", "ms929", "r666b",
	"pc_31_120", "pc_70_120", "vesa_480", "vesa_600", "vesa_768", "vesa_1024",
	"custom", nullptr};

GroovyMiSTerSettingsWidget::GroovyMiSTerSettingsWidget(SettingsWindow* settings_dialog, QWidget* parent)
	: SettingsWidget(settings_dialog, parent)
{
	SettingsInterface* sif = dialog()->getSettingsInterface();

	setupTab(m_ui);

	// Connection.
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.enabled, SECTION, "Enabled", false);
	SettingWidgetBinder::BindWidgetToStringSetting(sif, m_ui.host, SECTION, "Host", "127.0.0.1");
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.mtu, SECTION, "Mtu",
		Pcsx2Config::GroovyMiSTerOptions::DEFAULT_MTU);

	// Display / CRT.
	SettingWidgetBinder::BindWidgetToEnumSetting(sif, m_ui.monitorPreset, SECTION, "MonitorPreset",
		s_monitor_names, s_monitor_values, "arcade_15_25_31");
	SettingWidgetBinder::BindWidgetToStringSetting(sif, m_ui.switchresIni, SECTION, "SwitchresIni", "");
	setupInterlaceBinding();
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.crtSafetyCap, SECTION, "CrtSafetyCap", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.forceNativeUpscale, SECTION, "ForceNativeUpscale", false);

	// Codec. NlcPack's values are 1 (Tiled) and 2 (Rice), i.e. combo index + 1, which the
	// binder handles natively via option_offset. Codec itself cannot - see CODEC_VALUES.
	setupCodecBinding();
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.nlcPack, SECTION, "NlcPack",
		static_cast<int>(GroovyMiSTerNlcPack::Rice), /*option_offset=*/1);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.nlcNearLevel, SECTION, "NlcNearLevel", 1);
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.rgbMode, SECTION, "RgbMode",
		static_cast<int>(GroovyMiSTerRgbMode::RGB888));

	// Latency.
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.pacing, SECTION, "Pacing",
		static_cast<int>(GroovyMiSTerPacing::Pcsx2Master));
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.readback, SECTION, "Readback",
		static_cast<int>(GroovyMiSTerReadback::Sync));
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.hostDisplay, SECTION, "HostDisplay",
		static_cast<int>(GroovyMiSTerHostDisplay::Parallel));

	// Audio.
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.tapAudio, SECTION, "TapAudio", true);

	// Diagnostics.
	SettingWidgetBinder::BindWidgetToIntSetting(sif, m_ui.logVerbosity, SECTION, "LogVerbosity", 0);

	connect(m_ui.enabled, &QCheckBox::checkStateChanged, this, &GroovyMiSTerSettingsWidget::onEnabledChanged);
	connect(m_ui.nlcPack, &QComboBox::currentIndexChanged, this, &GroovyMiSTerSettingsWidget::onNlcPackChanged);
	connect(m_ui.crtSafetyCap, &QCheckBox::toggled, this, &GroovyMiSTerSettingsWidget::onCrtSafetyCapToggled);
	connect(m_ui.switchresIniBrowse, &QPushButton::clicked, this, &GroovyMiSTerSettingsWidget::onBrowseSwitchresIni);

	// Set the initial enable/disable + warning state to match the loaded settings.
	onEnabledChanged();
	onCodecChanged();
	onNlcPackChanged();

	addTooltips();
}

GroovyMiSTerSettingsWidget::~GroovyMiSTerSettingsWidget() = default;

void GroovyMiSTerSettingsWidget::setupCodecBinding()
{
	// Hand-rolled because the stored values are non-contiguous (see CODEC_VALUES).
	// This page is global-only, so there is no "Use Global Setting" row to offset past.
	const int current = dialog()->getEffectiveIntValue(SECTION, "Codec", static_cast<int>(GroovyMiSTerCodec::NLC));

	const auto it = std::find(CODEC_VALUES.begin(), CODEC_VALUES.end(), current);
	const int index = (it != CODEC_VALUES.end()) ? static_cast<int>(std::distance(CODEC_VALUES.begin(), it))
												 : static_cast<int>(CODEC_VALUES.size() - 1); // fall back to NLC
	m_ui.codec->setCurrentIndex(index);

	connect(m_ui.codec, &QComboBox::currentIndexChanged, this, [this](int idx) {
		if (idx < 0 || idx >= static_cast<int>(CODEC_VALUES.size()))
			return;

		dialog()->setIntSettingValue(SECTION, "Codec", CODEC_VALUES[idx]);
		onCodecChanged();
	});
}

void GroovyMiSTerSettingsWidget::setupInterlaceBinding()
{
	// Hand-rolled because the two shown rows map to non-contiguous, out-of-order enum values
	// (see INTERLACE_VALUES). Same shape as setupCodecBinding(); global-only page, no offset row.
	const int current = dialog()->getEffectiveIntValue(SECTION, "Interlace",
		static_cast<int>(GroovyMiSTerInterlace::ProgressiveFB));

	const auto it = std::find(INTERLACE_VALUES.begin(), INTERLACE_VALUES.end(), current);
	// A value not in the list - notably Field(1), which has no row yet - falls back to row 0
	// (Automatic), so the box never shows a blank selection.
	const int index = (it != INTERLACE_VALUES.end())
						  ? static_cast<int>(std::distance(INTERLACE_VALUES.begin(), it))
						  : 0;
	m_ui.interlace->setCurrentIndex(index);

	connect(m_ui.interlace, &QComboBox::currentIndexChanged, this, [this](int idx) {
		if (idx < 0 || idx >= static_cast<int>(INTERLACE_VALUES.size()))
			return;

		dialog()->setIntSettingValue(SECTION, "Interlace", INTERLACE_VALUES[idx]);
	});
}

void GroovyMiSTerSettingsWidget::onEnabledChanged()
{
	const bool enabled = m_ui.enabled->isChecked();

	m_ui.displayGroup->setEnabled(enabled);
	m_ui.codecGroup->setEnabled(enabled);
	m_ui.latencyGroup->setEnabled(enabled);
	m_ui.audioGroup->setEnabled(enabled);
	m_ui.diagnosticsGroup->setEnabled(enabled);

	m_ui.host->setEnabled(enabled);
	m_ui.mtu->setEnabled(enabled);
	m_ui.labelHost->setEnabled(enabled);
	m_ui.labelMtu->setEnabled(enabled);
}

void GroovyMiSTerSettingsWidget::onCodecChanged()
{
	// Pack and NEAR ride CMD_INIT byte[1] and are read only when the codec is NLC, so they
	// mean nothing on the raw/LZ4 paths.
	const bool is_nlc = (m_ui.codec->currentIndex() >= 0 &&
						 CODEC_VALUES[m_ui.codec->currentIndex()] == static_cast<int>(GroovyMiSTerCodec::NLC));

	m_ui.nlcPack->setEnabled(is_nlc);
	m_ui.labelNlcPack->setEnabled(is_nlc);
	m_ui.nlcNearLevel->setEnabled(is_nlc);
	m_ui.labelNlcNearLevel->setEnabled(is_nlc);
	m_ui.riceWarning->setVisible(is_nlc);

	// NLC is RGB888-only - the FPGA decoder has three plane cores and no pixel-format
	// input, and CmdInit refuses anything else - so pin the format rather than leave a
	// combination that cannot connect. Combo row order matches GroovyMiSTerRgbMode, and
	// setting the row writes the setting through the binder.
	if (is_nlc)
		m_ui.rgbMode->setCurrentIndex(static_cast<int>(GroovyMiSTerRgbMode::RGB888));
	m_ui.rgbMode->setEnabled(!is_nlc);
	m_ui.labelRgbMode->setEnabled(!is_nlc);

	onNlcPackChanged();
}

void GroovyMiSTerSettingsWidget::onNlcPackChanged()
{
	// Combo row 1 == GroovyMiSTerNlcPack::Rice.
	const bool is_rice = (m_ui.nlcPack->currentIndex() == 1);
	const bool is_nlc = m_ui.nlcPack->isEnabled();

	if (!is_nlc || !is_rice)
	{
		m_ui.riceWarning->clear();
		m_ui.riceWarning->setVisible(false);
		return;
	}

	// Rice is not negotiated. A core without the Rice decoder ignores the bit in CMD_INIT and
	// parses our Rice bytes as Tiled, which shows up as a garbage picture and not as an error
	// - so this warning is permanent whenever Rice is selected, rather than a one-shot dialog.
	m_ui.riceWarning->setVisible(true);
	m_ui.riceWarning->setText(
		tr("<b>Rice requires a MiSTer core with the Rice decoder</b> (the rbf_rice_r3 kit or newer). "
		   "There is no compatibility check: an older core silently misreads the stream and the picture "
		   "will look corrupted. If that happens, switch NLC Pack to Tiled."));
}

void GroovyMiSTerSettingsWidget::onCrtSafetyCapToggled(bool checked)
{
	if (checked)
		return;

	// Turning this off lets PCSX2 send whatever modeline switchres produces. On a fixed-
	// frequency CRT - an arcade monitor especially - a modeline well outside its designed
	// envelope can destroy the horizontal output stage. Make the user say yes on purpose.
	const QMessageBox::StandardButton response = QMessageBox::warning(QtUtils::GetRootWidget(this),
		tr("Disable CRT Safety Cap"),
		tr("<b>This can permanently damage a CRT.</b>"
		   "<br><br>"
		   "The safety cap refuses any video mode outside the range a 15kHz/31kHz CRT is designed to "
		   "sync (up to %1x%2). Arcade monitors in particular can be destroyed by being driven far "
		   "outside their specification - the damage is to the hardware, not just the picture."
		   "<br><br>"
		   "Only disable this if you know your display tolerates the modes you intend to send it."
		   "<br><br>"
		   "Are you sure you want to disable the CRT safety cap?")
			.arg(MAX_SAFE_W)
			.arg(MAX_SAFE_H),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

	if (response == QMessageBox::Yes)
		return;

	// Declined. Re-check it; we deliberately do NOT block signals here, so the binder's own
	// checkStateChanged handler fires and writes `true` back to the INI (it will already have
	// written `false` by this point). Re-entering this slot with checked==true is a no-op, so
	// there is no loop.
	m_ui.crtSafetyCap->setChecked(true);
}

void GroovyMiSTerSettingsWidget::onBrowseSwitchresIni()
{
	const QString path = QFileDialog::getOpenFileName(QtUtils::GetRootWidget(this),
		tr("Select Switchres INI"), m_ui.switchresIni->text(), tr("INI Files (*.ini);;All Files (*.*)"));

	if (path.isEmpty())
		return;

	// QLineEdit's binder listens on textChanged, so this persists without any extra work.
	m_ui.switchresIni->setText(QDir::toNativeSeparators(path));
}

void GroovyMiSTerSettingsWidget::addTooltips()
{
	// PCSX2 shows this text in the description pane at the bottom of the Settings window when
	// a control is hovered. It is the only place a user will read the non-obvious constraints,
	// so put the real reasoning here rather than restating the label.

	dialog()->registerWidgetHelp(m_ui.enabled, tr("Enable MiSTer Output"), tr("Unchecked"),
		tr("Streams the emulated PS2 picture (and optionally audio) to a MiSTer FPGA over the network, so it "
		   "can be displayed on a real CRT with very low latency. Controllers plugged into the MiSTer are "
		   "read back and appear as a \"MiSTer\" input source in Controller Settings."));

	dialog()->registerWidgetHelp(m_ui.host, tr("MiSTer Host"), tr("127.0.0.1"),
		tr("IP address or hostname of the MiSTer running the Groovy core. A wired connection is strongly "
		   "recommended: the picture is streamed uncompressed or near-losslessly, and Wi-Fi cannot sustain it."));

	dialog()->registerWidgetHelp(m_ui.mtu, tr("MTU"), tr("1500"),
		tr("Size of each network packet. 1500 is the safe default for any normal network. If every device on "
		   "the path (PC, switch, MiSTer) is configured for jumbo frames, a larger value such as 3800 reduces "
		   "per-packet overhead. Setting this larger than your network actually supports will break the "
		   "stream entirely."));

	dialog()->registerWidgetHelp(m_ui.monitorPreset, tr("Monitor Preset"), tr("Tri-sync arcade (15/25/31 kHz)"),
		tr("Describes what horizontal scan rates your <b>physical</b> CRT can sync. switchres uses this to compute "
		   "the video timings sent to the MiSTer, and it is the setting that most often decides whether you get a "
		   "picture at all.<br><br>"
		   "The default <b>tri-sync arcade (15/25/31 kHz)</b> matches a standard multisync arcade monitor and "
		   "covers everything the PS2 outputs: 240p/480i over the 15 kHz band and 480p over the 31 kHz band, "
		   "switched automatically per game.<br><br>"
		   "A <b>15 kHz-only</b> profile refuses 480p; a <b>31 kHz-only</b> profile refuses 240p/480i - pick one "
		   "of those only if that is genuinely all your display can do. There are also entries for specific arcade "
		   "monitors (Wells-Gardner, etc.). <b>Custom</b> takes its timings from the Switchres INI below.<br><br>"
		   "For a truly native-resolution picture, also run at 1x internal resolution (or enable Force Native "
		   "Upscaling), since the streamed size follows the upscale multiplier."));

	dialog()->registerWidgetHelp(m_ui.switchresIni, tr("Switchres INI"), tr("Empty"),
		tr("Optional path to a custom switchres.ini, for displays that need timings beyond the built-in presets. "
		   "Leave empty unless you have written one."));

	dialog()->registerWidgetHelp(m_ui.interlace, tr("Interlacing"), tr("Automatic - match the game"),
		tr("Whether the picture sent to the MiSTer follows the game's own interlacing.<br><br>"
		   "<b>Automatic - match the game</b> streams whatever the PS2 is actually rendering: interlaced games "
		   "(480i/576i) go out as a native 15kHz interlaced signal, progressive games (240p/480p) go out "
		   "progressive, and a game that switches between them is followed automatically. This is how a real PS2 "
		   "(and the MiSTer PSX core) drives a CRT, and is the recommended choice for a 15kHz-capable display.<br><br>"
		   "<b>Force progressive (480p)</b> always sends a progressive picture, deinterlacing 480i games to 480p at "
		   "31kHz. Choose this for a 31kHz-only VGA/multisync display, or if you prefer a steadier progressive image "
		   "over native interlace.<br><br>"
		   "Either way the result is bounded by the Monitor Preset: a 15kHz-only preset cannot show 480p and a "
		   "31kHz-only preset cannot show 480i, so an unsupported mode falls back to the one the monitor can sync."));

	dialog()->registerWidgetHelp(m_ui.crtSafetyCap, tr("Enforce CRT Safety Cap"), tr("Checked"),
		tr("Refuses to send any video mode larger than %1x%2 to the MiSTer.<br><br>"
		   "<b>Leave this on.</b> Driving a fixed-frequency CRT - an arcade monitor especially - far outside the "
		   "range it was designed for can physically destroy it. With the cap on, an out-of-range mode (a game "
		   "switching to 720p or 1080i, for example) is refused and streaming pauses until the mode changes, "
		   "rather than being sent to your display.")
			.arg(MAX_SAFE_W)
			.arg(MAX_SAFE_H));

	dialog()->registerWidgetHelp(m_ui.forceNativeUpscale, tr("Force Native (1x) Upscaling While Streaming"),
		tr("Unchecked"),
		tr("Drops the graphics upscale multiplier to 1x while streaming. A CRT displays the PS2's native "
		   "resolution, so rendering at 4x and then scaling back down costs GPU time and readback bandwidth for "
		   "detail that can never be seen. Turn this on unless you are also watching the upscaled image on the "
		   "PC monitor."));

	dialog()->registerWidgetHelp(m_ui.codec, tr("Codec"), tr("NLC"),
		tr("How the picture is compressed before being sent.<br><br>"
		   "<b>NLC</b> (near-lossless) is the default and what the MiSTer core is tuned for.<br><br>"
		   "<b>Raw</b> sends uncompressed pixels - no encode cost, but far more bandwidth than a heavy scene can "
		   "afford.<br><br>"
		   "<b>LZ4</b> / <b>LZ4 HC</b> are general-purpose fallbacks for cores without NLC support."));

	dialog()->registerWidgetHelp(m_ui.nlcPack, tr("NLC Pack"), tr("Rice"),
		tr("The NLC entropy coder. Only used when the codec is NLC.<br><br>"
		   "<b>Rice</b> compresses 3D scenes much better and is what keeps demanding games under the MiSTer's "
		   "ingest limit. <b>It requires a core with the Rice decoder (rbf_rice_r3 or newer)</b> - there is no "
		   "compatibility check, and an older core will show a corrupted picture rather than an error.<br><br>"
		   "<b>Tiled</b> works on any NLC-capable core and is better on flat 2D content."));

	dialog()->registerWidgetHelp(m_ui.nlcNearLevel, tr("NLC NEAR Level"), tr("1"),
		tr("How much NLC is allowed to approximate. 0 is mathematically lossless.<br><br>"
		   "1 is the default because levels 0, 1 and 2 were confirmed <i>visually identical on a CRT</i>, while 1 "
		   "cuts the data rate enough to keep heavy 3D scenes comfortably under the MiSTer core's ingest ceiling "
		   "(roughly 38 MB/s). At level 0 a demanding scene can saturate the receiver and produce brief bands of "
		   "noise until the scene lightens.<br><br>"
		   "Raise it only if you are still seeing dropped frames."));

	dialog()->registerWidgetHelp(m_ui.rgbMode, tr("Pixel Format"), tr("RGB888"),
		tr("Colour depth on the wire. Fixed at RGB888 when the codec is NLC, which the MiSTer decodes "
		   "three bytes per pixel.<br><br>"
		   "<b>RGB888</b> (24-bit) is the default.<br><br>"
		   "<b>RGB565</b> (16-bit) halves the raw data before compression, at the cost of slight banding in "
		   "gradients - useful on a constrained link.<br><br>"
		   "<b>RGBA8888</b> sends a fourth byte the core ignores; it costs bandwidth for nothing and exists only "
		   "for debugging."));

	dialog()->registerWidgetHelp(m_ui.pacing, tr("Frame Pacing"), tr("PCSX2 Frame Limiter"),
		tr("Which clock decides when a frame is sent.<br><br>"
		   "<b>PCSX2 Frame Limiter</b> keeps PCSX2's normal timing and chases the CRT's raster with each frame. "
		   "Safe: a network hiccup can never stall emulation.<br><br>"
		   "<b>MiSTer CRT Raster</b> makes the CRT the master clock and paces emulation off it. This gives the "
		   "most consistent latency, but if the network stalls, emulation stalls with it. Experimental."));

	dialog()->registerWidgetHelp(m_ui.readback, tr("GPU Readback"), tr("Synchronous"),
		tr("How the finished frame is copied off the GPU.<br><br>"
		   "<b>Synchronous</b> adds no latency at all - the frame is read back the moment it is drawn. It briefly "
		   "blocks the GS thread, which PCSX2 absorbs, so this is the right default.<br><br>"
		   "<b>Deferred</b> reads the frame back one frame later. It never blocks, at the cost of exactly one "
		   "frame of extra latency. Use it only if Synchronous hurts performance on your GPU."));

	dialog()->registerWidgetHelp(m_ui.hostDisplay, tr("Host Display"), tr("Parallel"),
		tr("<b>Parallel</b> keeps drawing to the PCSX2 window as well as the MiSTer.<br><br>"
		   "<b>Headless</b> skips presenting to the PC entirely, so the MiSTer is the only output. Saves a little "
		   "GPU work - intended for a dedicated cabinet where nobody is looking at the PC monitor."));

	dialog()->registerWidgetHelp(m_ui.tapAudio, tr("Mirror Audio to MiSTer"), tr("Checked"),
		tr("Sends the emulated audio to the MiSTer alongside the picture, so sound comes out of the same place as "
		   "the image. Turn this off to keep audio on your PC's own sound device."));

	dialog()->registerWidgetHelp(m_ui.logVerbosity, tr("Client Log Verbosity"), tr("Errors and Setup Only"),
		tr("How much the MiSTer streaming client writes to the console.<br><br>"
		   "The default logs connection and errors only. The higher levels log once (or many times) <i>per frame</i> "
		   "and measurably add latency to the sender - use them for short, targeted debugging, not for normal play."));
}

#include "moc_GroovyMiSTerSettingsWidget.cpp"
