// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "pcsx2/SIO/Pad/Pad.h"
#include "GameSummaryWidget.h"
#include "SettingsWindow.h"
#include "MainWindow.h"
#include "QtHost.h"
#include "QtProgressCallback.h"
#include "QtUtils.h"

#include "pcsx2/CDVD/IsoHasher.h"
#include "pcsx2/GameDatabase.h"
#include "pcsx2/GameList.h"

#include "common/Error.h"
#include "common/MD5Digest.h"
#include "common/ScopedGuard.h"
#include "common/StringUtil.h"

#include "fmt/format.h"

#include <QtCore/QDir>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>

GameSummaryWidget::GameSummaryWidget(const GameList::Entry* entry, SettingsWindow* settings_dialog, QWidget* parent)
	: SettingsWidget(settings_dialog, parent)
{
	setupTab(m_ui);

	const QString base_path(QtHost::GetResourcesBasePath());
	for (int i = 0; i < m_ui.region->count(); i++)
	{
		m_ui.region->setItemIcon(i,
			QIcon(QStringLiteral("%1/icons/flags/%2.svg").arg(base_path).arg(GameList::RegionToFlagFilename(static_cast<GameList::Region>(i)))));
	}

	m_entry_path = entry->path;
	populateInputProfiles();
	populateDetails(entry);
	populateDiscPath(entry);
	populateTrackList(entry);

	connect(m_ui.inputProfile, &QComboBox::currentIndexChanged, this, &GameSummaryWidget::onInputProfileChanged);
	connect(m_ui.verify, &QAbstractButton::clicked, this, &GameSummaryWidget::onVerifyClicked);
	connect(m_ui.searchHash, &QAbstractButton::clicked, this, &GameSummaryWidget::onSearchHashClicked);
	connect(m_ui.checkWiki, &QAbstractButton::clicked, this,
		[this, serial = entry->serial]() { onCheckWikiClicked(serial); });

	bool has_custom_title = false, has_custom_region = false;
	GameList::CheckCustomAttributesForPath(m_entry_path, has_custom_title, has_custom_region);
	m_ui.restoreTitle->setEnabled(has_custom_title);
	m_ui.restoreRegion->setEnabled(has_custom_region);
	m_ui.checkWiki->setEnabled(!entry->serial.empty());
}

GameSummaryWidget::~GameSummaryWidget() = default;

void GameSummaryWidget::populateInputProfiles()
{
	for (const std::string& name : Pad::GetInputProfileNames())
		m_ui.inputProfile->addItem(QString::fromStdString(name));
}

void GameSummaryWidget::populateDetails(const GameList::Entry* entry)
{
	m_ui.title->setText(QString::fromStdString(entry->title));
	m_ui.titleSort->setText(QString::fromStdString(entry->title_sort));
	m_ui.titleEN->setText(QString::fromStdString(entry->title_en));
	m_ui.path->setText(QString::fromStdString(entry->path));
	m_ui.serial->setText(QString::fromStdString(entry->serial));
	m_ui.crc->setText(QString::fromStdString(fmt::format("{:08X}", entry->crc)));
	m_ui.type->setCurrentIndex(static_cast<int>(entry->type));
	m_ui.region->setCurrentIndex(static_cast<int>(entry->region));
	//: First arg is a GameList compat; second is a string with space followed by star rating OR empty if Unknown compat
	m_ui.compatibility->setText(
		tr("%0%1")
			.arg(GameList::EntryCompatibilityRatingToString(entry->compatibility_rating, true))
			.arg([entry]() {
				if (entry->compatibility_rating == GameList::CompatibilityRating::Unknown)
					return QStringLiteral("");

				const qsizetype compatibility_value = static_cast<qsizetype>(entry->compatibility_rating);
				//: First arg is filled-in stars for game compatibility; second is empty stars; should be swapped for RTL languages
				return tr(" %0%1").arg(QStringLiteral("★").repeated(compatibility_value - 1)).arg(QStringLiteral("☆").repeated(6 - compatibility_value));
			}()));

	int row = 0;
	m_ui.detailsFormLayout->getWidgetPosition(m_ui.titleSort, &row, nullptr);
	m_ui.detailsFormLayout->setRowVisible(row, !entry->title_sort.empty());
	m_ui.detailsFormLayout->getWidgetPosition(m_ui.titleEN, &row, nullptr);
	m_ui.detailsFormLayout->setRowVisible(row, !entry->title_en.empty());

	std::optional<std::string> profile(dialog()->getStringValue("EmuCore", "InputProfileName", std::nullopt));
	if (profile.has_value())
		m_ui.inputProfile->setCurrentIndex(m_ui.inputProfile->findText(QString::fromStdString(profile.value())));
	else
		m_ui.inputProfile->setCurrentIndex(0);

	connect(m_ui.title, &QLineEdit::editingFinished, this, [this]() {
		if (m_ui.title->isModified())
		{
			setCustomTitle(m_ui.title->text().toStdString());
			m_ui.title->setModified(false);
		}
	});
	connect(m_ui.restoreTitle, &QAbstractButton::clicked, this, [this]() {
		setCustomTitle("");
	});

	connect(m_ui.region, &QComboBox::currentIndexChanged, this, [this](int index) {
		setCustomRegion(index);
	});
	connect(m_ui.restoreRegion, &QAbstractButton::clicked, this, [this]() {
		setCustomRegion(-1);
	});
}

