// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GuardedDialog.h"

#include <QtWidgets/QPushButton>

// Based on the QMessageBoxPrivate::showNewMessageBox function from Qt Widgets.
static QMessageBox::StandardButton showGuardedMessageBox(QWidget* parent,
	QMessageBox::Icon icon,
	const QString& title, const QString& text,
	QMessageBox::StandardButtons buttons,
	QMessageBox::StandardButton default_button)
{
	// Don't support the fallback for Qt 4.
	pxAssert(!(default_button && !(buttons & default_button)));

	GuardedDialog<QMessageBox> message_box(icon, title, text, QMessageBox::NoButton, parent);
	QDialogButtonBox* button_box = message_box->findChild<QDialogButtonBox*>();
	pxAssert(button_box);

	u32 mask = QMessageBox::FirstButton;
	while (mask <= QMessageBox::LastButton)
	{
		QMessageBox::StandardButton standard_button =
			static_cast<QMessageBox::StandardButton>(static_cast<u32>(buttons) & mask);

		mask <<= 1;

		if (standard_button == QMessageBox::NoButton)
			continue;

		QPushButton* button = message_box->addButton(standard_button);

		if (message_box->defaultButton())
			continue;

		if ((default_button == QMessageBox::NoButton &&
				button_box->buttonRole(button) == QDialogButtonBox::AcceptRole) ||
			(default_button != QMessageBox::NoButton &&
				standard_button == default_button))
			message_box->setDefaultButton(button);
	}

	if (!message_box.execute().has_value())
		return QMessageBox::Cancel;

	return message_box->standardButton(message_box->clickedButton());
}

GuardedMessageBox::Button GuardedMessageBox::information(
	QWidget* parent, const QString& title,
	const QString& text, Buttons buttons,
	Button default_button)
{
	return showGuardedMessageBox(parent, QMessageBox::Information, title, text, buttons, default_button);
}

GuardedMessageBox::Button GuardedMessageBox::question(
	QWidget* parent, const QString& title,
	const QString& text, Buttons buttons,
	Button default_button)
{
	return showGuardedMessageBox(parent, QMessageBox::Question, title, text, buttons, default_button);
}

GuardedMessageBox::Button GuardedMessageBox::warning(
	QWidget* parent, const QString& title,
	const QString& text, Buttons buttons,
	Button default_button)
{
	return showGuardedMessageBox(parent, QMessageBox::Warning, title, text, buttons, default_button);
}

GuardedMessageBox::Button GuardedMessageBox::critical(
	QWidget* parent, const QString& title,
	const QString& text, Buttons buttons,
	Button default_button)
{
	return showGuardedMessageBox(parent, QMessageBox::Critical, title, text, buttons, default_button);
}
