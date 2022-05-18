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
#include "gui/AppCoreThread.h"
#include "System.h"
#include "MemoryCardFile.h"

#include "ConfigurationPanels.h"
#include "MemoryCardPanels.h"

#include "gui/Dialogs/ConfigurationDialog.h"
#include "gui/IniInterface.h"
#include "common/StringUtil.h"
#include "Sio.h"

#include <wx/filepicker.h>
#include <wx/ffile.h>
#include <wx/dir.h>


static wxDataFormat drag_drop_format(L"PCSX2McdDragDrop");

bool CopyDirectory(const wxString& from, const wxString& to);
bool RemoveWxDirectory(const wxString& dirname);

using namespace pxSizerFlags;
using namespace Panels;

static bool IsMcdFormatted(wxFFile& fhand)
{
	static const char formatted_psx[] = "MC";
	static const char formatted_string[] = "Sony PS2 Memory Card Format";
	static const int fmtstrlen = sizeof(formatted_string) - 1;

	char dest[fmtstrlen];

	fhand.Read(dest, fmtstrlen);

	bool formatted = memcmp(formatted_string, dest, fmtstrlen) == 0;
	if (!formatted)
		formatted = memcmp(formatted_psx, dest, 2) == 0;

	return formatted;
}

//sets IsPresent if the file is valid, and derived properties (filename, formatted, size, etc)
bool EnumerateMemoryCard(McdSlotItem& dest, const wxFileName& filename, const wxDirName basePath)
{
	dest.IsFormatted = false;
	dest.IsPresent = false;
	dest.IsPSX = false;
	dest.Type = MemoryCardType::Empty;

	const wxString fullpath(filename.GetFullPath());
	//DevCon.WriteLn( fullpath );
	if (filename.FileExists())
	{
		// might be a memory card file
		wxFFile mcdFile(fullpath);
		if (!mcdFile.IsOpened())
		{
			return false;
		} // wx should log the error for us.

		wxFileOffset length = mcdFile.Length();

		if (length < (1024 * 528) && length != 0x20000)
		{
			Console.Warning("... Memory card appears to be truncated. Ignoring.");
			return false;
		}

		const int mb_ecc = 1024 * 528 * 2;
		const int mb_noecc = 1024 * 512 * 2;
		dest.SizeInMB = (uint)(length % mb_ecc ? length / mb_noecc : length / mb_ecc);

		if (length == 0x20000)
		{
			dest.IsPSX = true; // PSX memcard;
			dest.SizeInMB = 1; // MegaBIT
		}

		dest.Type = MemoryCardType::File;
		dest.IsFormatted = IsMcdFormatted(mcdFile);
		filename.GetTimes(NULL, &dest.DateModified, &dest.DateCreated);
	}
	else if (filename.DirExists())
	{
		// might be a memory card folder
		wxFileName superBlockFileName(fullpath, L"_pcsx2_superblock");
		if (!superBlockFileName.FileExists())
		{
			return false;
		}
		wxFFile mcdFile(superBlockFileName.GetFullPath());
		if (!mcdFile.IsOpened())
		{
			return false;
		}

		dest.SizeInMB = 0;

		dest.Type = MemoryCardType::Folder;
		dest.IsFormatted = IsMcdFormatted(mcdFile);
		superBlockFileName.GetTimes(NULL, &dest.DateModified, &dest.DateCreated);
	}
	else
	{
		// is neither
		return false;
	}

	dest.IsPresent = true;
	dest.Filename = filename;
	if (filename.GetFullPath() == (basePath + filename.GetFullName()).GetFullPath())
		dest.Filename = filename.GetFullName();

	return true;
}

//avih: unused
/*
static int EnumerateMemoryCards( McdList& dest, const wxArrayString& files )
{
	int pushed = 0;
	Console.WriteLn( Color_StrongBlue, "Enumerating memory cards..." );
	for( size_t i=0; i<files.GetCount(); ++i )
	{
		ConsoleIndentScope con_indent;
		McdSlotItem mcdItem;
		if( EnumerateMemoryCard(mcdItem, files[i]) )
		{
			dest.push_back( mcdItem );
			++pushed;
		}
	}
	if( pushed > 0 )
		Console.WriteLn( Color_StrongBlue, "Memory card Enumeration Complete." );
	else
		Console.WriteLn( Color_StrongBlue, "No valid memory card found." );

	return pushed;
}
*/
// --------------------------------------------------------------------------------------
//  McdListItem  (implementations)
// --------------------------------------------------------------------------------------
bool McdSlotItem::IsMultitapSlot() const
{
	return FileMcd_IsMultitapSlot(Slot);
}

uint McdSlotItem::GetMtapPort() const
{
	return FileMcd_GetMtapPort(Slot);
}

uint McdSlotItem::GetMtapSlot() const
{
	return FileMcd_GetMtapSlot(Slot);
}

// Compares two cards -- If this equality comparison is used on items where
// no filename is specified, then the check will include port and slot.
bool McdSlotItem::operator==(const McdSlotItem& right) const
{
	bool fileEqu;

	if (Filename.GetFullName().IsEmpty())
		fileEqu = OpEqu(Slot);
	else
		fileEqu = OpEqu(Filename);

	return fileEqu &&
		   OpEqu(IsPresent) && OpEqu(IsEnabled) &&
		   OpEqu(SizeInMB) && OpEqu(IsFormatted) &&
		   OpEqu(DateCreated) && OpEqu(DateModified);
}

bool McdSlotItem::operator!=(const McdSlotItem& right) const
{
	return operator==(right);
}


// =====================================================================================================
//  BaseMcdListPanel (implementations)
// =====================================================================================================
Panels::BaseMcdListPanel::BaseMcdListPanel(wxWindow* parent)
	: _parent(parent)
{
	m_FolderPicker = new DirPickerPanel(this, FolderId_MemoryCards,
										//_("memory card Search Path:"),				// static box label
										_("Select folder with PS2 memory cards") // dir picker popup label
	);

	m_listview = NULL;
	s_leftside_buttons = NULL;
	s_rightside_buttons = NULL;

	m_btn_Refresh = new wxButton(this, wxID_ANY, _("Refresh list"));

	Bind(wxEVT_BUTTON, &BaseMcdListPanel::OnRefreshSelections, this, m_btn_Refresh->GetId());
}

