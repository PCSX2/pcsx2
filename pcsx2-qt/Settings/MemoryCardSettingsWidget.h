// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <array>
#include <optional>
#include <string>

#include <QtGui/QResizeEvent>
#include <QtWidgets/QWidget>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QListWidget>

class SettingsWindow;

struct AvailableMcdInfo;

class MemoryCardListWidget final : public QTreeWidget
{
	Q_OBJECT
public:
	explicit MemoryCardListWidget(QWidget* parent);
	~MemoryCardListWidget() override;

	void refresh(SettingsWindow* dialog);

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

	MemoryCardSettingsWidget(SettingsWindow* dialog, QWidget* parent);
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

	SettingsWindow* m_dialog;
	Ui::MemoryCardSettingsWidget m_ui;

	std::array<SlotGroup, MAX_SLOTS> m_slots;
};
