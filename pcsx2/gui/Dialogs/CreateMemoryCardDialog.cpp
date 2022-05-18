/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2018  PCSX2 Dev Team
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
//#include <wx/filepicker.h>
#include <wx/ffile.h>

using namespace pxSizerFlags;

extern wxString GetMsg_McdNtfsCompress();
/*
wxFilePickerCtrl* CreateMemoryCardFilePicker( wxWindow* parent, uint portidx, uint slotidx, const wxString& filename=wxEmptyString )
{
	return new wxFilePickerCtrl( parent, wxID_ANY, filename,
		wxsFormat(_("Select memory card for Port %u / Slot %u"), portidx+1, slotidx+1),	// picker window title
		L"*.ps2",	// default wildcard
		wxDefaultPosition, wxDefaultSize,
		wxFLP_DEFAULT_STYLE & ~wxFLP_FILE_MUST_EXIST
	);

}
*/
Dialogs::CreateMemoryCardDialog::CreateMemoryCardDialog( wxWindow* parent, const wxDirName& mcdpath, const wxString& suggested_mcdfileName)
	: wxDialogWithHelpers( parent, _("Create a new memory card") )
	, m_mcdpath( mcdpath )
	, m_mcdfile( suggested_mcdfileName )//suggested_and_result_mcdfileName.IsEmpty() ? g_Conf->Mcd[slot].Filename.GetFullName()
{
	SetMinWidth( 472 * MSW_GetDPIScale());
	//m_filepicker	= NULL;

	CreateControls();

	//m_filepicker = CreateMemoryCardFilePicker( this, m_port, m_slot, filepath );

	// ----------------------------
	//      Sizers and Layout
	// ----------------------------


	wxBoxSizer& s_buttons( *new wxBoxSizer(wxHORIZONTAL) );
	s_buttons += new wxButton( this, wxID_OK, _("Create") )	| pxProportion(2);
	s_buttons += pxStretchSpacer(3);
	s_buttons += new wxButton( this, wxID_CANCEL )			| pxProportion(2);
	if (m_radio_CardType)
		m_radio_CardType->Realize();

	wxBoxSizer& s_padding( *new wxBoxSizer(wxVERTICAL) );

	//s_padding += Heading(_("Select the size for your new memory card."));

//	if( m_filepicker )
//		s_padding += m_filepicker			| StdExpand();
//	else
	{
		s_padding += Heading( _( "New memory card:" ) )					| StdExpand();
		s_padding += Heading( wxString(_("At folder:    ")) + (m_mcdpath + m_mcdfile).GetPath() ).Unwrapped()	| StdExpand();

		wxBoxSizer& s_filename( *new wxBoxSizer(wxHORIZONTAL) );
		s_filename += Heading( _("Select file name: ")).Unwrapped().Align(wxALIGN_RIGHT) | pxProportion(1);
		m_text_filenameInput->SetValue ((m_mcdpath + m_mcdfile).GetName());
		s_filename += m_text_filenameInput | pxProportion(2);
		s_filename += m_mcd_Extension | pxProportion(1);
		s_padding += s_filename | StdExpand();
	}

	s_padding += m_radio_CardType | StdExpand();
#ifdef __WXMSW__
	if (m_check_CompressNTFS)
		s_padding += m_check_CompressNTFS | StdExpand();
#endif


	s_padding += s_buttons | StdCenter();

	*this += s_padding | StdExpand();

	Bind(wxEVT_BUTTON, &CreateMemoryCardDialog::OnOk_Click, this, wxID_OK);
	Bind(wxEVT_TEXT_ENTER, &CreateMemoryCardDialog::OnOk_Click, this, m_text_filenameInput->GetId());
	Bind(wxEVT_RADIOBUTTON, &CreateMemoryCardDialog::OnRadioChanged, this);

	// ...Typical solution to everything? Or are we doing something weird?
	SetSizerAndFit(GetSizer());

	m_text_filenameInput->SetFocus();
	m_text_filenameInput->SelectAll();
}

bool Dialogs::CreateMemoryCardDialog::CreateIt( const wxString& mcdFile, uint sizeInMB, bool isPSX )
{
	//int enc[16] = {0x77,0x7f,0x7f,0x77,0x7f,0x7f,0x77,0x7f,0x7f,0x77,0x7f,0x7f,0,0,0,0};

	// PS2 Memory Card
	u8	m_effeffs[528 * 16];
	memset8<0xff>(m_effeffs);

	// PSX Memory Card; 8192 is the size in bytes of a single block of a PSX memory card (8 KiB).
	u8 m_effeffs_psx[8192];
	memset8<0xff>(m_effeffs_psx);

	// Since isPSX will have a default false state, it makes more sense to check "not PSX" first
	if (!isPSX) {
		Console.WriteLn("(FileMcd) Creating new PS2 %uMB memory card: '%ls'", sizeInMB, WX_STR(mcdFile));
	}
	else {
		Console.WriteLn("(FileMcd) Creating new PSX 128 KiB memory card: '%ls'", WX_STR(mcdFile));
	}

	wxFFile fp( mcdFile, L"wb" );
	if( !fp.IsOpened() ) return false;

	static const int MC2_MBSIZE	= 1024 * 528 * 2;		// Size of a single megabyte of card data

	if (!isPSX) {
		for (uint i = 0; i<(MC2_MBSIZE*sizeInMB) / sizeof(m_effeffs); i++) {
			if (fp.Write(m_effeffs, sizeof(m_effeffs)) == 0) {
				return false;
			}
		}
	} else {
		// PSX cards consist of 16 blocks, each 8 KiB in size.
		for (uint i = 0; i < 16; i++) {
			if (fp.Write(m_effeffs_psx, sizeof(m_effeffs_psx)) == 0) {
				return false;
			}
		}
	}

	return true;
}