void Panels::BaseMcdListPanel::RefreshMcds() const
{
	wxCommandEvent refit(wxEVT_BUTTON);
	refit.SetId(m_btn_Refresh->GetId());
	GetEventHandler()->AddPendingEvent(refit);
}

void Panels::BaseMcdListPanel::CreateLayout()
{
	//if( m_listview ) m_listview->SetMinSize( wxSize( 480, 140 ) );

	wxFlexGridSizer* s_flex = new wxFlexGridSizer(3, 1, 0, 0);
	s_flex->AddGrowableCol(0);
	s_flex->AddGrowableRow(1);
	SetSizer(s_flex);

	wxBoxSizer& s_buttons(*new wxBoxSizer(wxHORIZONTAL));
	s_leftside_buttons = new wxBoxSizer(wxHORIZONTAL);
	s_rightside_buttons = new wxBoxSizer(wxHORIZONTAL);

	s_buttons += s_leftside_buttons | pxAlignLeft;
	s_buttons += pxStretchSpacer();
	s_buttons += s_rightside_buttons | pxAlignRight;

	if (m_FolderPicker)
		*this += m_FolderPicker | pxExpand;
	else
		*this += StdPadding; //we need the 'else' because we need these items to land into the proper rows of s_flex.

	if (m_listview)
		*this += m_listview | pxExpand;
	else
		*this += StdPadding;

	*this += s_buttons | pxExpand;

	*s_leftside_buttons += m_btn_Refresh;

	if (m_listview)
	{
		IniLoader loader;
		ScopedIniGroup group(loader, L"MemoryCardListPanel");
		m_listview->LoadSaveColumns(loader);
	}
}

void Panels::BaseMcdListPanel::Apply()
{
	// Save column widths to the configuration file.  Since these are used *only* by this
	// dialog, we use a direct local ini save approach, instead of going through g_conf.
	if (m_listview)
	{
		IniSaver saver;
		ScopedIniGroup group(saver, L"MemoryCardListPanel");
		m_listview->LoadSaveColumns(saver);
	}
}

void Panels::BaseMcdListPanel::AppStatusEvent_OnSettingsApplied()
{
	if ((m_MultitapEnabled[0] != g_Conf->EmuOptions.MultitapPort0_Enabled) ||
		(m_MultitapEnabled[1] != g_Conf->EmuOptions.MultitapPort1_Enabled))
	{
		m_MultitapEnabled[0] = g_Conf->EmuOptions.MultitapPort0_Enabled;
		m_MultitapEnabled[1] = g_Conf->EmuOptions.MultitapPort1_Enabled;

		RefreshMcds();
	}
}

class McdDropTarget : public wxDropTarget
{
protected:
	BaseMcdListView* m_listview;

public:
	McdDropTarget(BaseMcdListView* listview = NULL)
	{
		m_listview = listview;
		SetDataObject(new wxCustomDataObject(drag_drop_format));
	}

	virtual wxDragResult OnData(wxCoord x, wxCoord y, wxDragResult def)
	{
		int flags = 0;

		int destViewIndex = m_listview->HitTest(wxPoint(x, y), flags);
		if (wxNOT_FOUND == destViewIndex)
			destViewIndex = -1; //non list item target is the filesystem placeholder.

		if (!GetData())
			return wxDragNone;

		wxCustomDataObject* dobj = static_cast<wxCustomDataObject*>(GetDataObject());

		if (dobj->GetDataSize() != sizeof(int))
			return wxDragNone;

		int sourceViewIndex;
		dobj->GetDataHere(&sourceViewIndex);

		wxDragResult result = OnDropMcd(
			m_listview->GetMcdProvider().GetCardForViewIndex(sourceViewIndex),
			m_listview->GetMcdProvider().GetCardForViewIndex(destViewIndex),
			def);

		if (wxDragNone == result)
			return wxDragNone;

		m_listview->GetMcdProvider().RefreshMcds();
		return result;
	}


	virtual wxDragResult OnDropMcd(McdSlotItem& src, McdSlotItem& dest, wxDragResult def)
	{
		if (src.Slot == dest.Slot)
			return wxDragNone;
		//if( !pxAssert( (src.Slot >= 0) && (dest.Slot >= 0) ) ) return wxDragNone;
		const wxDirName basepath(m_listview->GetMcdProvider().GetMcdPath());

		if (wxDragCopy == def)
		{
			if (!m_listview->GetMcdProvider().UiDuplicateCard(src, dest))
				return wxDragNone;
		}
		else if (wxDragMove == def)
		{ // source can only be an existing card.
			//   if dest is a ps2-port (empty or not) -> swap cards between ports.
			//   is dest is a non-ps2-port -> remove card from port.

			//   Note: For the sake of usability, automatically enable dest if a ps2-port.
			if (src.IsPresent)
			{
				wxFileName tmpFilename = dest.Filename;
				bool tmpPresent = dest.IsPresent;
				if (src.Slot < 0 && m_listview->GetMcdProvider().isFileAssignedToInternalSlot(src.Filename))
					m_listview->GetMcdProvider().RemoveCardFromSlot(src.Filename);

				dest.Filename = src.Filename;
				dest.IsEnabled = dest.IsPresent ? dest.IsEnabled : true;
				dest.IsPresent = src.IsPresent;

				if (dest.Slot >= 0)
				{ //2 internal slots: swap
					src.Filename = tmpFilename;
					src.IsPresent = tmpPresent;
				}
				else
				{ //dest is at the filesystem (= remove card from slot)
					src.Filename = L"";
					src.IsPresent = false;
					src.IsEnabled = false;
				}
			}
		}

		return def;
	}
};


