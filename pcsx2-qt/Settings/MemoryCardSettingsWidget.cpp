// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include <QtGui/QDrag>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <algorithm>

#include "common/StringUtil.h"

#include "MemoryCardConvertDialog.h"
#include "MemoryCardCreateDialog.h"
#include "MemoryCardSettingsWidget.h"
#include "QtHost.h"
#include "QtUtils.h"
#include "SettingWidgetBinder.h"
#include "SettingsWindow.h"

#include "pcsx2/SIO/Memcard/MemoryCardFile.h"

static constexpr const char* CONFIG_SECTION = "MemoryCards";

static std::string getSlotFilenameKey(u32 slot)
{
	return StringUtil::StdStringFromFormat("Slot%u_Filename", slot + 1);
}

MemoryCardSettingsWidget::MemoryCardSettingsWidget(SettingsWindow* dialog, QWidget* parent)
	: QWidget(parent)
	, m_dialog(dialog)
{
	SettingsInterface* sif = m_dialog->getSettingsInterface();

	m_ui.setupUi(this);

	// this is a bit lame, but resizeEvent() isn't good enough to autosize our columns,
	// since the group box hasn't been resized at that point.
	m_ui.cardGroupBox->installEventFilter(this);

	SettingWidgetBinder::BindWidgetToFolderSetting(sif, m_ui.directory, m_ui.browse, m_ui.open, m_ui.reset, "Folders",
		"MemoryCards", Path::Combine(EmuFolders::DataRoot, "memcards"));
	SettingWidgetBinder::BindWidgetToBoolSetting(sif, m_ui.automaticManagement, "EmuCore", "McdFolderAutoManage", true);

	setupAdditionalUi();

	connect(m_ui.directory, &QLineEdit::textChanged, this, &MemoryCardSettingsWidget::refresh);
	m_ui.cardList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(
		m_ui.cardList, &MemoryCardListWidget::itemSelectionChanged, this, &MemoryCardSettingsWidget::updateCardActions);
	connect(m_ui.cardList, &MemoryCardListWidget::customContextMenuRequested, this,
		&MemoryCardSettingsWidget::listContextMenuRequested);

	connect(m_ui.refreshCard, &QPushButton::clicked, this, &MemoryCardSettingsWidget::refresh);
	connect(m_ui.createCard, &QPushButton::clicked, this, &MemoryCardSettingsWidget::createCard);
	connect(m_ui.duplicateCard, &QPushButton::clicked, this, &MemoryCardSettingsWidget::duplicateCard);
	connect(m_ui.renameCard, &QPushButton::clicked, this, &MemoryCardSettingsWidget::renameCard);
	connect(m_ui.convertCard, &QPushButton::clicked, this, &MemoryCardSettingsWidget::convertCard);
	connect(m_ui.deleteCard, &QPushButton::clicked, this, &MemoryCardSettingsWidget::deleteCard);

	refresh();

	dialog->registerWidgetHelp(m_ui.automaticManagement, tr("Automatically manage saves based on running game"),
		tr("Checked"),
		tr("(Folder type only / Card size: Auto) Loads only the relevant booted game saves, ignoring others. Avoids "
		   "running out of space for saves."));
}

MemoryCardSettingsWidget::~MemoryCardSettingsWidget() = default;

void MemoryCardSettingsWidget::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
	autoSizeUI();
}

bool MemoryCardSettingsWidget::eventFilter(QObject* watched, QEvent* event)
{
	if (watched == m_ui.cardGroupBox && event->type() == QEvent::Resize)
		autoSizeUI();

	return QWidget::eventFilter(watched, event);
}

void MemoryCardSettingsWidget::setupAdditionalUi()
{
	for (u32 i = 0; i < static_cast<u32>(m_slots.size()); i++)
		createSlotWidgets(&m_slots[i], i);

	// button to swap Memory Cards
	QToolButton* swap_button = new QToolButton(m_ui.portGroupBox);
	swap_button->setIcon(QIcon::fromTheme("arrow-left-right-line"));
	swap_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
	swap_button->setToolTip(tr("Swap Memory Cards"));
	connect(swap_button, &QToolButton::clicked, this, &MemoryCardSettingsWidget::swapCards);
	static_cast<QGridLayout*>(m_ui.portGroupBox->layout())->addWidget(swap_button, 0, 1);
}

