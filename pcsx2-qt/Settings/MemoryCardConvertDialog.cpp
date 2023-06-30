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

#include "PrecompiledHeader.h"

#include "MemoryCardConvertDialog.h"

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QProgressDialog>

#include "common/Path.h"
#include "common/StringUtil.h"

#include "pcsx2/System.h"

MemoryCardConvertDialog::MemoryCardConvertDialog(QWidget* parent, QString selectedCard)
	: QDialog(parent)
{
	m_ui.setupUi(this);

	// For some reason, setting these in the .ui doesn't work..
	m_ui.conversionTypeDescription->setFrameStyle(QFrame::Sunken);
	m_ui.conversionTypeDescription->setFrameShape(QFrame::WinPanel);
	m_ui.note->setFrameStyle(QFrame::Sunken);
	m_ui.note->setFrameShape(QFrame::WinPanel);

	m_selectedCard = selectedCard;
	std::optional<AvailableMcdInfo> srcCardInfo = FileMcd_GetCardInfo(m_selectedCard.toStdString());

	if (srcCardInfo.has_value())
	{
		m_srcCardInfo = srcCardInfo.value();
	}

	isSetup = SetupPicklist();
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	m_ui.progressBar->setRange(0, 100);
	m_ui.progressBar->setValue(0);

	connect(m_ui.conversionTypeSelect, &QComboBox::currentIndexChanged, this, [this]() 
		{
			switch (m_srcCardInfo.type)
			{
				case MemoryCardType::File:
					SetType(MemoryCardType::Folder, MemoryCardFileType::Unknown, tr("Uses a folder on your PC filesystem, instead of a file. Infinite capacity, while keeping the same compatibility as an 8 MB Memory Card."));
					break;
				case MemoryCardType::Folder:
					switch (m_ui.conversionTypeSelect->currentData().toInt())
					{
						case 8:
							SetType(MemoryCardType::File, MemoryCardFileType::PS2_8MB, tr("A standard, 8 MB Memory Card. Most compatible, but smallest capacity."));
							break;
						case 16:
							SetType(MemoryCardType::File, MemoryCardFileType::PS2_16MB, tr("2x larger than a standard Memory Card. May have some compatibility issues."));
							break;
						case 32:
							SetType(MemoryCardType::File, MemoryCardFileType::PS2_32MB, tr("4x larger than a standard Memory Card. Likely to have compatibility issues."));
							break;
						case 64:
							SetType(MemoryCardType::File, MemoryCardFileType::PS2_64MB, tr("8x larger than a standard Memory Card. Likely to have compatibility issues."));
							break;
						default:
							//: MemoryCardType should be left as-is.
							QMessageBox::critical(this, tr("Convert Memory Card Failed"), tr("Invalid MemoryCardType"));
							return;
					}
					break;
				default:
					//: MemoryCardType should be left as-is.
					QMessageBox::critical(this, tr("Convert Memory Card Failed"), tr("Invalid MemoryCardType"));
					return;
			}
		}
	);
	
	disconnect(m_ui.buttonBox, &QDialogButtonBox::accepted, this, nullptr);

	connect(m_ui.buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &MemoryCardConvertDialog::ConvertCard);
	connect(m_ui.buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &MemoryCardConvertDialog::close);
}

MemoryCardConvertDialog::~MemoryCardConvertDialog() = default;

bool MemoryCardConvertDialog::IsSetup()
{
	return isSetup;
}

void MemoryCardConvertDialog::onStatusUpdated()
{

}

void MemoryCardConvertDialog::onProgressUpdated(int value, int range)
{
	m_ui.progressBar->setRange(0, range);
	m_ui.progressBar->setValue(value);
}

void MemoryCardConvertDialog::onThreadFinished()
{
	QMessageBox::information(this, tr("Conversion Complete"), tr("Memory Card \"%1\" converted to \"%2\"").arg(m_selectedCard).arg(m_destCardName));
	accept();
}

void MemoryCardConvertDialog::StartThread()
{
	m_thread = std::make_unique<MemoryCardConvertWorker>(this, m_srcCardInfo.type, m_fileType, m_selectedCard.toStdString(), m_destCardName.toStdString());
	connect(m_thread.get(), &MemoryCardConvertWorker::statusUpdated, this, &MemoryCardConvertDialog::onStatusUpdated);
	connect(m_thread.get(), &MemoryCardConvertWorker::progressUpdated, this, &MemoryCardConvertDialog::onProgressUpdated);
	connect(m_thread.get(), &MemoryCardConvertWorker::threadFinished, this, &MemoryCardConvertDialog::onThreadFinished);
	m_thread->start();
	UpdateEnabled();
}

void MemoryCardConvertDialog::CancelThread()
{
	if (!m_thread)
	{
		return;
	}

	m_thread->requestInterruption();
	m_thread->join();
	m_thread.reset();
}

void MemoryCardConvertDialog::UpdateEnabled()
{
	if (m_thread)
	{
		m_ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
		m_ui.buttonBox->button(QDialogButtonBox::Cancel)->setEnabled(false);
	}
}