enum McdMenuId
{
	McdMenuId_Create = 0x888,
	//McdMenuId_Mount,
	McdMenuId_Rename,
	McdMenuId_RefreshList,
	McdMenuId_AssignUnassign,
	McdMenuId_Duplicate,
	McdMenuId_Convert,
};


Panels::MemoryCardListPanel_Simple* g_uglyPanel = NULL;
void g_uglyFunc()
{
	if (g_uglyPanel)
		g_uglyPanel->OnChangedListSelection();
}

Panels::MemoryCardListPanel_Simple::~MemoryCardListPanel_Simple() { g_uglyPanel = NULL; }

Panels::MemoryCardListPanel_Simple::MemoryCardListPanel_Simple(wxWindow* parent)
	: _parent(parent)
{
	m_MultitapEnabled[0] = false;
	m_MultitapEnabled[1] = false;

	m_listview = new MemoryCardListView_Simple(this);

	m_listview->SetMinSize(wxSize(620, m_listview->GetCharHeight() * 13)); // 740 is nice for default font sizes

	m_listview->SetDropTarget(new McdDropTarget(m_listview));

	//m_button_Mount	= new wxButton(this, wxID_ANY, _("Enable port"));

	m_button_AssignUnassign = new wxButton(this, wxID_ANY, _("Eject"));
	m_button_Duplicate = new wxButton(this, wxID_ANY, _("Duplicate ..."));
	m_button_Rename = new wxButton(this, wxID_ANY, _("Rename ..."));
	m_button_Create = new wxButton(this, wxID_ANY, _("Create ..."));
	m_button_Convert = new wxButton(this, wxID_ANY, _("Convert ..."));

	// ------------------------------------
	//       Sizer / Layout Section
	// ------------------------------------

	CreateLayout();

	*s_leftside_buttons += 20;
	//*s_leftside_buttons	+= m_button_Mount;
	//*s_leftside_buttons	+= 20;

	*s_leftside_buttons += Label(_("Card: ")) | pxMiddle;
	*s_leftside_buttons += m_button_AssignUnassign;
	*s_leftside_buttons += 20;
	*s_leftside_buttons += m_button_Duplicate;
	*s_leftside_buttons += 2;
	*s_leftside_buttons += m_button_Rename;
	*s_leftside_buttons += 2;
	*s_leftside_buttons += m_button_Create;
	*s_leftside_buttons += 2;
	*s_leftside_buttons += m_button_Convert;
	SetSizerAndFit(GetSizer());

	parent->SetWindowStyle(parent->GetWindowStyle() | wxRESIZE_BORDER);

	Bind(wxEVT_LIST_BEGIN_DRAG, &MemoryCardListPanel_Simple::OnListDrag, this, m_listview->GetId());
	Bind(wxEVT_LIST_ITEM_SELECTED, &MemoryCardListPanel_Simple::OnListSelectionChanged, this, m_listview->GetId());
	Bind(wxEVT_LIST_ITEM_ACTIVATED, &MemoryCardListPanel_Simple::OnItemActivated, this, m_listview->GetId()); //enter or double click

	//Deselected is not working for some reason (e.g. when clicking an empty row at the table?) - avih
	// wxMSW bug for virtual listviews. Works fine on Linux: http://trac.wxwidgets.org/ticket/1919 - turtleli
	Bind(wxEVT_LIST_ITEM_DESELECTED, &MemoryCardListPanel_Simple::OnListSelectionChanged, this, m_listview->GetId());

	Bind(wxEVT_LIST_ITEM_RIGHT_CLICK, &MemoryCardListPanel_Simple::OnOpenItemContextMenu, this, m_listview->GetId());

	//Bind(wxEVT_BUTTON, &MemoryCardListPanel_Simple::OnMountCard, this, m_button_Mount->GetId());
	Bind(wxEVT_BUTTON, &MemoryCardListPanel_Simple::OnCreateOrDeleteCard, this, m_button_Create->GetId());
	Bind(wxEVT_BUTTON, &MemoryCardListPanel_Simple::OnConvertCard, this, m_button_Convert->GetId());
	Bind(wxEVT_BUTTON, &MemoryCardListPanel_Simple::OnRenameFile, this, m_button_Rename->GetId());
	Bind(wxEVT_BUTTON, &MemoryCardListPanel_Simple::OnDuplicateFile, this, m_button_Duplicate->GetId());
	Bind(wxEVT_BUTTON, &MemoryCardListPanel_Simple::OnAssignUnassignFile, this, m_button_AssignUnassign->GetId());

	// Popup Menu Connections!
	//Bind(McdMenuId_Mount, &MemoryCardListPanel_Simple::OnMountCard, this, McdMenuId_Mount);
	Bind(wxEVT_MENU, &MemoryCardListPanel_Simple::OnCreateOrDeleteCard, this, McdMenuId_Create);
	Bind(wxEVT_MENU, &MemoryCardListPanel_Simple::OnConvertCard, this, McdMenuId_Convert);
	Bind(wxEVT_MENU, &MemoryCardListPanel_Simple::OnRenameFile, this, McdMenuId_Rename);
	Bind(wxEVT_MENU, &MemoryCardListPanel_Simple::OnDuplicateFile, this, McdMenuId_Duplicate);
	Bind(wxEVT_MENU, &MemoryCardListPanel_Simple::OnAssignUnassignFile, this, McdMenuId_AssignUnassign);

	Bind(wxEVT_MENU, &MemoryCardListPanel_Simple::OnRefreshSelections, this, McdMenuId_RefreshList);

	//because the wxEVT_LIST_ITEM_DESELECTED doesn't work (buttons stay enabled when clicking an empty area of the list),
	//  m_listview can send us an event that indicates a change at the list. Ugly, but works.
	g_uglyPanel = this;
	m_listview->setExternHandler(g_uglyFunc);
}

