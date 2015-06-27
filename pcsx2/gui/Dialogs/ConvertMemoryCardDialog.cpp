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

#include "MemoryCardFile.h"
#include "MemoryCardFolder.h"
#include <wx/ffile.h>

Dialogs::ConvertMemoryCardDialog::ConvertMemoryCardDialog( wxWindow* parent, const wxDirName& mcdPath, const AppConfig::McdOptions& mcdSourceConfig )
	: wxDialogWithHelpers( parent, _( "Convert a memory card to a different format" ) )
	, m_mcdPath( mcdPath )
	, m_mcdSourceFilename( mcdSourceConfig.Filename.GetFullName() )
{
	SetMinWidth( 472 );

	CreateControls( mcdSourceConfig.Type );

	if ( m_radio_CardType ) m_radio_CardType->Realize();

	wxBoxSizer& s_buttons( *new wxBoxSizer( wxHORIZONTAL ) );
	s_buttons += new wxButton( this, wxID_OK, _( "Convert" ) ) | pxProportion( 2 );
	s_buttons += pxStretchSpacer( 3 );
	s_buttons += new wxButton( this, wxID_CANCEL ) | pxProportion( 2 );

	wxBoxSizer& s_padding( *new wxBoxSizer( wxVERTICAL ) );

	s_padding += Heading( wxString( _( "Convert: " ) ) + ( mcdPath + m_mcdSourceFilename ).GetFullPath() ).Unwrapped() | pxSizerFlags::StdExpand();

	wxBoxSizer& s_filename( *new wxBoxSizer( wxHORIZONTAL ) );
	s_filename += Heading( _( "To: " ) ).SetMinWidth( 50 );
	m_text_filenameInput->SetMinSize( wxSize( 250, 20 ) );
	m_text_filenameInput->SetValue( wxFileName( m_mcdSourceFilename ).GetName() + L"_converted" );
	s_filename += m_text_filenameInput;
	s_filename += Heading( L".ps2" );

	s_padding += s_filename | wxALIGN_LEFT;

	s_padding += m_radio_CardType | pxSizerFlags::StdExpand();

	s_padding += Heading( pxE( L"WARNING: Converting a memory card may take several minutes! Please do not close the emulator during the conversion process, even if the emulator is no longer responding to input." ) );

	s_padding += 12;
	s_padding += s_buttons | pxSizerFlags::StdCenter();

	*this += s_padding | pxSizerFlags::StdExpand();

	Connect( wxID_OK, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConvertMemoryCardDialog::OnOk_Click ) );
	Connect( m_text_filenameInput->GetId(), wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler( ConvertMemoryCardDialog::OnOk_Click ) );

	m_text_filenameInput->SetFocus();
	m_text_filenameInput->SelectAll();
}

void Dialogs::ConvertMemoryCardDialog::CreateControls( const MemoryCardType sourceType ) {
	m_text_filenameInput = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER );

	RadioPanelItem toFile = RadioPanelItem( _( "File" ), pxE( L"Convert this memory card to a regular 8 MB .ps2 file. Please note that the resulting file may not actually contain all saves, depending on how many are in the source memory card." ) )
		.SetInt( MemoryCardType::MemoryCard_File );
	RadioPanelItem toFolder = RadioPanelItem( _( "Folder" ), _( "Convert this memory card to a folder of individual saves." ) )
		.SetInt( MemoryCardType::MemoryCard_Folder );

	const RadioPanelItem tblForFile[] = { toFolder };
	const RadioPanelItem tblForFolder[] = { toFile };

	switch ( sourceType ) {
		case MemoryCardType::MemoryCard_File:
			m_radio_CardType = new pxRadioPanel( this, tblForFile );
			break;
		case MemoryCardType::MemoryCard_Folder:
			m_radio_CardType = new pxRadioPanel( this, tblForFolder );
			break;
		default:
			Console.Error( L"Memory Card Conversion: Invalid source type!" );
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
		MemoryCardType targetType = (MemoryCardType)m_radio_CardType->SelectedItem().SomeInt;

		switch ( targetType ) {
		case MemoryCardType::MemoryCard_File:
			success = ConvertToFile( sourcePath, targetPath );
			break;
		case MemoryCardType::MemoryCard_Folder:
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

bool Dialogs::ConvertMemoryCardDialog::ConvertToFile( const wxFileName& sourcePath, const wxFileName& targetPath ) {
	// Conversion method: Open FolderMcd as usual, then read the raw data from it and write it to a file stream

	wxFFile targetFile( targetPath.GetFullPath(), L"wb" );
	if ( !targetFile.IsOpened() ) {
		return false;
	}

	FolderMemoryCard sourceFolderMemoryCard;
	AppConfig::McdOptions config;
	config.Enabled = true;
	config.Type = MemoryCardType::MemoryCard_Folder;
	sourceFolderMemoryCard.Open( sourcePath.GetFullPath(), config, false, L"" );

	u8 buffer[FolderMemoryCard::PageSizeRaw];
	u32 adr = 0;
	while ( adr < FolderMemoryCard::TotalSizeRaw ) {
		sourceFolderMemoryCard.Read( buffer, adr, FolderMemoryCard::PageSizeRaw );
		targetFile.Write( buffer, FolderMemoryCard::PageSizeRaw );
		adr += FolderMemoryCard::PageSizeRaw;
	}

	targetFile.Close();
	sourceFolderMemoryCard.Close();

	return true;
}

bool Dialogs::ConvertMemoryCardDialog::ConvertToFolder( const wxFileName& sourcePath, const wxFileName& targetPath ) {
	// Conversion method: Read all pages of the FileMcd into a FolderMcd, then just write that out with the regular methods
	// TODO: Test if >8MB files don't super fuck up something

	wxFFile sourceFile( sourcePath.GetFullPath(), L"rb" );
	if ( !sourceFile.IsOpened() ) {
		return false;
	}

	u8 buffer[FolderMemoryCard::PageSizeRaw];
	FolderMemoryCard targetFolderMemoryCard;
	AppConfig::McdOptions config;
	config.Enabled = true;
	config.Type = MemoryCardType::MemoryCard_Folder;
	targetFolderMemoryCard.Open( targetPath.GetFullPath(), config, false, L"" );

	u32 adr = 0;
	while ( !sourceFile.Eof() ) {
		int size = sourceFile.Read( buffer, FolderMemoryCard::PageSizeRaw );
		if ( size > 0 ) {
			targetFolderMemoryCard.Save( buffer, adr, size );
			adr += size;
		}
	}

	sourceFile.Close();
	targetFolderMemoryCard.Close();

	if ( adr != FolderMemoryCard::TotalSizeRaw ) {
		// reset memory card metrics in superblock to the default 8MB, since the converted card was different
		targetFolderMemoryCard.Open( targetPath.GetFullPath(), config, true, L"" );
		targetFolderMemoryCard.SetSizeInMB( 8 );
		targetFolderMemoryCard.Close();
	}

	return true;
}
