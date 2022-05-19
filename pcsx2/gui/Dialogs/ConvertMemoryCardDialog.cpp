/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2015  PCSX2 Dev Team
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
#include "ConfigurationDialog.h"
#include "System.h"

#include "gui/MSWstuff.h"

#include "MemoryCardFile.h"
#include "MemoryCardFolder.h"
#include <wx/ffile.h>

enum MemoryCardConversionType {
	MemoryCardConversion_File_8MB,
	MemoryCardConversion_File_16MB,
	MemoryCardConversion_File_32MB,
	MemoryCardConversion_File_64MB,
	MemoryCardConversion_Folder,
	MemoryCardConversion_MaxCount
};

Dialogs::ConvertMemoryCardDialog::ConvertMemoryCardDialog( wxWindow* parent, const wxDirName& mcdPath, MemoryCardType type, const wxString& sourceFilename )
	: wxDialogWithHelpers( parent, _( "Convert a memory card to a different format" ) )
	, m_mcdPath( mcdPath )
	, m_mcdSourceFilename( sourceFilename )
{
	SetMinWidth( 472 * MSW_GetDPIScale());

	CreateControls( type );

	if ( m_radio_CardType ) m_radio_CardType->Realize();

	wxBoxSizer& s_buttons( *new wxBoxSizer( wxHORIZONTAL ) );
	s_buttons += new wxButton( this, wxID_OK, _( "Convert" ) ) | pxProportion( 2 );
	s_buttons += pxStretchSpacer( 3 );
	s_buttons += new wxButton( this, wxID_CANCEL ) | pxProportion( 2 );

	wxBoxSizer& s_padding( *new wxBoxSizer( wxVERTICAL ) );

	s_padding += Heading( wxString( _( "Convert: " ) ) + ( mcdPath + m_mcdSourceFilename ).GetFullPath() ).Unwrapped() | pxSizerFlags::StdExpand();

	wxBoxSizer& s_filename( *new wxBoxSizer( wxHORIZONTAL ) );
	s_filename += Heading( _( "To: " ) ).Unwrapped().Align(wxALIGN_RIGHT) | pxProportion(1);
	m_text_filenameInput->SetValue( wxFileName( m_mcdSourceFilename ).GetName() + L"_converted" );
	s_filename += m_text_filenameInput | pxProportion(2);
	s_filename += Heading( L".ps2" ).Align(wxALIGN_LEFT) | pxProportion(1);

	s_padding += s_filename | pxSizerFlags::StdExpand();

	s_padding += m_radio_CardType | pxSizerFlags::StdExpand();

	if ( type != MemoryCardType::File ) {
		s_padding += Heading( pxE( L"Please note that the resulting file may not actually contain all saves, depending on how many are in the source memory card." ) );
	}
	s_padding += Heading( pxE( L"WARNING: Converting a memory card may take a while! Please do not close the emulator during the conversion process, even if the emulator is no longer responding to input." ) );

	s_padding += 12;
	s_padding += s_buttons | pxSizerFlags::StdCenter();

	*this += s_padding | pxSizerFlags::StdExpand();

	Bind(wxEVT_BUTTON, &ConvertMemoryCardDialog::OnOk_Click, this, wxID_OK);
	Bind(wxEVT_TEXT_ENTER, &ConvertMemoryCardDialog::OnOk_Click, this, m_text_filenameInput->GetId());

	SetSizerAndFit(GetSizer());

	m_text_filenameInput->SetFocus();
	m_text_filenameInput->SelectAll();
}

void Dialogs::ConvertMemoryCardDialog::CreateControls( const MemoryCardType sourceType ) {
	m_text_filenameInput = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER );

	RadioPanelItem toFile8MB = RadioPanelItem( _( "8 MB File (most compatible)" ), pxE( L"Convert this memory card to a standard 8 MB Memory Card .ps2 file." ) )
		.SetInt( MemoryCardConversionType::MemoryCardConversion_File_8MB );
	RadioPanelItem toFile16MB = RadioPanelItem( _( "16 MB File" ), pxE( L"Convert this memory card to a 16 MB Memory Card .ps2 file." ) )
		.SetInt( MemoryCardConversionType::MemoryCardConversion_File_16MB );
	RadioPanelItem toFile32MB = RadioPanelItem( _( "32 MB File" ), pxE( L"Convert this memory card to a 32 MB Memory Card .ps2 file." ) )
		.SetInt( MemoryCardConversionType::MemoryCardConversion_File_32MB );
	RadioPanelItem toFile64MB = RadioPanelItem( _( "64 MB File" ), pxE( L"Convert this memory card to a 64 MB Memory Card .ps2 file." ) )
		.SetInt( MemoryCardConversionType::MemoryCardConversion_File_64MB );
	RadioPanelItem toFolder = RadioPanelItem( _( "Folder" ), _(
		"Convert this memory card to a folder of individual saves. "
		"Unlimited capacity for saving and is not compatible with other PS2 emulators. "
		"Allows direct access to the saves which makes it easy to view and back-up per game." ) )
		.SetInt( MemoryCardConversionType::MemoryCardConversion_Folder );

	const RadioPanelItem tblForFile[] = { toFolder };
	const RadioPanelItem tblForFolder[] = { toFile8MB, toFile16MB, toFile32MB, toFile64MB };

	switch ( sourceType ) {
		case MemoryCardType::File:
			m_radio_CardType = new pxRadioPanel( this, tblForFile );
			break;
		case MemoryCardType::Folder:
			m_radio_CardType = new pxRadioPanel( this, tblForFolder );
			break;
		default:
			Console.Error( "Memory Card Conversion: Invalid source type!" );
			return;
	}

	m_radio_CardType->SetDefaultItem( 0 );
}