void Panels::MemoryCardListPanel_Simple::UpdateUI()
{
	if (!m_listview)
		return;

	int sel = m_listview->GetFirstSelected();

	m_button_Create->Enable();

	if (wxNOT_FOUND == sel)
	{
		m_button_Create->SetLabel(_("Create ..."));
		pxSetToolTip(m_button_Create, _("Create a new memory card."));

		//		m_button_Mount->Disable();
		m_button_Rename->Disable();
		m_button_Duplicate->Disable();
		m_button_AssignUnassign->Disable();
		m_button_Convert->Disable();
		return;
	}

	const McdSlotItem& card(GetCardForViewIndex(sel));


	m_button_Rename->Enable(card.IsPresent);
	wxString renameTip = _("Rename this memory card ...");
	pxSetToolTip(m_button_Rename, renameTip);

	m_button_AssignUnassign->Enable(card.IsPresent);
	m_button_AssignUnassign->SetLabel(card.Slot >= 0 ? _("Eject") : _("Insert ..."));
	wxString assignTip = (card.Slot >= 0) ? _("Eject the card from this port") : _("Insert this card to a port ...");
	pxSetToolTip(m_button_AssignUnassign, assignTip);

	m_button_Duplicate->Enable(card.IsPresent);
	wxString dupTip = _("Create a duplicate of this memory card ...");
	pxSetToolTip(m_button_Duplicate, dupTip);

	m_button_Convert->Enable(card.IsPresent && card.IsFormatted && !card.IsPSX);
	pxSetToolTip(m_button_Convert, _("Convert this memory card to or from a folder memory card. Creates a duplicate of the current memory card in the new type.\n\n"
									 "Note: Only available when a memory card is formatted. Not available for PSX memory cards."));

	//m_button_Create->Enable( card.Slot>=0 || card.IsPresent);
	m_button_Create->SetLabel(card.IsPresent ? _("Delete") : _("Create ..."));

	if (card.IsPresent)
		pxSetToolTip(m_button_Create, _("Permanently delete this memory card from disk (all contents are lost)"));
	else if (card.Slot >= 0)
		pxSetToolTip(m_button_Create, _("Create a new memory card and assign it to this Port."));
	else
		pxSetToolTip(m_button_Create, _("Create a new memory card."));

	/*
	m_button_Mount->Enable( card.IsPresent && card.Slot>=0);
	m_button_Mount->SetLabel( card.IsEnabled ? _("Disable Port") : _("Enable Port") );
	pxSetToolTip( m_button_Mount,
		card.IsEnabled
			? _("Disable the selected PS2-Port (this memory card will be invisible to games/BIOS).")
			: _("Enable the selected PS2-Port (games/BIOS will see this memory card).")
	);
*/
}

void Panels::MemoryCardListPanel_Simple::Apply()
{
	_parent::Apply();

	int used = 0;
	Console.WriteLn("Apply memory cards:");
	for (uint slot = 0; slot < 8; ++slot)
	{
		g_Conf->EmuOptions.Mcd[slot].Type = m_Cards[slot].Type;
		g_Conf->EmuOptions.Mcd[slot].Enabled = m_Cards[slot].IsEnabled && m_Cards[slot].IsPresent;
		if (m_Cards[slot].IsPresent)
			g_Conf->EmuOptions.Mcd[slot].Filename = StringUtil::wxStringToUTF8String(m_Cards[slot].Filename.GetFullName());
		else
			g_Conf->EmuOptions.Mcd[slot].Filename.clear();

		if (g_Conf->EmuOptions.Mcd[slot].Enabled)
		{
			used++;
			Console.WriteLn("slot[%d]='%s'", slot, g_Conf->EmuOptions.Mcd[slot].Filename.c_str());
		}
	}
	if (!used)
		Console.WriteLn("No active slots.");

	SetForceMcdEjectTimeoutNow();
}

void Panels::MemoryCardListPanel_Simple::AppStatusEvent_OnSettingsApplied()
{
	for (uint slot = 0; slot < 8; ++slot)
	{
		m_Cards[slot].IsEnabled = g_Conf->EmuOptions.Mcd[slot].Enabled;
		m_Cards[slot].Filename = StringUtil::UTF8StringToWxString(g_Conf->EmuOptions.Mcd[slot].Filename);

		// Automatically create the enabled but non-existing file such that it can be managed (else will get created anyway on boot)
		wxString targetFile = (GetMcdPath() + m_Cards[slot].Filename.GetFullName()).GetFullPath();
		if (m_Cards[slot].IsEnabled && !(wxFileExists(targetFile) || wxDirExists(targetFile)))
		{
			wxString errMsg;
			if (isValidNewFilename(m_Cards[slot].Filename.GetFullName(), GetMcdPath(), errMsg, 5))
			{
				if (!Dialogs::CreateMemoryCardDialog::CreateIt(targetFile, 8, false))
				{
					Console.Error("Automatic creation of memory card '%ls' failed. Hope for the best...", WX_STR(targetFile));
				}
				else
				{
					Console.WriteLn("Memory card created: '%ls'.", WX_STR(targetFile));
				}
			}
			else
			{
				Console.Error("Memory card was enabled, but it had an invalid file name. Aborting automatic creation. Hope for the best... (%ls)", WX_STR(errMsg));
			}
		}

		if (!m_Cards[slot].IsEnabled || !(wxFileExists(targetFile) || wxDirExists(targetFile)))
		{
			m_Cards[slot].IsEnabled = false;
			m_Cards[slot].IsPresent = false;
			m_Cards[slot].Filename = L"";
		}
	}
	DoRefresh();

	_parent::AppStatusEvent_OnSettingsApplied();
}


//BUG: the next function is never reached because, for some reason, IsoDropTarget::OnDropFiles is called instead.
//     Interestingly, IsoDropTarget::OnDropFiles actually "detects" a memory card file as a valid Audio-CD ISO...  - avih
bool Panels::MemoryCardListPanel_Simple::OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& filenames)
{
	if (filenames.GetCount() == 1 && wxFileName(filenames[0]).IsDir())
	{
		m_FolderPicker->SetPath(filenames[0]);
		return true;
	}
	return false;
}

