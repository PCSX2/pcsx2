/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include "PrecompiledHeader.h"
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
