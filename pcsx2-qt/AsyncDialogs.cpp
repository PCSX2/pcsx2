// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "AsyncDialogs.h"

#include "common/Assertions.h"

#include <QtWidgets/QPushButton>

template <typename Value>
static std::function<void(std::optional<Value>)> wrapValueCallback(std::function<void(Value)> callback)
{
	return [callback = std::move(callback)](std::optional<Value> input) {
		if (input.has_value())
			callback(std::move(*input));
	};
}

void AsyncDialogs::getText(
	QWidget* parent,
	const QString& title,
	const QString& label,
	const QString& text,
	std::function<void(QString)> value_callback)
{
	getText(parent, title, label, QLineEdit::Normal, text, {}, Qt::ImhNone, wrapValueCallback(std::move(value_callback)));
}

void AsyncDialogs::getText(
	QWidget* parent,
	const QString& title,
	const QString& label,
	QLineEdit::EchoMode echo,
	const QString& text,
	Qt::WindowFlags flags,
	Qt::InputMethodHints input_method_hints,
	std::function<void(std::optional<QString>)> callback)
{
	QInputDialog* dialog = new QInputDialog(parent, flags);
	dialog->setWindowTitle(title);
	dialog->setLabelText(label);
	dialog->setTextValue(text);
	dialog->setTextEchoMode(echo);
	dialog->setInputMethodHints(input_method_hints);
	dialog->setAttribute(Qt::WA_DeleteOnClose);

	if (callback)
	{
		QObject::connect(dialog, &QDialog::finished, [dialog, callback = std::move(callback)](int result) {
			callback(result ? std::make_optional<QString>(dialog->textValue()) : std::nullopt);
		});
	}

	dialog->open();
}

void AsyncDialogs::getMultiLineText(
	QWidget* parent,
	const QString& title,
	const QString& label,
	const QString& text,
	std::function<void(QString)> value_callback)
{
	getMultiLineText(parent, title, label, text, {}, Qt::ImhNone, wrapValueCallback(std::move(value_callback)));
}

void AsyncDialogs::getMultiLineText(
	QWidget* parent,
	const QString& title,
	const QString& label,
	const QString& text,
	Qt::WindowFlags flags,
	Qt::InputMethodHints input_method_hints,
	std::function<void(std::optional<QString>)> callback)
{
	QInputDialog* dialog = new QInputDialog(parent, flags);
	dialog->setOptions(QInputDialog::UsePlainTextEditForTextInput);
	dialog->setWindowTitle(title);
	dialog->setLabelText(label);
	dialog->setTextValue(text);
	dialog->setInputMethodHints(input_method_hints);
	dialog->setAttribute(Qt::WA_DeleteOnClose);

	if (callback)
	{
		QObject::connect(dialog, &QDialog::finished, [dialog, callback = std::move(callback)](int result) {
			callback(result ? std::make_optional<QString>(dialog->textValue()) : std::nullopt);
		});
	}

	dialog->open();
}

void AsyncDialogs::getItem(
	QWidget* parent,
	const QString& title,
	const QString& label,
	const QStringList& items,
	int current,
	std::function<void(QString)> value_callback)
{
	getItem(parent, title, label, items, current, true, {}, Qt::ImhNone, wrapValueCallback(std::move(value_callback)));
}

void AsyncDialogs::getItem(
	QWidget* parent,
	const QString& title,
	const QString& label,
	const QStringList& items,
	int current,
	bool editable,
	Qt::WindowFlags flags,
	Qt::InputMethodHints input_method_hints,
	std::function<void(std::optional<QString>)> callback)
{
	const QString text(items.value(current));

	QInputDialog* dialog = new QInputDialog(parent, flags);
	dialog->setWindowTitle(title);
	dialog->setLabelText(label);
	dialog->setComboBoxItems(items);
	dialog->setTextValue(text);
	dialog->setComboBoxEditable(editable);
	dialog->setInputMethodHints(input_method_hints);
	dialog->setAttribute(Qt::WA_DeleteOnClose);

	if (callback)
	{
		QObject::connect(dialog, &QDialog::finished, [dialog, callback = std::move(callback)](int result) {
			callback(result ? std::make_optional<QString>(dialog->textValue()) : std::nullopt);
		});
	}

	dialog->open();
}

void AsyncDialogs::getInt(
	QWidget* parent,
	const QString& title,
	const QString& label,
	int value,
	std::function<void(int)> value_callback)
{
	getInt(parent, title, label, value, -2147483647, 2147483647, 1, {}, wrapValueCallback(std::move(value_callback)));
}

void AsyncDialogs::getInt(
	QWidget* parent,
	const QString& title,
	const QString& label,
	int value,
	int min_value,
	int max_value,
	int step,
	Qt::WindowFlags flags,
	std::function<void(std::optional<int>)> callback)
{
	QInputDialog* dialog = new QInputDialog(parent, flags);
	dialog->setWindowTitle(title);
	dialog->setLabelText(label);
	dialog->setIntRange(min_value, max_value);
	dialog->setIntValue(value);
	dialog->setIntStep(step);
	dialog->setAttribute(Qt::WA_DeleteOnClose);

	if (callback)
	{
		QObject::connect(dialog, &QDialog::finished, [dialog, callback = std::move(callback)](int result) {
			callback(result ? std::make_optional<int>(dialog->intValue()) : std::nullopt);
		});
	}

	dialog->open();
}