bool Panels::MemoryCardListPanel_Simple::ValidateEnumerationStatus()
{
	if (m_listview)
		m_listview->SetMcdProvider(NULL);
	return false;
}

void Panels::MemoryCardListPanel_Simple::DoRefresh()
{
	for (uint slot = 0; slot < 8; ++slot)
	{
		//if( FileMcd_IsMultitapSlot(slot) && !m_MultitapEnabled[FileMcd_GetMtapPort(slot)] )
		//	continue;

		//wxFileName fullpath( m_FolderPicker->GetPath() + g_Conf->Mcd[slot].Filename.GetFullName() );
		wxFileName fullpath = m_FolderPicker->GetPath() + m_Cards[slot].Filename.GetFullName();

		EnumerateMemoryCard(m_Cards[slot], fullpath, m_FolderPicker->GetPath());
		m_Cards[slot].Slot = slot;
	}

	ReadFilesAtMcdFolder();


	if (m_listview)
		m_listview->SetMcdProvider(this);
	UpdateUI();
}


// =====================================================================================================
//  MemoryCardListPanel_Simple (implementations)
// =====================================================================================================

void Panels::MemoryCardListPanel_Simple::UiCreateNewCard(McdSlotItem& card)
{
	// card can also be the filesystem placeholder. On that case, the changes
	// made to it will be reverted on refresh, and we'll just have a new card at the folder.
	if (card.IsPresent)
	{
		Console.WriteLn("Error: Aborted: create memory card invoked, but a file is already associated.");
		return;
	}

	Dialogs::CreateMemoryCardDialog dialog(this, m_FolderPicker->GetPath(), L"my memory card");
	wxWindowID result = dialog.ShowModal();

	if (result != wxID_CANCEL)
	{
		card.IsEnabled = true;
		card.Filename = dialog.result_createdMcdFilename;
		card.IsPresent = true;
		if (card.Slot >= 0)
		{
			Console.WriteLn("Setting new memory card to slot %u: '%ls'", card.Slot, WX_STR(card.Filename.GetFullName()));
		}
		else
		{
			Console.WriteLn("Created a new unassigned memory card file: '%ls'", WX_STR(card.Filename.GetFullName()));
		}
	}
	else
	{
		card.IsEnabled = false;
	}

	Apply();
	RefreshSelections();
}

void Panels::MemoryCardListPanel_Simple::UiConvertCard(McdSlotItem& card)
{
	if (!card.IsPresent)
	{
		Console.WriteLn("Error: Aborted: Convert memory card invoked, but a file is not associated.");
		return;
	}

	Dialogs::ConvertMemoryCardDialog dialog(this, m_FolderPicker->GetPath(), card.Type, card.Filename.GetFullName());
	wxWindowID result = dialog.ShowModal();

	if (result != wxID_CANCEL)
	{
		Apply();
		RefreshSelections();
	}
}

void Panels::MemoryCardListPanel_Simple::UiDeleteCard(McdSlotItem& card)
{
	if (!card.IsPresent)
	{
		Console.WriteLn("Error: Aborted: delete memory card invoked, but a file is not associated.");
		return;
	}

	bool result = true;
	if (card.IsFormatted)
	{
		wxString content;
		content.Printf(
			pxE(L"You are about to delete the formatted memory card '%s'. All data on this card will be lost!  Are you absolutely and quite positively sure?"), WX_STR(card.Filename.GetFullName()));

		result = Msgbox::YesNo(content, _("Delete memory file?"));
	}

	if (result)
	{

		wxFileName fullpath(m_FolderPicker->GetPath() + card.Filename.GetFullName());

		card.IsEnabled = false;
		Apply();

		if (fullpath.FileExists())
		{
			wxRemoveFile(fullpath.GetFullPath());
		}
		else
		{
			RemoveWxDirectory(fullpath.GetFullPath());
		}

		RefreshSelections();
	}
}

bool Panels::MemoryCardListPanel_Simple::UiDuplicateCard(McdSlotItem& src, McdSlotItem& dest)
{
	wxDirName basepath = GetMcdPath();
	if (!src.IsPresent)
	{
		Msgbox::Alert(_("Failed: Can only duplicate an existing card."), _("Duplicate memory card"));
		return false;
	}

	if (dest.IsPresent && dest.Slot != -1)
	{
		wxString content;
		content.Printf(
			pxE(L"Failed: Duplicate is only allowed to an empty PS2-Port or to the file system."));

		Msgbox::Alert(content, _("Duplicate memory card"));
		return false;
	}

	while (1)
	{
		wxString newFilename = L"";
		newFilename = wxGetTextFromUser(_("Select a name for the duplicate\n( '.ps2' will be added automatically)"), _("Duplicate memory card"));
		if (newFilename == L"")
		{
			//Msgbox::Alert( _("Duplicate canceled"), _("Duplicate memory card") );
			return false;
		}
		newFilename += L".ps2";

		//check that the name is valid for a new file
		wxString errMsg;
		if (!isValidNewFilename(newFilename, basepath, errMsg, 5))
		{
			wxString message;
			message.Printf(_("Failed: %s"), WX_STR(errMsg));
			Msgbox::Alert(message, _("Duplicate memory card"));
			continue;
		}

		dest.Filename = newFilename;
		break;
	}

	wxFileName srcfile(basepath + src.Filename);
	wxFileName destfile(basepath + dest.Filename);

	ScopedBusyCursor doh(Cursor_ReallyBusy);

	if (!((srcfile.FileExists() && wxCopyFile(srcfile.GetFullPath(), destfile.GetFullPath(), true)) || (!srcfile.FileExists() && CopyDirectory(srcfile.GetFullPath(), destfile.GetFullPath()))))
	{
		wxString heading;
		heading.Printf(pxE(L"Failed: Destination memory card '%s' is in use."),
					   WX_STR(dest.Filename.GetFullName()), dest.Slot);

		wxString content;

		Msgbox::Alert(heading + L"\n\n" + content, _("Copy failed!"));

		return false;
	}

	// Destination memcard isEnabled state is the same now as the source's
	wxString success;
	success.Printf(_("Memory card '%s' duplicated to '%s'."),
				   WX_STR(src.Filename.GetFullName()),
				   WX_STR(dest.Filename.GetFullName()));
	Msgbox::Alert(success, _("Success"));
	dest.IsPresent = true;
	dest.IsEnabled = true;

	Apply();
	DoRefresh();
	return true;
}