void Dialogs::ConvertMemoryCardDialog::OnOk_Click( wxCommandEvent& evt ) {
	wxString composedName = m_text_filenameInput->GetValue().Trim() + L".ps2";

	wxString errMsg;
	if ( !isValidNewFilename( composedName, m_mcdPath, errMsg, 5 ) ) {
		wxString message;
		message.Printf( _( "Error (%s)" ), errMsg.c_str() );
		Msgbox::Alert( message, _( "Convert memory card" ) );
		m_text_filenameInput->SetFocus();
		m_text_filenameInput->SelectAll();
		return;
	}

	bool success = false;

	wxFileName sourcePath = ( m_mcdPath + m_mcdSourceFilename );
	wxFileName targetPath = ( m_mcdPath + composedName );
	if ( m_radio_CardType ) {
		MemoryCardConversionType targetType = (MemoryCardConversionType)m_radio_CardType->SelectedItem().SomeInt;

		switch ( targetType ) {
		case MemoryCardConversionType::MemoryCardConversion_File_8MB:
			success = ConvertToFile( sourcePath, targetPath, 8 );
			break;
		case MemoryCardConversionType::MemoryCardConversion_File_16MB:
			success = ConvertToFile( sourcePath, targetPath, 16 );
			break;
		case MemoryCardConversionType::MemoryCardConversion_File_32MB:
			success = ConvertToFile( sourcePath, targetPath, 32 );
			break;
		case MemoryCardConversionType::MemoryCardConversion_File_64MB:
			success = ConvertToFile( sourcePath, targetPath, 64 );
			break;
		case MemoryCardConversionType::MemoryCardConversion_Folder:
			success = ConvertToFolder( sourcePath, targetPath );
			break;
		default:
			Msgbox::Alert( _( "This target type is not supported!" ), _( "Convert memory card" ) );
			return;
		}
	}

	if ( !success ) {
		Msgbox::Alert( _( "Memory Card conversion failed for unknown reasons." ), _( "Convert memory card" ) );
		return;
	}

	EndModal( wxID_OK );
}

bool Dialogs::ConvertMemoryCardDialog::ConvertToFile( const wxFileName& sourcePath, const wxFileName& targetPath, const u32 sizeInMB ) {
	// Conversion method: Open FolderMcd as usual, then read the raw data from it and write it to a file stream

	wxFFile targetFile( targetPath.GetFullPath(), L"wb" );
	if ( !targetFile.IsOpened() ) {
		return false;
	}

	FolderMemoryCard sourceFolderMemoryCard;
	Pcsx2Config::McdOptions config;
	config.Enabled = true;
	config.Type = MemoryCardType::Folder;
	sourceFolderMemoryCard.Open( StringUtil::wxStringToUTF8String(sourcePath.GetFullPath()), config, ( sizeInMB * 1024 * 1024 ) / FolderMemoryCard::ClusterSize, false, "" );

	u8 buffer[FolderMemoryCard::PageSizeRaw];
	u32 adr = 0;
	while ( adr < sourceFolderMemoryCard.GetSizeInClusters() * FolderMemoryCard::ClusterSizeRaw ) {
		sourceFolderMemoryCard.Read( buffer, adr, FolderMemoryCard::PageSizeRaw );
		targetFile.Write( buffer, FolderMemoryCard::PageSizeRaw );
		adr += FolderMemoryCard::PageSizeRaw;
	}

	targetFile.Close();
	sourceFolderMemoryCard.Close( false );

	return true;
}

bool Dialogs::ConvertMemoryCardDialog::ConvertToFolder( const wxFileName& sourcePath, const wxFileName& targetPath ) {
	// Conversion method: Read all pages of the FileMcd into a FolderMcd, then just write that out with the regular methods

	wxFFile sourceFile( sourcePath.GetFullPath(), L"rb" );
	if ( !sourceFile.IsOpened() ) {
		return false;
	}

	u8 buffer[FolderMemoryCard::PageSizeRaw];
	FolderMemoryCard targetFolderMemoryCard;
	Pcsx2Config::McdOptions config;
	config.Enabled = true;
	config.Type = MemoryCardType::Folder;
	u32 adr = 0;

	for ( int i = 0; i < 2; ++i ) {
		// Before writing the data, we first simulate the entire process without actual writes to the file system.
		// This ensures that if we crash/fail due to a corrupted memory card file system or similar, we do so during
		// the simulation run, and don't actually write out any partial data to the host file system.
		bool simulateWrites = i == 0;
		targetFolderMemoryCard.Open(StringUtil::wxStringToUTF8String(targetPath.GetFullPath()), config, 0, false, "", simulateWrites );

		adr = 0;
		sourceFile.Seek( 0 );
		while ( !sourceFile.Eof() ) {
			int size = sourceFile.Read( buffer, FolderMemoryCard::PageSizeRaw );
			if ( size > 0 ) {
				targetFolderMemoryCard.Save( buffer, adr, size );
				adr += size;
			}
		}

		targetFolderMemoryCard.Close();
	}

	sourceFile.Close();

	if ( adr != FolderMemoryCard::TotalSizeRaw ) {
		// reset memory card metrics in superblock to the default 8MB, since the converted card was different
		targetFolderMemoryCard.Open(StringUtil::wxStringToUTF8String(targetPath.GetFullPath()), config, 0, true, "" );
		targetFolderMemoryCard.SetSizeInMB( 8 );
		targetFolderMemoryCard.Close();
	}

	return true;
}
