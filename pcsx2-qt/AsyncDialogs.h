// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>

#include <functional>

/// A collection of helper functions for opening asynchronous dialog boxes.
/// These should be used in place of Qt's built-in blocking functions as those
/// are prone to crashing if the parent is destroyed while the dialog is open.
namespace AsyncDialogs
{
	/// Replacement for QInputDialog::getText.
	void getText(
		QWidget* parent,
		const QString& title,
		const QString& label,
		const QString& text,
		std::function<void(QString)> value_callback);

	/// Replacement for QInputDialog::getText.
	void getText(
		QWidget* parent,
		const QString& title,
		const QString& label,
		QLineEdit::EchoMode echo,
		const QString& text,
		Qt::WindowFlags flags,
		Qt::InputMethodHints input_method_hints,
		std::function<void(std::optional<QString>)> callback);

	/// Replacement for QInputDialog::getMultiLineText.
	void getMultiLineText(
		QWidget* parent,
		const QString& title,
		const QString& label,
		const QString& text,
		std::function<void(QString)> value_callback);

	/// Replacement for QInputDialog::getMultiLineText.
	void getMultiLineText(
		QWidget* parent,
		const QString& title,
		const QString& label,
		const QString& text,
		Qt::WindowFlags flags,
		Qt::InputMethodHints input_method_hints,
		std::function<void(std::optional<QString>)> callback);

	/// Replacement for QInputDialog::getItem.
	void getItem(
		QWidget* parent,
		const QString& title,
		const QString& label,
		const QStringList& items,
		int current,
		std::function<void(QString)> value_callback);

	/// Replacement for QInputDialog::getItem.
	void getItem(
		QWidget* parent,
		const QString& title,
		const QString& label,
		const QStringList& items,
		int current,
		bool editable,
		Qt::WindowFlags flags,
		Qt::InputMethodHints input_method_hints,
		std::function<void(std::optional<QString>)> callback);

	/// Replacement for QInputDialog::getInt.
	void getInt(
		QWidget* parent,
		const QString& title,
		const QString& label,
		int value,
		std::function<void(int)> value_callback);

	/// Replacement for QInputDialog::getInt.
	void getInt(
		QWidget* parent,
		const QString& title,
		const QString& label,
		int value,
		int min_value,
		int max_value,
		int step,
		Qt::WindowFlags flags,
		std::function<void(std::optional<int>)> callback);

	/// Replacement for QInputDialog::getDouble.
	void getDouble(
		QWidget* parent,
		const QString& title,
		const QString& label,
		double value,
		std::function<void(double)> value_callback);

	/// Replacement for QInputDialog::getDouble.
	void getDouble(
		QWidget* parent,
		const QString& title,
		const QString& label,
		double value,
		double min_value,
		double max_value,
		int decimals,
		Qt::WindowFlags flags,
		double step,
		std::function<void(std::optional<double>)> callback);

	/// Replacement for QMessageBox::information.
	void information(
		QWidget* parent,
		const QString& title,
		const QString& text,
		std::function<void(QMessageBox::StandardButton)> callback = {});

	/// Replacement for QMessageBox::information.
	void information(
		QWidget* parent,
		const QString& title,
		const QString& text,
		QMessageBox::StandardButtons buttons,
		QMessageBox::StandardButton default_button,
		std::function<void(QMessageBox::StandardButton)> callback = {});

	/// Replacement for QMessageBox::question.
	void question(
		QWidget* parent,
		const QString& title,
		const QString& text,
		std::function<void()> yes_callback);

	/// Replacement for QMessageBox::question.
	void question(
		QWidget* parent,
		const QString& title,
		const QString& text,
		QMessageBox::StandardButtons buttons,
		QMessageBox::StandardButton default_button,
		std::function<void(QMessageBox::StandardButton)> callback);

	/// Replacement for QMessageBox::warning.
	void warning(
		QWidget* parent,
		const QString& title,
		const QString& text,
		std::function<void(QMessageBox::StandardButton)> callback = {});

	/// Replacement for QMessageBox::warning.
	void warning(
		QWidget* parent,
		const QString& title,
		const QString& text,
		QMessageBox::StandardButtons buttons,
		QMessageBox::StandardButton default_button,
		std::function<void(QMessageBox::StandardButton)> callback = {});

	/// Replacement for QMessageBox::critical.
	void critical(
		QWidget* parent,
		const QString& title,
		const QString& text,
		std::function<void(QMessageBox::StandardButton)> callback = {});

	/// Replacement for QMessageBox::critical.
	void critical(
		QWidget* parent,
		const QString& title,
		const QString& text,
		QMessageBox::StandardButtons buttons,
		QMessageBox::StandardButton default_button,
		std::function<void(QMessageBox::StandardButton)> callback = {});
} // namespace AsyncDialogs