void Panels::MemoryCardListPanel_Simple::UiRenameCard(McdSlotItem& card)
{
	if (!card.IsPresent)
	{
		Console.WriteLn("Error: Aborted: Rename memory card invoked, but no file is associated.");
		return;
	}

	const wxDirName basepath(m_listview->GetMcdProvider().GetMcdPath());
	wxString newFilename;
	while (1)
	{
		wxString title;
		title.Printf(_("Select a new name for the memory card '%s'\n( '.ps2' will be added automatically)"),
					 WX_STR(card.Filename.GetFullName()));
		newFilename = wxGetTextFromUser(title, _("Rename memory card"));
		if (newFilename == L"")
			return;

		newFilename += L".ps2";

		//check that the name is valid for a new file
		wxString errMsg;
		if (!isValidNewFilename(newFilename, basepath, errMsg, 5))
		{
			wxString message;
			message.Printf(_("Error (%s)"), WX_STR(errMsg));
			Msgbox::Alert(message, _("Rename memory card"));
			continue;
		}

		break;
	}

	bool origEnabled = card.IsEnabled;
	card.IsEnabled = false;
	Apply();
	if (!wxRenameFile((basepath + card.Filename).GetFullPath(), (basepath + wxFileName(newFilename)).GetFullPath(), false))
	{
		card.IsEnabled = origEnabled;
		Apply();
		Msgbox::Alert(_("Error: Rename could not be completed.\n"), _("Rename memory card"));

		return;
	}

	card.Filename = newFilename;
	card.IsEnabled = origEnabled;
	Apply();

	RefreshSelections();
}

void Panels::MemoryCardListPanel_Simple::OnCreateOrDeleteCard(wxCommandEvent& evt)
{
	int selectedViewIndex = m_listview->GetFirstSelected();
	if (wxNOT_FOUND == selectedViewIndex)
		selectedViewIndex = -1; //get filesystem placeholder, just create a new card at the filesystem.

	McdSlotItem& card(GetCardForViewIndex(selectedViewIndex));

	if (card.IsPresent)
		UiDeleteCard(card);
	else
		UiCreateNewCard(card);
}

void Panels::MemoryCardListPanel_Simple::OnConvertCard(wxCommandEvent& evt)
{
	int selectedViewIndex = m_listview->GetFirstSelected();
	if (wxNOT_FOUND == selectedViewIndex)
	{
		return;
	}

	McdSlotItem& card(GetCardForViewIndex(selectedViewIndex));
	if (card.IsPresent)
	{
		UiConvertCard(card);
	}
}

//enable/disapbe port
/*
void Panels::MemoryCardListPanel_Simple::OnMountCard(wxCommandEvent& evt)
{
	evt.Skip();

	const int selectedViewIndex = m_listview->GetFirstSelected();
	if( wxNOT_FOUND == selectedViewIndex ) return;

	McdSlotItem& card( GetCardForViewIndex(selectedViewIndex) );

	card.IsEnabled = !card.IsEnabled;
	m_listview->RefreshItem(selectedViewIndex);
	UpdateUI();
}
*/
/*
//text dialog: can be used for rename: wxGetTextFromUser - avih
void Panels::MemoryCardListPanel_Simple::OnRelocateCard(wxCommandEvent& evt)
{
	evt.Skip();

	const int slot = m_listview->GetFirstSelected();
	if( wxNOT_FOUND == slot ) return;

	// Issue a popup to the user that allows them to pick a new .PS2 file to serve as
	// the new host memorycard file for the slot.  The dialog has a number of warnings
	// present to reiterate that this is an advanced operation that PCSX2 may not
	// support very well (ie, might be buggy).

	m_listview->RefreshItem(slot);
	UpdateUI();
}
*/

// enter/double-click
void Panels::MemoryCardListPanel_Simple::OnItemActivated(wxListEvent& evt)
{
	const int viewIndex = m_listview->GetFirstSelected();
	if (wxNOT_FOUND == viewIndex)
		return;
	McdSlotItem& card(GetCardForViewIndex(viewIndex));

	if (card.IsPresent)
		UiRenameCard(card);
	else if (card.Slot >= 0)
		UiCreateNewCard(card); // IsPresent==false can only happen for an internal slot (vs filename on the HD), so a card can be created.
}

void Panels::MemoryCardListPanel_Simple::OnDuplicateFile(wxCommandEvent& evt)
{
	const int viewIndex = m_listview->GetFirstSelected();
	if (wxNOT_FOUND == viewIndex)
		return;
	McdSlotItem& card(GetCardForViewIndex(viewIndex));

	pxAssert(card.IsPresent);
	McdSlotItem dummy;
	UiDuplicateCard(card, dummy);
}

wxString GetPortName(int slotIndex)
{
	if (slotIndex == 0 || slotIndex == 1)
		return pxsFmt(wxString(L" ") + _("Port-%u / Multitap-%u--Port-1"), FileMcd_GetMtapPort(slotIndex) + 1, FileMcd_GetMtapPort(slotIndex) + 1);
	return pxsFmt(wxString(L" ") + _("             Multitap-%u--Port-%u"), FileMcd_GetMtapPort(slotIndex) + 1, FileMcd_GetMtapSlot(slotIndex) + 1);
}