void AsyncDialogs::getDouble(
	QWidget* parent,
	const QString& title,
	const QString& label,
	double value,
	std::function<void(double)> callback)
{
	getDouble(parent, title, label, value, -2147483647, 2147483647, 1, {}, 1, wrapValueCallback(std::move(callback)));
}

void AsyncDialogs::getDouble(
	QWidget* parent,
	const QString& title,
	const QString& label,
	double value,
	double min_value,
	double max_value,
	int decimals,
	Qt::WindowFlags flags,
	double step,
	std::function<void(std::optional<double>)> callback)
{
	QInputDialog* dialog = new QInputDialog(parent, flags);
	dialog->setWindowTitle(title);
	dialog->setLabelText(label);
	dialog->setDoubleDecimals(decimals);
	dialog->setDoubleRange(min_value, max_value);
	dialog->setDoubleValue(value);
	dialog->setDoubleStep(step);
	dialog->setAttribute(Qt::WA_DeleteOnClose);

	if (callback)
	{
		QObject::connect(dialog, &QDialog::finished, [dialog, callback = std::move(callback)](int result) {
			callback(result ? std::make_optional<double>(dialog->doubleValue()) : std::nullopt);
		});
	}

	dialog->open();
}

// Based on the QMessageBoxPrivate::showNewMessageBox function from Qt Widgets.
static void openAsyncMessageBox(QWidget* parent,
	QMessageBox::Icon icon,
	const QString& title,
	const QString& text,
	QMessageBox::StandardButtons buttons,
	QMessageBox::StandardButton default_button,
	std::function<void(QMessageBox::StandardButton)> callback)
{
	// Don't support the fallback for Qt 4.
	pxAssert(!(default_button && !(buttons & default_button)));

	QMessageBox* message_box = new QMessageBox(icon, title, text, QMessageBox::NoButton, parent);
	message_box->setAttribute(Qt::WA_DeleteOnClose);

	const QDialogButtonBox* button_box = message_box->findChild<QDialogButtonBox*>();
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

	if (callback)
	{
		QObject::connect(message_box, &QMessageBox::finished, [message_box, callback = std::move(callback)](int result) {
			callback(message_box->standardButton(message_box->clickedButton()));
		});
	}

	message_box->open();
}

void AsyncDialogs::information(
	QWidget* parent,
	const QString& title,
	const QString& text,
	std::function<void(QMessageBox::StandardButton)> callback)
{
	information(parent, title, text, QMessageBox::Ok, QMessageBox::NoButton, std::move(callback));
}

void AsyncDialogs::information(
	QWidget* parent,
	const QString& title,
	const QString& text,
	QMessageBox::StandardButtons buttons,
	QMessageBox::StandardButton default_button,
	std::function<void(QMessageBox::StandardButton)> callback)
{
	openAsyncMessageBox(parent, QMessageBox::Information, title, text, buttons, default_button, std::move(callback));
}

void AsyncDialogs::question(
	QWidget* parent,
	const QString& title,
	const QString& text,
	std::function<void()> yes_callback)
{
	std::function<void(QMessageBox::StandardButton)> callback;
	if (yes_callback)
	{
		callback = [yes_callback = std::move(yes_callback)](QMessageBox::StandardButton result) {
			if (result == QMessageBox::Yes)
				yes_callback();
		};
	}

	const QMessageBox::StandardButtons buttons(QMessageBox::Yes | QMessageBox::No);
	question(parent, title, text, buttons, QMessageBox::NoButton, std::move(callback));
}

void AsyncDialogs::question(
	QWidget* parent,
	const QString& title,
	const QString& text,
	QMessageBox::StandardButtons buttons,
	QMessageBox::StandardButton default_button,
	std::function<void(QMessageBox::StandardButton)> callback)
{
	openAsyncMessageBox(parent, QMessageBox::Question, title, text, buttons, default_button, std::move(callback));
}

void AsyncDialogs::warning(
	QWidget* parent,
	const QString& title,
	const QString& text,
	std::function<void(QMessageBox::StandardButton)> callback)
{
	warning(parent, title, text, QMessageBox::Ok, QMessageBox::NoButton, std::move(callback));
}

void AsyncDialogs::warning(
	QWidget* parent,
	const QString& title,
	const QString& text,
	QMessageBox::StandardButtons buttons,
	QMessageBox::StandardButton default_button,
	std::function<void(QMessageBox::StandardButton)> callback)
{
	openAsyncMessageBox(parent, QMessageBox::Warning, title, text, buttons, default_button, std::move(callback));
}

void AsyncDialogs::critical(
	QWidget* parent,
	const QString& title,
	const QString& text,
	std::function<void(QMessageBox::StandardButton)> callback)
{
	critical(parent, title, text, QMessageBox::Ok, QMessageBox::NoButton, std::move(callback));
}

void AsyncDialogs::critical(
	QWidget* parent,
	const QString& title,
	const QString& text,
	QMessageBox::StandardButtons buttons,
	QMessageBox::StandardButton default_button,
	std::function<void(QMessageBox::StandardButton)> callback)
{
	openAsyncMessageBox(parent, QMessageBox::Critical, title, text, buttons, default_button, std::move(callback));
}