void MemoryCardSettingsWidget::createSlotWidgets(SlotGroup* port, u32 slot)
{
	const bool perGame = m_dialog->isPerGameSettings();

	port->root = new QWidget(m_ui.portGroupBox);

	SettingsInterface* sif = m_dialog->getSettingsInterface();
	port->enable = new QCheckBox(tr("Port %1").arg(slot + 1), port->root);
	SettingWidgetBinder::BindWidgetToBoolSetting(
		sif, port->enable, CONFIG_SECTION, StringUtil::StdStringFromFormat("Slot%u_Enable", slot + 1), true);
	connect(port->enable, &QCheckBox::stateChanged, this, &MemoryCardSettingsWidget::refresh);

	port->eject = new QToolButton(port->root);
	port->eject->setIcon(QIcon::fromTheme(perGame ? QStringLiteral("delete-back-2-line") : QStringLiteral("eject-line")));
	port->eject->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
	port->eject->setToolTip(perGame ? tr("Reset") : tr("Eject Memory Card"));
	connect(port->eject, &QToolButton::clicked, this, [this, slot]() { ejectSlot(slot); });

	port->slot = new MemoryCardSlotWidget(port->root);
	connect(port->slot, &MemoryCardSlotWidget::cardDropped, this,
		[this, slot](const QString& card) { tryInsertCard(slot, card); });

	QHBoxLayout* bottom_layout = new QHBoxLayout();
	bottom_layout->setContentsMargins(0, 0, 0, 0);
	bottom_layout->addWidget(port->slot, 1);
	bottom_layout->addWidget(port->eject, 0);

	QVBoxLayout* vert_layout = new QVBoxLayout(port->root);
	vert_layout->setContentsMargins(0, 0, 0, 0);
	vert_layout->addWidget(port->enable, 0);
	vert_layout->addLayout(bottom_layout, 1);

	static_cast<QGridLayout*>(m_ui.portGroupBox->layout())->addWidget(port->root, 0, (slot != 0) ? 2 : 0);
}

void MemoryCardSettingsWidget::autoSizeUI()
{
	QtUtils::ResizeColumnsForTreeView(m_ui.cardList, {-1, 100, 80, 150});
}

void MemoryCardSettingsWidget::tryInsertCard(u32 slot, const QString& newCard)
{
	// handle where the card is dragged in from explorer or something
	const int lastSlashPos = std::max(newCard.lastIndexOf('/'), newCard.lastIndexOf('\\'));
	const std::string newCardStr(
		(lastSlashPos >= 0) ? newCard.mid(0, lastSlashPos).toStdString() : newCard.toStdString());
	if (newCardStr.empty())
		return;

	// make sure it's a card in the directory
	const std::vector<AvailableMcdInfo> mcds(FileMcd_GetAvailableCards(true));
	if (std::none_of(
			mcds.begin(), mcds.end(), [&newCardStr](const AvailableMcdInfo& mcd) { return mcd.name == newCardStr; }))
	{
		QMessageBox::critical(this, tr("Error"), tr("This Memory Card is unknown."));
		return;
	}

	m_dialog->setStringSettingValue(CONFIG_SECTION, getSlotFilenameKey(slot).c_str(), newCardStr.c_str());
	refresh();
}

void MemoryCardSettingsWidget::ejectSlot(u32 slot)
{
	m_dialog->setStringSettingValue(CONFIG_SECTION, getSlotFilenameKey(slot).c_str(),
		m_dialog->isPerGameSettings() ? std::nullopt : std::optional<const char*>(""));
	refresh();
}

void MemoryCardSettingsWidget::createCard()
{
	MemoryCardCreateDialog dialog(QtUtils::GetRootWidget(this));
	if (dialog.exec() == QDialog::Accepted)
		refresh();
}

QString MemoryCardSettingsWidget::getSelectedCard() const
{
	QString ret;

	const QList<QTreeWidgetItem*> selection(m_ui.cardList->selectedItems());
	if (!selection.empty())
		ret = selection[0]->text(0);

	return ret;
}

