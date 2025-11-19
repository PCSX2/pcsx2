// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Assertions.h"

#include <QtCore/QPointer>
#include <QtWidgets/QDialog>
#include <QtWidgets/QMessageBox>

#include <type_traits>

/// Helper class template for creating blocking modal dialogs that won't crash
/// when they are destroyed from within their exec function. Such dialogs must
/// be created in a way that satisfies the following conditions:
///
///  - They must be allocated on the heap (so that delete is never called on
///    stack memory).
///  - They must be referenced using a QPointer (so that you won't try to use
///    a dangling pointer).
///  - You must check if they have been destroyed immediately upon returning
///    from their exec function.
///
/// This is safe because QDialog::exec uses a QPointer internally to return
/// immediately if the dialog was destroyed, and destroying an object from
/// within one of its own member functions is well-defined behaviour in C++:
///
///  - https://github.com/qt/qtbase/blob/6.10.0/src/widgets/dialogs/qdialog.cpp#L569-L578
///  - https://isocpp.org/wiki/faq/freestore-mgmt#delete-this
///
/// That said, if you are creating new dialogs, consider making them
/// asynchronous by using QDialog::open instead.
template <typename Dialog>
	requires std::is_base_of_v<QDialog, Dialog>
class GuardedDialog
{
public:
	/// Construct a new dialog of the specified type on the heap.
	template <typename... Args>
	GuardedDialog(Args&&... args)
		: m_dialog(new Dialog(std::forward<Args>(args)...))
	{
	}

	/// Delete the dialog when we're done.
	~GuardedDialog()
	{
		delete m_dialog.get();
	}

	/// Don't allow copying.
	GuardedDialog(const GuardedDialog<Dialog>&) = delete;
	GuardedDialog<Dialog>& operator=(const GuardedDialog<Dialog>&) = delete;

	/// Allow moving.
	GuardedDialog(GuardedDialog<Dialog>&& rhs) = default;
	GuardedDialog<Dialog>& operator=(GuardedDialog<Dialog>&& rhs) = default;

	/// Assume that the dialog hasn't been destroyed and dereference it.
	Dialog* operator->()
	{
		pxAssert(m_dialog.get());
		return m_dialog.get();
	}

	/// Assume that the dialog hasn't been destroyed and dereference it.
	const Dialog* operator->() const
	{
		pxAssert(m_dialog.get());
		return m_dialog.get();
	}

	/// Assume that the dialog hasn't been destroyed and return a pointer to it.
	Dialog* get()
	{
		pxAssert(m_dialog.get());
		return m_dialog.get();
	}

	/// Assume that the dialog hasn't been destroyed and return a constant
	/// pointer to it.
	const Dialog* get() const
	{
		pxAssert(m_dialog.get());
		return m_dialog.get();
	}

	/// Check that the dialog hasn't been destroyed.
	bool valid() const
	{
		return m_dialog.get();
	}

	/// Run the dialog's exec function and block until it is dismissed, creating
	/// a new event loop to process events in the meantime. If the returned
	/// value is std::nullopt it means that the dialog was destroyed.
	std::optional<int> execute()
	{
		if (!m_dialog.get())
			return std::nullopt;

		int result = m_dialog->exec();

		if (!m_dialog.get())
		{
			// The dialog was destroyed from inside its own exec function.
			return std::nullopt;
		}

		return result;
	}

private:
	QPointer<Dialog> m_dialog;
};

/// Helper functions to create simple message boxes, that should be used in
/// place of the QMessageBox ones (specifically the static member functions
/// information, question, warning, and critical). Those functions are
/// problematic since they allocate their message boxes on the stack (see the
/// comment at the top of this file for more information).
///
/// This is just a problem for QMessageBox. The static helper functions on the
/// other built-in dialog classes (QColorDialog, QFileDialog, QFontDialog, and
/// QInputDialog) are fine.
namespace GuardedMessageBox
{
	using Button = QMessageBox::StandardButton;
	using Buttons = QMessageBox::StandardButtons;

	/// Opens an information message box. Replacement for QMessageBox::information.
	Button information(QWidget* parent, const QString& title,
		const QString& text, Buttons buttons = Button::Ok,
		Button default_button = Button::NoButton);

	/// Opens a question message box. Replacement for QMessageBox::question.
	Button question(QWidget* parent, const QString& title,
		const QString& text, Buttons buttons = Buttons(Button::Yes | Button::No),
		Button default_button = Button::NoButton);

	/// Opens a warning message box. Replacement for QMessageBox::warning.
	Button warning(QWidget* parent, const QString& title,
		const QString& text, Buttons buttons = Button::Ok,
		Button default_button = Button::NoButton);

	/// Opens a critical message box. Replacement for QMessageBox::critical.
	Button critical(QWidget* parent, const QString& title,
		const QString& text, Buttons buttons = Button::Ok,
		Button default_button = Button::NoButton);
} // namespace GuardedMessageBox
