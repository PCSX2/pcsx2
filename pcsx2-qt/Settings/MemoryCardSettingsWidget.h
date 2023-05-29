/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <array>
#include <optional>
#include <string>

#include <QtGui/QResizeEvent>
#include <QtWidgets/QWidget>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QListWidget>

class SettingsDialog;

struct AvailableMcdInfo;

class MemoryCardListWidget final : public QTreeWidget
{
	Q_OBJECT
public:
	explicit MemoryCardListWidget(QWidget* parent);
	~MemoryCardListWidget() override;

	void refresh(SettingsDialog* dialog);

protected:
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;

private:
	QPoint m_dragStartPos = {};
};

class MemoryCardSlotWidget final : public QListWidget
{
	Q_OBJECT
public:
	explicit MemoryCardSlotWidget(QWidget* parent);
	~MemoryCardSlotWidget() override;

Q_SIGNALS:
	void cardDropped(const QString& newCard);

public:
	void setCard(const std::optional<std::string>& name, bool inherited);

protected:
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dragMoveEvent(QDragMoveEvent* event) override;
	void dropEvent(QDropEvent* event) override;
};

// Must be included *after* the custom widgets.
#include "ui_MemoryCardSettingsWidget.h"

class MemoryCardSettingsWidget : public QWidget
{
	Q_OBJECT

public:
	enum : u32
	{
		MAX_SLOTS = 2
	};

	MemoryCardSettingsWidget(SettingsDialog* dialog, QWidget* parent);
	~MemoryCardSettingsWidget();

protected:
	void resizeEvent(QResizeEvent* event);
	bool eventFilter(QObject* watched, QEvent* event);

private Q_SLOTS:
	void listContextMenuRequested(const QPoint& pos);
	void refresh();
	void swapCards();

private:
	struct SlotGroup
	{
		QWidget* root;
		QCheckBox* enable;
		QToolButton* eject;
		MemoryCardSlotWidget* slot;
	};

	void createSlotWidgets(SlotGroup* port, u32 slot);
	void setupAdditionalUi();
	void autoSizeUI();

	void tryInsertCard(u32 slot, const QString& newCard);
	void ejectSlot(u32 slot);

	void createCard();

	QString getSelectedCard() const;
	void updateCardActions();
	void duplicateCard();
	void deleteCard();
	void renameCard();
	void convertCard();

	SettingsDialog* m_dialog;
	Ui::MemoryCardSettingsWidget m_ui;

	std::array<SlotGroup, MAX_SLOTS> m_slots;
};