void MemoryCardSettingsWidget::updateCardActions()
{
	QString selectedCard = getSelectedCard();
	const bool hasSelection = !selectedCard.isEmpty();

	std::optional<AvailableMcdInfo> cardInfo = FileMcd_GetCardInfo(selectedCard.toStdString());
	bool isPS1 = (cardInfo.has_value() ? cardInfo.value().file_type == MemoryCardFileType::PS1 : false);

	m_ui.convertCard->setEnabled(hasSelection && !isPS1);
	m_ui.duplicateCard->setEnabled(hasSelection);
	m_ui.renameCard->setEnabled(hasSelection);
	m_ui.deleteCard->setEnabled(hasSelection);
}

void MemoryCardSettingsWidget::duplicateCard()
{
	const QString selectedCard(getSelectedCard());
	if (selectedCard.isEmpty())
		return;

	QMessageBox::critical(this, tr("Error"), tr("Not yet implemented."));
}

void MemoryCardSettingsWidget::deleteCard()
{
	const QString selectedCard(getSelectedCard());
	if (selectedCard.isEmpty())
		return;

	if (QMessageBox::question(QtUtils::GetRootWidget(this), tr("Delete Memory Card"),
			tr("Are you sure you wish to delete the Memory Card '%1'?\n\n"
			   "This action cannot be reversed, and you will lose any saves on the card.")
				.arg(selectedCard)) != QMessageBox::Yes)
	{
		return;
	}

	if (!FileMcd_DeleteCard(selectedCard.toStdString()))
	{
		QMessageBox::critical(QtUtils::GetRootWidget(this), tr("Delete Memory Card"),
			tr("Failed to delete the Memory Card. The log may have more information."));
		return;
	}

	refresh();
}

void MemoryCardSettingsWidget::renameCard()
{
	const QString selectedCard(getSelectedCard());
	if (selectedCard.isEmpty())
		return;

	const QString newName(QInputDialog::getText(
		QtUtils::GetRootWidget(this), tr("Rename Memory Card"), tr("New Card Name"), QLineEdit::Normal, selectedCard));
	if (newName.isEmpty() || newName == selectedCard)
		return;

	if (!newName.endsWith(QStringLiteral(".ps2")) || newName.length() <= 4)
	{
		QMessageBox::critical(
			QtUtils::GetRootWidget(this), tr("Rename Memory Card"), tr("New name is invalid, it must end with .ps2"));
		return;
	}

	const std::string newNameStr(newName.toStdString());
	if (FileMcd_GetCardInfo(newNameStr).has_value())
	{
		QMessageBox::critical(QtUtils::GetRootWidget(this), tr("Rename Memory Card"),
			tr("New name is invalid, a card with this name already exists."));
		return;
	}

	if (!FileMcd_RenameCard(selectedCard.toStdString(), newNameStr))
	{
		QMessageBox::critical(QtUtils::GetRootWidget(this), tr("Rename Memory Card"),
			tr("Failed to rename Memory Card. The log may contain more information."));
		return;
	}

	refresh();
}

void MemoryCardSettingsWidget::convertCard()
{
	const QString selectedCard(getSelectedCard());

	if (selectedCard.isEmpty())
		return;

	MemoryCardConvertDialog dialog(QtUtils::GetRootWidget(this), selectedCard);

	if (dialog.IsSetup() && dialog.exec() == QDialog::Accepted)
		refresh();
}

void MemoryCardSettingsWidget::listContextMenuRequested(const QPoint& pos)
{
	QMenu menu(this);

	const QString selectedCard(getSelectedCard());
	if (!selectedCard.isEmpty())
	{
		for (u32 slot = 0; slot < MAX_SLOTS; slot++)
		{
			connect(menu.addAction(tr("Use for Port %1").arg(slot + 1)), &QAction::triggered, this,
				[this, &selectedCard, slot]() { tryInsertCard(slot, selectedCard); });
		}
		menu.addSeparator();

		connect(menu.addAction(tr("Duplicate")), &QAction::triggered, this, &MemoryCardSettingsWidget::duplicateCard);
		connect(menu.addAction(tr("Rename")), &QAction::triggered, this, &MemoryCardSettingsWidget::renameCard);
		connect(menu.addAction(tr("Convert")), &QAction::triggered, this, &MemoryCardSettingsWidget::convertCard);
		connect(menu.addAction(tr("Delete")), &QAction::triggered, this, &MemoryCardSettingsWidget::deleteCard);
		menu.addSeparator();
	}

	connect(menu.addAction(tr("Create")), &QAction::triggered, this, &MemoryCardSettingsWidget::createCard);

	menu.exec(m_ui.cardList->mapToGlobal(pos));
}