void Dialogs::CreateMemoryCardDialog::OnRadioChanged(wxCommandEvent& evt)
{
	evt.Skip();

	m_mcd_Extension->SetLabel(m_radio_CardType->SelectedItem().SomeInt == 1 ? L".mcr" : L".ps2");
}

void Dialogs::CreateMemoryCardDialog::OnOk_Click( wxCommandEvent& evt )
{
	// Save status of the NTFS compress checkbox for future reference.
	// [TODO] Remove g_Conf->McdCompressNTFS, and have this dialog load/save directly from the ini.

#ifdef __WXMSW__
	g_Conf->EmuOptions.McdCompressNTFS = m_check_CompressNTFS->GetValue();
#endif
	result_createdMcdFilename=L"_INVALID_FILE_NAME_";

	wxString composedName = m_text_filenameInput->GetValue().Trim() + (m_radio_CardType->SelectedItem().SomeInt == 1 ? L".mcr" : L".ps2");

	wxString errMsg;
	if( !isValidNewFilename(composedName, m_mcdpath, errMsg, 5) )
	{
		wxString message;
		message.Printf(_("Error (%s)"), errMsg.c_str());
		Msgbox::Alert( message, _("Create memory card") );
		m_text_filenameInput->SetFocus();
		m_text_filenameInput->SelectAll();
		return;
	}

	wxString fullPath = ( m_mcdpath + composedName ).GetFullPath();
	if (m_radio_CardType && m_radio_CardType->SelectedItem().SomeInt == 0) {
		// user selected to create a folder memory card
		if ( !wxFileName::Mkdir( fullPath ) ) {
			Msgbox::Alert(
				_( "Error: The directory for the memory card could not be created." ),
				_( "Create memory card" )
			);
		} else {
			// also create an empty superblock so we can recognize memory card folders based on if they have a superblock
			wxFFile superblock( wxFileName( fullPath, L"_pcsx2_superblock" ).GetFullPath(), L"wb" );
			superblock.Close();
		}
	} else {
		// otherwise create a file
		if (!CreateIt(
				fullPath,
				m_radio_CardType ? m_radio_CardType->SelectedItem().SomeInt : 8,
				m_radio_CardType->SelectedItem().SomeInt == 1)) {
			Msgbox::Alert(
				_( "Error: The memory card could not be created." ),
				_( "Create memory card" )
				);
			return;
		}
	}

	result_createdMcdFilename = composedName;
	EndModal( wxID_OK );
}

void Dialogs::CreateMemoryCardDialog::CreateControls()
{
#ifdef __WXMSW__
	m_check_CompressNTFS = new pxCheckBox( this,
		_("Use NTFS compression when creating this card."),
		GetMsg_McdNtfsCompress()
	);

	m_check_CompressNTFS->SetToolTip( pxEt( L"NTFS compression can be changed manually at any time by using file properties from Windows Explorer."
		)
	);

	// Initial value of the checkbox is saved between calls to the dialog box.  If the user checks
	// the option, it remains checked for future dialog.  If the user unchecks it, ditto.
	m_check_CompressNTFS->SetValue( g_Conf->EmuOptions.McdCompressNTFS );
#endif

	m_text_filenameInput = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	m_mcd_Extension = new wxStaticText(this, wxID_ANY, L".ps2");

	const RadioPanelItem tbl_CardTypes[] =
	{
		RadioPanelItem(_("8 MB [most compatible]"), _("This is the standard Sony-provisioned size, and is supported by all games and BIOS versions."))
		.	SetToolTip(_t("Always use this option if you want the safest and surest memory card behavior."))
		.	SetInt(8),

		RadioPanelItem(_("16 MB"), _("A typical size for 3rd-party memory cards which should work with most games."))
		.	SetToolTip(_t("16 and 32 MB cards have roughly the same compatibility factor."))
		.	SetInt(16),

		RadioPanelItem(_("32 MB"), _("A typical size for 3rd-party memory cards which should work with most games."))
		.	SetToolTip(_t("16 and 32 MB cards have roughly the same compatibility factor."))
		.	SetInt(32),

		RadioPanelItem(_("64 MB"), _("Low compatibility warning: Yes it's very big, but may not work with many games."))
		.	SetToolTip(_t("Use at your own risk.  Erratic memory card behavior is possible (though unlikely)."))
		.	SetInt(64),

		RadioPanelItem(_("Folder [Recommended]"), _("Store memory card contents in the host filesystem instead of a file."))
		.	SetToolTip(_t("Dynamically allocate and store memory card contents in a folder.\n" 
						  "Only exposes the save files for the running game, rather than the whole memory card.\n"
						  "You can see the structure and the saves with your File Explorer.\n"
						  "Can be used to back-up individual saves instead of all saves on a memcard.\n" 
						  "Incompatible with PS2 memory card editing tools or savegame managers (such as MyMC, MyMCPlus).\n"
						  "You can always convert back between folder type and single memcard file.\n"))
		.	SetInt(0),

		RadioPanelItem(_("128 KiB (PS1)"), _("This is the standard Sony-provisioned size PS1 memory card, only compatible with PS1 games."))
		.	SetToolTip(_t("This memory card is required by PS1 games. It is not compatible with PS2 games."))
		.	SetInt(1)
	};

	m_radio_CardType = new pxRadioPanel(this, tbl_CardTypes);
	m_radio_CardType->SetDefaultItem(0);
}