void GameSummaryWidget::populateDiscPath(const GameList::Entry* entry)
{
	if (entry->type == GameList::EntryType::ELF)
	{
		std::optional<std::string> iso_path(dialog()->getStringValue("EmuCore", "DiscPath", std::nullopt));
		if (iso_path.has_value() && !iso_path->empty())
			m_ui.discPath->setText(QString::fromStdString(iso_path.value()));

		connect(m_ui.discPath, &QLineEdit::textChanged, this, &GameSummaryWidget::onDiscPathChanged);
		connect(m_ui.discPathBrowse, &QPushButton::clicked, this, &GameSummaryWidget::onDiscPathBrowseClicked);
		connect(m_ui.discPathClear, &QPushButton::clicked, m_ui.discPath, &QLineEdit::clear);
	}
	else
	{
		// Makes no sense to have disc override for a disc.
		int row = 0;
		m_ui.detailsFormLayout->getWidgetPosition(m_ui.label_discPath, &row, nullptr);
		m_ui.detailsFormLayout->removeRow(row);
		m_ui.discPath = nullptr;
		m_ui.discPathBrowse = nullptr;
		m_ui.discPathClear = nullptr;
	}
}

void GameSummaryWidget::onInputProfileChanged(int index)
{
	if (index == 0)
		dialog()->setStringSettingValue("EmuCore", "InputProfileName", std::nullopt);
	else
		dialog()->setStringSettingValue("EmuCore", "InputProfileName", m_ui.inputProfile->itemText(index).toUtf8());
}

void GameSummaryWidget::onDiscPathChanged(const QString& value)
{
	if (value.isEmpty())
		dialog()->removeSettingValue("EmuCore", "DiscPath");
	else
		dialog()->setStringSettingValue("EmuCore", "DiscPath", value.toStdString().c_str());

	// force rescan of elf to update the serial
	g_main_window->rescanFile(m_entry_path);

	auto lock = GameList::GetLock();
	const GameList::Entry* entry = GameList::GetEntryForPath(m_entry_path.c_str());
	if (entry)
	{
		populateDetails(entry);
		dialog()->setSerial(entry->serial);
		m_ui.checkWiki->setEnabled(!entry->serial.empty());
	}
}

void GameSummaryWidget::onDiscPathBrowseClicked()
{
	const QString filename(QFileDialog::getOpenFileName(
		QtUtils::GetRootWidget(this), tr("Select Disc Path"), QString(), qApp->translate("MainWindow", MainWindow::DISC_IMAGE_FILTER)));
	if (filename.isEmpty())
		return;

	// let the signal take care of it
	m_ui.discPath->setText(QDir::toNativeSeparators(filename));
}