void MemoryCardSettingsWidget::refresh()
{
	const bool perGame = m_dialog->isPerGameSettings();

	for (u32 slot = 0; slot < static_cast<u32>(m_slots.size()); slot++)
	{
		const bool enabled = m_slots[slot].enable->isChecked();
		const std::string slotKey = getSlotFilenameKey(slot);
		const std::optional<std::string> name(
			m_dialog->getEffectiveStringValue(CONFIG_SECTION, slotKey.c_str(), FileMcd_GetDefaultName(slot).c_str()));
		const bool inherited = perGame ? !m_dialog->containsSettingValue(CONFIG_SECTION, slotKey.c_str()) : false;

		m_slots[slot].slot->setCard(name, inherited);
		m_slots[slot].slot->setEnabled(enabled);
		m_slots[slot].eject->setEnabled(enabled);
	}

	m_ui.cardList->refresh(m_dialog);
	updateCardActions();
}

void MemoryCardSettingsWidget::swapCards()
{
	const std::string card1Key = getSlotFilenameKey(0);
	const std::string card2Key = getSlotFilenameKey(1);
	std::optional<std::string> card1Name = m_dialog->getStringValue(CONFIG_SECTION, card1Key.c_str(), std::nullopt);
	std::optional<std::string> card2Name = m_dialog->getStringValue(CONFIG_SECTION, card2Key.c_str(), std::nullopt);
	if (!card1Name.has_value() || card1Name->empty() || !card2Name.has_value() || card2Name->empty())
	{
		QMessageBox::critical(
			QtUtils::GetRootWidget(this), tr("Error"), tr("Both ports must have a card selected to swap."));
		return;
	}

	m_dialog->setStringSettingValue(CONFIG_SECTION, card1Key.c_str(), card2Name->c_str());
	m_dialog->setStringSettingValue(CONFIG_SECTION, card2Key.c_str(), card1Name->c_str());
	refresh();
}

static QString getSizeSummary(const AvailableMcdInfo& mcd)
{
	if (mcd.type == MemoryCardType::File)
	{
		switch (mcd.file_type)
		{
			case MemoryCardFileType::PS2_8MB:
				return qApp->translate("MemoryCardSettingsWidget", "PS2 (8MB)");

			case MemoryCardFileType::PS2_16MB:
				return qApp->translate("MemoryCardSettingsWidget", "PS2 (16MB)");

			case MemoryCardFileType::PS2_32MB:
				return qApp->translate("MemoryCardSettingsWidget", "PS2 (32MB)");

			case MemoryCardFileType::PS2_64MB:
				return qApp->translate("MemoryCardSettingsWidget", "PS2 (64MB)");

			case MemoryCardFileType::PS1:
				return qApp->translate("MemoryCardSettingsWidget", "PS1 (128KB)");

			case MemoryCardFileType::Unknown:
			default:
				return qApp->translate("MemoryCardSettingsWidget", "Unknown");
		}
	}
	else if (mcd.type == MemoryCardType::Folder)
	{
		return qApp->translate("MemoryCardSettingsWidget", "PS2 (Folder)");
	}
	else
	{
		return qApp->translate("MemoryCardSettingsWidget", "Unknown");
	}
}

static QIcon getCardIcon(const AvailableMcdInfo& mcd)
{
	if (mcd.type == MemoryCardType::File)
		return QIcon::fromTheme(QStringLiteral("memcard-line"));
	else
		return QIcon::fromTheme(QStringLiteral("folder-open-line"));
}

MemoryCardListWidget::MemoryCardListWidget(QWidget* parent)
	: QTreeWidget(parent)
{
}

MemoryCardListWidget::~MemoryCardListWidget() = default;

void MemoryCardListWidget::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
		m_dragStartPos = event->pos();

	QTreeWidget::mousePressEvent(event);
}