void Panels::MemoryCardListPanel_Simple::UiAssignUnassignFile(McdSlotItem& card)
{
	pxAssert(card.IsPresent);

	if (card.Slot >= 0)
	{ //eject
		card.IsEnabled = false;
		card.IsPresent = false;
		card.Filename = L"";
		DoRefresh();
	}
	else
	{ //insert into a (UI) selected slot
		wxArrayString selections;
		int i;
		for (i = 0; i < GetNumVisibleInternalSlots(); i++)
		{
			McdSlotItem& selCard = GetCardForViewIndex(i);
			wxString sel = GetPortName(selCard.Slot) + L"   ( ";
			if (selCard.IsPresent)
				sel += selCard.Filename.GetFullName();
			else
				sel += _("Empty");
			sel += L" )";

			selections.Add(sel);
		}
		wxString title;
		title.Printf(_("Select a target port for '%s'"), WX_STR(card.Filename.GetFullName()));
		int res = wxGetSingleChoiceIndex(title, _("Insert card"), selections, this);
		if (res < 0)
			return;

		McdSlotItem& target = GetCardForViewIndex(res);
		bool en = target.IsPresent ? target.IsEnabled : true;
		RemoveCardFromSlot(card.Filename);
		target.Filename = card.Filename;
		target.IsPresent = true;
		target.IsEnabled = en;
		DoRefresh();
	}
}
void Panels::MemoryCardListPanel_Simple::OnAssignUnassignFile(wxCommandEvent& evt)
{
	const int viewIndex = m_listview->GetFirstSelected();
	if (wxNOT_FOUND == viewIndex)
		return;
	McdSlotItem& card(GetCardForViewIndex(viewIndex));

	UiAssignUnassignFile(card);
}

void Panels::MemoryCardListPanel_Simple::OnRenameFile(wxCommandEvent& evt)
{
	const int viewIndex = m_listview->GetFirstSelected();
	if (wxNOT_FOUND == viewIndex)
		return;
	McdSlotItem& card(GetCardForViewIndex(viewIndex));

	UiRenameCard(card);
}


void Panels::MemoryCardListPanel_Simple::OnListDrag(wxListEvent& evt)
{
	int selectionViewIndex = m_listview->GetFirstSelected();

	if (selectionViewIndex < 0)
		return;
	wxCustomDataObject my_data(drag_drop_format);
	my_data.SetData(sizeof(int), &selectionViewIndex);

	wxDropSource dragSource(m_listview);
	dragSource.SetData(my_data);
	/*wxDragResult result = */ dragSource.DoDragDrop(wxDrag_DefaultMove);
}

void Panels::MemoryCardListPanel_Simple::OnListSelectionChanged(wxListEvent& evt)
{
	UpdateUI();
}

void Panels::MemoryCardListPanel_Simple::OnOpenItemContextMenu(wxListEvent& evt)
{
	int idx = evt.GetIndex();

	wxMenu* junk = new wxMenu();

	if (idx != wxNOT_FOUND)
	{
		const McdSlotItem& card(GetCardForViewIndex(idx));

		if (card.IsPresent)
		{
			junk->Append(McdMenuId_AssignUnassign, card.Slot >= 0 ? _("&Eject card") : _("&Insert card ..."));
			junk->Append(McdMenuId_Duplicate, _("D&uplicate card ..."));
			junk->Append(McdMenuId_Rename, _("&Rename card ..."));
			junk->Append(McdMenuId_Create, _("&Delete card"));
			if (card.IsFormatted && !card.IsPSX)
			{
				junk->Append(McdMenuId_Convert, _("&Convert card"));
			}
		}
		else
		{
			junk->Append(McdMenuId_Create, _("&Create a new card ..."));
		}
	}
	else
	{
		junk->Append(McdMenuId_Create, _("&Create a new card ..."));
	}

	junk->AppendSeparator();

	junk->Append(McdMenuId_RefreshList, _("Re&fresh List"));

	PopupMenu(junk);
	m_listview->RefreshItem(idx);
	UpdateUI();
}

void Panels::MemoryCardListPanel_Simple::ReadFilesAtMcdFolder()
{
	//Dir enumeration/iteration code courtesy of cotton. - avih.
	while (!m_allFilesystemCards.empty())
		m_allFilesystemCards.pop_back();

	m_filesystemPlaceholderCard.Slot = -1;
	m_filesystemPlaceholderCard.IsEnabled = false;
	m_filesystemPlaceholderCard.IsPresent = false;
	m_filesystemPlaceholderCard.Filename = L"";


	wxArrayString memcardList;
	wxString filename = m_FolderPicker->GetPath().ToString();
	wxDir memcardDir(filename);
	if (memcardDir.IsOpened())
	{
		// add memory card files
		wxDir::GetAllFiles(filename, &memcardList, L"*.ps2", wxDIR_FILES);
		wxDir::GetAllFiles(filename, &memcardList, L"*.bin", wxDIR_FILES);
		wxDir::GetAllFiles(filename, &memcardList, L"*.mcd", wxDIR_FILES);
		wxDir::GetAllFiles(filename, &memcardList, L"*.mcr", wxDIR_FILES);

		// add memory card folders
		wxString dirname;
		if (memcardDir.GetFirst(&dirname, wxEmptyString, wxDIR_DIRS | wxDIR_HIDDEN))
		{
			do
			{
				wxFileName superBlockFileName(wxFileName(filename, dirname).GetFullPath(), L"_pcsx2_superblock");
				if (superBlockFileName.FileExists())
				{
					memcardList.Add(superBlockFileName.GetPath());
				}
			} while (memcardDir.GetNext(&dirname));
		}
	}


	for (uint i = 0; i < memcardList.size(); i++)
	{
		McdSlotItem currentCardFile;
		bool isOk = EnumerateMemoryCard(currentCardFile, memcardList[i], m_FolderPicker->GetPath());
		if (isOk && !isFileAssignedAndVisibleOnList(currentCardFile.Filename))
		{
			currentCardFile.Slot = -1;
			currentCardFile.IsEnabled = false;
			m_allFilesystemCards.push_back(currentCardFile);
			//DevCon.WriteLn(L"Enumerated file: '%s'", WX_STR(currentCardFile.Filename.GetFullName()) );
		}
		/*else
			DevCon.WriteLn(L"MCD folder card file skipped: '%s'", WX_STR(memcardList[i]) );*/
	}
}