void GameSummaryWidget::populateTrackList(const GameList::Entry* entry)
{
	if (entry->type != GameList::EntryType::PS1Disc && entry->type != GameList::EntryType::PS2Disc)
	{
		m_ui.verify->setEnabled(false);
		m_ui.verifyResult->setPlainText(tr("Game is not a CD/DVD."));
		return;
	}

	if (QtHost::IsVMValid())
	{
		m_ui.verify->setEnabled(false);
		m_ui.verifyResult->setPlainText(tr("Track list unavailable while virtual machine is running."));
		return;
	}

	IsoHasher hasher;
	Error error;
	if (!hasher.Open(m_entry_path, &error))
	{
		m_ui.verify->setEnabled(false);
		m_ui.verifyResult->setPlainText(QString::fromStdString(error.GetDescription()));
		return;
	}

	const auto AddColumn = [this](const QString& text) {
		QTableWidgetItem* item = new QTableWidgetItem(text);
		const int column = m_ui.tracks->columnCount();
		m_ui.tracks->insertColumn(column);
		m_ui.tracks->setHorizontalHeaderItem(column, item);
	};
	const auto SetColumn = [this](int row, int column, const QString& text) {
		QTableWidgetItem* item = new QTableWidgetItem(text);
		m_ui.tracks->setItem(row, column, item);
	};

	// columns depend on CD vs DVD.
	AddColumn(tr("#"));
	if (hasher.IsCD())
	{
		AddColumn(tr("Mode"));
		AddColumn(tr("Start"));
		AddColumn(tr("Sectors"));
		AddColumn(tr("Size"));
		AddColumn(tr("MD5"));
		AddColumn(tr("Status"));
	}
	else
	{
		AddColumn(tr("Start"));
		AddColumn(tr("Sectors"));
		AddColumn(tr("Size"));
		AddColumn(tr("MD5"));
		AddColumn(tr("Status"));
	}

	for (const IsoHasher::Track& track : hasher.GetTracks())
	{
		const int row = m_ui.tracks->rowCount();
		m_ui.tracks->insertRow(row);

		SetColumn(row, 0, tr("%1").arg(track.number));

		if (hasher.IsCD())
		{
			SetColumn(row, 1, QtUtils::StringViewToQString(IsoHasher::GetTrackTypeString(track.type)));
			SetColumn(row, 2, tr("%1").arg(track.start_lsn));
			SetColumn(row, 3, tr("%1").arg(track.sectors));
			SetColumn(row, 4, tr("%1").arg(track.size));
			SetColumn(row, 5, tr("<not computed>"));
			SetColumn(row, 6, QString());
		}
		else
		{
			SetColumn(row, 1, tr("%1").arg(track.start_lsn));
			SetColumn(row, 2, tr("%1").arg(track.sectors));
			SetColumn(row, 3, tr("%1").arg(track.size));
			SetColumn(row, 4, tr("<not computed>"));
			SetColumn(row, 5, QString());
		}
	}

	if (hasher.IsCD())
		QtUtils::ResizeColumnsForTableView(m_ui.tracks, {20, 60, 70, 70, 100, 220, 40});
	else
		QtUtils::ResizeColumnsForTableView(m_ui.tracks, {20, 100, 100, 100, 220, 40});
}