void MemoryCardListWidget::mouseMoveEvent(QMouseEvent* event)
{
	if (!(event->buttons() & Qt::LeftButton) ||
		(event->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance())
	{
		QTreeWidget::mouseMoveEvent(event);
		return;
	}

	const QList<QTreeWidgetItem*> selection(selectedItems());
	if (selection.empty())
		return;

	QDrag* drag = new QDrag(this);
	QMimeData* mimeData = new QMimeData();
	mimeData->setText(selection[0]->text(0));
	drag->setMimeData(mimeData);
	drag->exec(Qt::CopyAction);
}

void MemoryCardListWidget::refresh(SettingsWindow* dialog)
{
	clear();

	// we can't use the in use flag here anyway, because the config may not be in line with per game settings.
	const std::vector<AvailableMcdInfo> mcds(FileMcd_GetAvailableCards(true));
	if (mcds.empty())
		return;

	std::array<std::string, MemoryCardSettingsWidget::MAX_SLOTS> currentCards;
	for (u32 i = 0; i < static_cast<u32>(currentCards.size()); i++)
	{
		const std::optional<std::string> filename = dialog->getEffectiveStringValue(
			CONFIG_SECTION, getSlotFilenameKey(i).c_str(), FileMcd_GetDefaultName(i).c_str());
		if (filename.has_value())
			currentCards[i] = std::move(filename.value());
	}

	for (const AvailableMcdInfo& mcd : mcds)
	{
		QTreeWidgetItem* item = new QTreeWidgetItem();
		const QDateTime mtime(QDateTime::fromSecsSinceEpoch(static_cast<qint64>(mcd.modified_time)));
		const bool inUse = (std::find(currentCards.begin(), currentCards.end(), mcd.name) != currentCards.end());

		item->setDisabled(inUse);
		item->setIcon(0, getCardIcon(mcd));
		item->setText(0, QString::fromStdString(mcd.name));
		item->setText(1, getSizeSummary(mcd));
		item->setText(2, mcd.formatted ? tr("Yes") : tr("No"));
		item->setText(3, mtime.toString(QLocale::system().dateTimeFormat(QLocale::ShortFormat)));
		addTopLevelItem(item);
	}
}

MemoryCardSlotWidget::MemoryCardSlotWidget(QWidget* parent)
	: QListWidget(parent)
{
	setAcceptDrops(true);
	setSelectionMode(NoSelection);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

MemoryCardSlotWidget::~MemoryCardSlotWidget() = default;

void MemoryCardSlotWidget::dragEnterEvent(QDragEnterEvent* event)
{
	if (event->mimeData()->hasFormat("text/plain"))
		event->acceptProposedAction();
}

void MemoryCardSlotWidget::dragMoveEvent(QDragMoveEvent* event)
{
}

void MemoryCardSlotWidget::dropEvent(QDropEvent* event)
{
	const QMimeData* data = event->mimeData();
	const QString text(data ? data->text() : QString());
	if (text.isEmpty())
	{
		event->ignore();
		return;
	}

	event->acceptProposedAction();
	emit cardDropped(text);
}

void MemoryCardSlotWidget::setCard(const std::optional<std::string>& name, bool inherited)
{
	clear();
	if (!name.has_value() || name->empty())
		return;

	const std::optional<AvailableMcdInfo> mcd(FileMcd_GetCardInfo(name.value()));
	QListWidgetItem* item = new QListWidgetItem(this);

	if (mcd.has_value())
	{
		item->setIcon(getCardIcon(mcd.value()));
		item->setText(tr("%1 [%2]").arg(QString::fromStdString(mcd->name)).arg(getSizeSummary(mcd.value())));
	}
	else
	{
		item->setIcon(QIcon::fromTheme("close-line"));
		//: Ignore Crowdin's warning for [Missing], the text should be translated.
		item->setText(tr("%1 [Missing]").arg(QString::fromStdString(name.value())));
	}

	if (inherited)
	{
		QFont font = item->font();
		font.setItalic(true);
		item->setFont(font);
		item->setForeground(palette().brush(QPalette::Disabled, QPalette::Text));
	}

	item->setToolTip(item->text());
}