bool Panels::MemoryCardListPanel_Simple::IsSlotVisible(int slotIndex) const
{
	if (slotIndex < 0 || slotIndex >= 8)
		return false;
	if (!m_MultitapEnabled[0] && 2 <= slotIndex && slotIndex <= 4)
		return false;
	if (!m_MultitapEnabled[1] && 5 <= slotIndex && slotIndex <= 7)
		return false;
	return true;
}
//whether or not this filename appears on the ports at the list (takes into account MT enabled/disabled)
bool Panels::MemoryCardListPanel_Simple::isFileAssignedAndVisibleOnList(const wxFileName cardFile) const
{
	int i;
	for (i = 0; i < 8; i++)
		if (IsSlotVisible(i) && cardFile.GetFullName() == m_Cards[i].Filename.GetFullName())
			return true;

	return false;
}

//whether or not this filename is assigned to a ports (regardless if MT enabled/disabled)
bool Panels::MemoryCardListPanel_Simple::isFileAssignedToInternalSlot(const wxFileName cardFile) const
{
	int i;
	for (i = 0; i < 8; i++)
		if (cardFile.GetFullName() == m_Cards[i].Filename.GetFullName())
			return true;

	return false;
}

void Panels::MemoryCardListPanel_Simple::RemoveCardFromSlot(const wxFileName cardFile)
{
	int i;
	for (i = 0; i < 8; i++)
		if (cardFile.GetFullName() == m_Cards[i].Filename.GetFullName())
		{
			m_Cards[i].Filename = L"";
			m_Cards[i].IsPresent = false;
			m_Cards[i].IsEnabled = false;
		}
}

int Panels::MemoryCardListPanel_Simple::GetNumFilesVisibleAsFilesystem() const
{
	return m_allFilesystemCards.size();
}


bool Panels::MemoryCardListPanel_Simple::IsNonEmptyFilesystemCards() const
{
	return GetNumFilesVisibleAsFilesystem() > 0;
}

McdSlotItem& Panels::MemoryCardListPanel_Simple::GetNthVisibleFilesystemCard(int n)
{
	return m_allFilesystemCards.at(n);
}

int Panels::MemoryCardListPanel_Simple::GetNumVisibleInternalSlots() const
{
	uint baselen = 2;
	if (m_MultitapEnabled[0])
		baselen += 3;
	if (m_MultitapEnabled[1])
		baselen += 3;
	return baselen;
}

// Interface Implementation for IMcdList
int Panels::MemoryCardListPanel_Simple::GetLength() const
{
	uint baselen = GetNumVisibleInternalSlots();
	baselen++; //filesystem placeholder
	baselen += GetNumFilesVisibleAsFilesystem();
	return baselen;
}


//Translates a list-view index (idx) to an internal memory card slot.
//This method effectively defines the arrangement of the card slots at the list view.
//The internal card slots array is fixed as sollows:
// slot 0: mcd1 (= MT1 slot 1)
// slot 1: mcd2 (= MT2 slot 1)
// slots 2,3,4: MT1 slots 2,3,4
// slots 5,6,7: MT2 slots 2,3,4
//
//however, the list-view doesn't show MT slots when this MT is disabled,
//  so the view-index should "shift" to point at the real card slot.
//While we're at it, we can alternatively enforce any other arrangment of the view by
//  using any other set of 'view-index-to-card-slot' translating that we'd like.
int Panels::MemoryCardListPanel_Simple::GetSlotIndexForViewIndex(int listViewIndex)
{
	pxAssert(0 <= listViewIndex && listViewIndex < GetNumVisibleInternalSlots());
	int targetSlot = -1;

	/*
	//this list-view arrangement is mostly kept aligned with the internal slots indexes, and only takes care
	//  of the case where MT1 is disabled (hence the MT2 slots 2,3,4 "move backwards" 3 places on the view-index)
	//  However, this arrangement it's not very intuitive to use...
	if (!m_MultitapEnabled[0] && listViewIndex>=2)
	{
		//we got an MT2 slot.
		assert(listViewIndex < 5);
		targetSlot = listViewIndex+3;
	}
	else
	{
		targetSlot=listViewIndex;//identical view-index and card slot.
	}
*/

	//This arrangement of list-view is as follows:
	//mcd1(=MT1 port 1)
	//[MT1 port 2,3,4 if MT1 is enabled]
	//mcd2(=MT2 slot 1)
	//[MT2 port 2,3,4 if MT2 is enabled]

	if (m_MultitapEnabled[0])
	{
		//MT1 enabled:
		if (1 <= listViewIndex && listViewIndex <= 3)
		{ //MT1 ports 2/3/4 move one place backwards
			targetSlot = listViewIndex + 1;
		}
		else if (listViewIndex == 4)
		{ //mcd2 (=MT2 port 1) moves 3 places forward
			targetSlot = 1;
		}
		else
		{ //mcd1 keeps it's pos as first, MT2 ports keep their pos at the end of the list.
			targetSlot = listViewIndex;
		}
	}
	else
	{
		//MT1 disabled: mcd1 and mcd2 stay put, MT2 ports 2,3,4 come next (move backwards 3 places)
		if (2 <= listViewIndex && listViewIndex <= 4)
			targetSlot = listViewIndex + 3;
		else
			targetSlot = listViewIndex;
	}

	pxAssert(0 <= targetSlot && targetSlot <= 7);
	return targetSlot;
}


McdSlotItem& Panels::MemoryCardListPanel_Simple::GetCardForViewIndex(int idx)
{
	pxAssert(-1 <= idx && idx < GetNumVisibleInternalSlots() + 1 + GetNumFilesVisibleAsFilesystem());

	if (0 <= idx && idx < GetNumVisibleInternalSlots())
		return m_Cards[GetSlotIndexForViewIndex(idx)];

	if (idx == -1 || idx == GetNumVisibleInternalSlots())
		return this->m_filesystemPlaceholderCard;

	return this->m_allFilesystemCards.at(idx - GetNumVisibleInternalSlots() - 1);
}