void GameSummaryWidget::onVerifyClicked()
{
	// Can't do this while a VM is running because of stupid CDVD.
	if (QtHost::IsVMValid())
	{
		QMessageBox::critical(QtUtils::GetRootWidget(this), tr("Error"), tr("Cannot verify image while a game is running."));
		return;
	}

	IsoHasher hasher;
	Error error;
	if (!hasher.Open(m_entry_path, &error))
	{
		QString message(QString::fromStdString(error.GetDescription()));
		QMessageBox::critical(QtUtils::GetRootWidget(this), tr("Error"), message);
		return;
	}

	QtModalProgressCallback callback(this);
	hasher.ComputeHashes(&callback);
	if (callback.IsCancelled())
		return;

	const int hash_column = hasher.IsCD() ? 5 : 4;
	int row = 0;

	// convert to database format
	std::vector<GameDatabase::TrackHash> thashes;
	thashes.reserve(hasher.GetTrackCount());
	for (const IsoHasher::Track& track : hasher.GetTracks())
	{
		GameDatabase::TrackHash thash;
		thash.size = track.size;
		if (track.hash.empty() || !thash.parseHash(track.hash))
		{
			m_ui.verify->setEnabled(false);
			m_ui.verifyResult->setPlainText(tr("One or more tracks is missing."));
			return;
		}

		// Use the first track's hash as the redump search term.
		if (m_redump_search_keyword.empty())
			m_redump_search_keyword = thash.toString();

		thashes.push_back(thash);
	}

	// match the hashes. can't use vector<bool> here because it's not an actual array
	std::unique_ptr<bool[]> val_results = std::make_unique<bool[]>(hasher.GetTrackCount());
	std::string match_error;
	const GameDatabase::HashDatabaseEntry* hentry =
		GameDatabase::lookupHash(thashes.data(), thashes.size(), val_results.get(), &match_error);

	// fill the UI with both the hashes and validation results
	for (u32 i = 0; i < hasher.GetTrackCount(); i++)
	{
		QTableWidgetItem* const hash_item = m_ui.tracks->item(row, hash_column);
		QTableWidgetItem* const status_item = m_ui.tracks->item(row, hash_column + 1);

		const bool result = val_results[i];
		const QBrush brush(result ? QColor(0, 200, 0) : QColor(200, 0, 0));

		hash_item->setText(QString::fromStdString(hasher.GetTrack(i).hash));
		hash_item->setForeground(brush);
		status_item->setText(result ? QStringLiteral("\u2713") : QStringLiteral("\u2715"));
		status_item->setForeground(brush);
		row++;
	}

	if (hentry)
	{
		if (!hentry->version.empty())
		{
			setVerifyResult(tr("Verified as %1 [%2] (Version %3).")
								.arg(QString::fromStdString(hentry->name))
								.arg(QString::fromStdString(hentry->serial))
								.arg(QString::fromStdString(hentry->version)));
		}
		else
		{
			setVerifyResult(tr("Verified as %1 [%2].")
								.arg(QString::fromStdString(hentry->name))
								.arg(QString::fromStdString(hentry->serial)));
		}
	}
	else
	{
		setVerifyResult(QString::fromStdString(match_error));
	}
}

void GameSummaryWidget::onSearchHashClicked()
{
	if (m_redump_search_keyword.empty())
		return;

	QtUtils::OpenURL(this, fmt::format("http://redump.org/discs/quicksearch/{}", m_redump_search_keyword).c_str());
}

void GameSummaryWidget::onCheckWikiClicked(const std::string& serial)
{
	QtUtils::OpenURL(this, fmt::format("https://wiki.pcsx2.net/{}", serial).c_str());
}

void GameSummaryWidget::setVerifyResult(QString error)
{
	m_ui.verify->setVisible(false);
	m_ui.verifyButtonLayout->removeWidget(m_ui.verify);
	m_ui.verify->deleteLater();
	m_ui.verify = nullptr;
	m_ui.verifyButtonLayout->removeItem(m_ui.verifyButtonSpacer);
	delete m_ui.verifyButtonSpacer;
	m_ui.verifyButtonSpacer = nullptr;
	m_ui.verifyLayout->removeItem(m_ui.verifyButtonLayout);
	m_ui.verifyButtonLayout->deleteLater();
	m_ui.verifyButtonLayout = nullptr;
	m_ui.verifyLayout->update();
	updateGeometry();

	m_ui.verifyResult->setPlainText(error);
	m_ui.verifyResult->setVisible(true);
	m_ui.searchHash->setVisible(true);
}

void GameSummaryWidget::repopulateCurrentDetails()
{
	auto lock = GameList::GetLock();
	const GameList::Entry* entry = GameList::GetEntryForPath(m_entry_path.c_str());
	if (entry)
	{
		populateDetails(entry);
		dialog()->setWindowTitle(QString::fromStdString(entry->title));
	}
}

void GameSummaryWidget::setCustomTitle(const std::string& text)
{
	m_ui.restoreTitle->setEnabled(!text.empty());

	GameList::SaveCustomTitleForPath(m_entry_path, text);
	repopulateCurrentDetails();
}

void GameSummaryWidget::setCustomRegion(int region)
{
	m_ui.restoreRegion->setEnabled(region >= 0);

	GameList::SaveCustomRegionForPath(m_entry_path, region);
	repopulateCurrentDetails();
}