bool MemoryCardConvertDialog::SetupPicklist()
{
	FileSystem::FindResultsArray rootDir;
	size_t sizeBytes = 0;
	bool typeSet = false;

	m_ui.conversionTypeSelect->clear();

	switch (m_srcCardInfo.type)
	{
		case MemoryCardType::File:
			m_ui.conversionTypeSelect->addItems({"Folder"});
			SetType(MemoryCardType::Folder, MemoryCardFileType::Unknown, tr("Uses a folder on your PC filesystem, instead of a file. Infinite capacity, while keeping the same compatibility as an 8 MB Memory Card."));
			break;
		case MemoryCardType::Folder:
			// Compute which file types should be allowed.
			FileSystem::FindFiles(m_srcCardInfo.path.c_str(), "*", FLAGS, &rootDir);

			for (auto dirEntry : rootDir)
			{
				const std::string_view fileName = Path::GetFileName(dirEntry.FileName);
				
				if (fileName.size() >= 7 && fileName.substr(0, 7).compare("_pcsx2_") == 0)
				{
					continue;
				}
				else if (dirEntry.Attributes & FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY)
				{
					sizeBytes += 512;
				}
				else
				{
					size_t toAdd = static_cast<size_t>(dirEntry.Size + (1024 - (dirEntry.Size % 1024)));
					sizeBytes += toAdd + 512; // The file content needs to be added, PLUS a directory entry
				}
			}

			// Finally, round up to the nearest erase block.
			sizeBytes += (512 * 16) - (sizeBytes % (512 * 16));

			if (sizeBytes < CardCapacity::_8_MB)
			{
				m_ui.conversionTypeSelect->addItem(tr("8 MB File"), 8);
				
				if (!typeSet)
				{
					SetType_8();
					typeSet = true;
				}
			}

			if (sizeBytes < CardCapacity::_16_MB)
			{
				m_ui.conversionTypeSelect->addItem(tr("16 MB File"), 16);
				
				if (!typeSet)
				{
					SetType_16();
					typeSet = true;
				}
			}

			if (sizeBytes < CardCapacity::_32_MB)
			{
				m_ui.conversionTypeSelect->addItem(tr("32 MB File"), 32);
				
				if (!typeSet)
				{
					SetType_32();
					typeSet = true;
				}
			}

			if (sizeBytes < CardCapacity::_64_MB)
			{
				m_ui.conversionTypeSelect->addItem(tr("64 MB File"), 64);
				
				if (!typeSet)
				{
					SetType_64();
					typeSet = true;
				}
			}

			if (!typeSet)
			{
				QMessageBox::critical(this, tr("Cannot Convert Memory Card"), tr("Your folder Memory Card has too much data inside it to be converted to a file Memory Card. The largest supported file Memory Card has a capacity of 64 MB. To convert your folder Memory Card, you must remove game folders until its size is 64 MB or less."));
				return false;
			}

			break;
		default:
			//: MemoryCardType should be left as-is.
			QMessageBox::critical(this, tr("Convert Memory Card Failed"), tr("Invalid MemoryCardType"));
			return false;
	}

	return true;
}

void MemoryCardConvertDialog::ConvertCard()
{
	if (m_thread)
	{
		CancelThread();
	}
	else
	{
		QString baseName = m_selectedCard;
		
		// Get our destination file name
		size_t extensionPos = baseName.lastIndexOf(".ps2", -1);
		// Strip the extension off of it
		baseName.replace(extensionPos, 4, "");
		// Add _converted to the end of it
		baseName.append("_converted");
		
		size_t num = 0;
		QString destName = baseName;
		destName.append(".ps2");
		
		// If a match is found, revert back to the base name, add a number and the extension, and try again.
		// Keep incrementing the number until we get a unique result.
		while (m_srcCardInfo.type == MemoryCardType::File ? FileSystem::DirectoryExists(Path::Combine(EmuFolders::MemoryCards, destName.toStdString()).c_str()) : FileSystem::FileExists(Path::Combine(EmuFolders::MemoryCards, destName.toStdString()).c_str()))
		{
			destName = baseName;
			destName.append(StringUtil::StdStringFromFormat("_%02d.ps2", ++num).c_str());
		}
		
		m_destCardName = destName;
		StartThread();
	}
}

void MemoryCardConvertDialog::ConvertCallback()
{
	Console.WriteLn("%s() Finished", __FUNCTION__);
}

void MemoryCardConvertDialog::SetType(MemoryCardType type, MemoryCardFileType fileType, const QString& description)
{
	m_type = type;
	m_fileType = fileType;
	m_ui.conversionTypeDescription->setText(QStringLiteral("<center>%1</center>").arg(description));
}

void MemoryCardConvertDialog::SetType_8()
{
	SetType(MemoryCardType::File, MemoryCardFileType::PS2_8MB, tr("A standard, 8 MB Memory Card. Most compatible, but smallest capacity."));
}

void MemoryCardConvertDialog::SetType_16()
{
	SetType(MemoryCardType::File, MemoryCardFileType::PS2_16MB, tr("2x larger as a standard Memory Card. May have some compatibility issues."));
}

void MemoryCardConvertDialog::SetType_32()
{
	SetType(MemoryCardType::File, MemoryCardFileType::PS2_32MB, tr("4x larger than a standard Memory Card. Likely to have compatibility issues."));
}

void MemoryCardConvertDialog::SetType_64()
{
	SetType(MemoryCardType::File, MemoryCardFileType::PS2_64MB, tr("8x larger than a standard Memory Card. Likely to have compatibility issues."));
}

void MemoryCardConvertDialog::SetType_Folder()
{
	SetType(MemoryCardType::Folder, MemoryCardFileType::Unknown, tr("Uses a folder on your PC filesystem, instead of a file. Infinite capacity, while keeping the same compatibility as an 8 MB Memory Card."));
}
