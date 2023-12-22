// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include <QtWidgets/QMessageBox>
#include "HddCreateQt.h"

HddCreateQt::HddCreateQt(QWidget* parent)
	: m_parent{parent}
	, progressDialog{nullptr}
{
}

void HddCreateQt::Init()
{
	reqMiB = (neededSize + ((1024 * 1024) - 1)) / (1024 * 1024);

	progressDialog = new QProgressDialog(QObject::tr("Creating HDD file \n %1 / %2 MiB").arg(0).arg(reqMiB), QObject::tr("Cancel"), 0, reqMiB, m_parent);
	progressDialog->setWindowTitle("HDD Creator");
	progressDialog->setWindowModality(Qt::WindowModal);
}

void HddCreateQt::SetFileProgress(u64 currentSize)
{
	const int writtenMB = (currentSize + ((1024 * 1024) - 1)) / (1024 * 1024);
	progressDialog->setValue(writtenMB);
	progressDialog->setLabelText(QObject::tr("Creating HDD file \n %1 / %2 MiB").arg(writtenMB).arg(reqMiB));

	if (progressDialog->wasCanceled())
		SetCanceled();
}

void HddCreateQt::SetError()
{
	QMessageBox::warning(progressDialog, QObject::tr("HDD Creator"),
		QObject::tr("Failed to create HDD image"),
		QMessageBox::StandardButton::Ok, QMessageBox::StandardButton::Ok);
}

void HddCreateQt::Cleanup()
{
	delete progressDialog;
}
