/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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
#include "CtrlMemSearch.h"
#include "DebugTools/Debug.h"
#include "gui/AppConfig.h"
#include "common/BitCast.h"
#include "common/StringUtil.h"
#include "Patch.h" // Required for SwapEndian :(


#include "BreakpointWindow.h"
#include "DebugEvents.h"
#include "DisassemblyDialog.h"
#include <wchar.h>
#include <wx/clipbrd.h>

wxDEFINE_EVENT(pxEvt_SearchFinished, wxCommandEvent);

wxBEGIN_EVENT_TABLE(CtrlMemSearch, wxWindow)
	EVT_SET_FOCUS(CtrlMemSearch::focusEvent)
	EVT_KILL_FOCUS(CtrlMemSearch::focusEvent)
	EVT_SIZE(CtrlMemSearch::sizeEvent)
wxEND_EVENT_TABLE()

enum MemoryViewMenuIdentifiers {
	ID_MEMVIEW_GOTOINDISASM = 1,
	ID_MEMVIEW_GOTOADDRESS,
	ID_MEMVIEW_COPYADDRESS,
	ID_MEMVIEW_FOLLOWADDRESS,
	ID_MEMVIEW_DISPLAYVALUE_8,
	ID_MEMVIEW_DISPLAYVALUE_16,
	ID_MEMVIEW_DISPLAYVALUE_32,
	ID_MEMVIEW_DISPLAYVALUE_64,
	ID_MEMVIEW_DISPLAYVALUE_128,
	ID_MEMVIEW_COPYVALUE_8,
	ID_MEMVIEW_COPYVALUE_16,
	ID_MEMVIEW_COPYVALUE_32,
	ID_MEMVIEW_COPYVALUE_64,
	ID_MEMVIEW_COPYVALUE_128,
	ID_MEMVIEW_ALIGNWINDOW,
};

// Kind of lame, but it works.
const wxString wxStringTypes[] =
	{
		"Byte",
		"2 Bytes",
		"4 Bytes",
		"8 Bytes",
		"Float",
		"Double",
		"String"};
const wxArrayString SearchTypes = wxArrayString(7, wxStringTypes);

// StringUtils::FromChars doesn't appreciate the leading 0x.
__fi wxString CheckHexadecimalString(wxString s)
{
		if(s.StartsWith("0x"))
			return s.Remove(0, 2);
		return s;
}

CtrlMemSearch::CtrlMemSearch(wxWindow* parent, DebugInterface* _cpu)
	: wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
	, cpu(_cpu)
{

	m_SearchThread.reset(new SearchThread(this, _cpu));
	m_SearchThread->SetName("Debugger Memory Search");

	Bind(pxEvt_SearchFinished, &CtrlMemSearch::onSearchFinished, this);

	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

	txtSearch = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(250, -1));
	btnSearch = new wxButton(this, wxID_ANY, L"Search", wxDefaultPosition);

	Bind(wxEVT_BUTTON, &CtrlMemSearch::Search, this, btnSearch->GetId());

	cmbSearchType = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(250, -1), 0, NULL, wxCB_READONLY);
	cmbSearchType->Set(SearchTypes);
	cmbSearchType->SetSelection(0);

	Bind(wxEVT_COMBOBOX, &CtrlMemSearch::onSearchTypeChanged, this, cmbSearchType->GetId());

	chkHexadecimal = new wxCheckBox(this, wxID_ANY, L"Hex");
	chkHexadecimal->SetValue(true);

	radBigEndian = new wxRadioButton(this, wxID_ANY, L"Big Endian", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	radLittleEndian = new wxRadioButton(this, wxID_ANY, L"Little Endian");
	radLittleEndian->SetValue(true);

	txtMemoryStart = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(250, -1));
	txtMemoryStart->SetValue(L"0x0");
	txtMemoryEnd = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(250, -1));
	if (_cpu->getCpuType() == BREAKPOINT_EE)
		txtMemoryEnd->SetValue(L"0x200000");
	else
		txtMemoryEnd->SetValue(L"0x20000");

	lblSearchHits = new wxStaticText(this, wxID_ANY, L"Hits: 0");
	lblSearchHitSelected = new wxStaticText(this, wxID_ANY, L"Cur: 0");

	btnNext = new wxButton(this, wxID_ANY, L"Next", wxDefaultPosition);
	btnNext->Enable(false);
	Bind(wxEVT_BUTTON, &CtrlMemSearch::onSearchNext, this, btnNext->GetId());
	btnPrev = new wxButton(this, wxID_ANY, L"Prev", wxDefaultPosition);
	btnPrev->Enable(false);
	Bind(wxEVT_BUTTON, &CtrlMemSearch::onSearchPrev, this, btnPrev->GetId());

	//  GUI design

	// Search box and search button
	wxBoxSizer* searchSizer = new wxBoxSizer(wxHORIZONTAL);
	searchSizer->Add(txtSearch, 0);
	searchSizer->Add(btnSearch, 0, wxLEFT, 5);

	mainSizer->Add(searchSizer, 0, wxUP | wxLEFT | wxRIGHT, 5);

	// Search type combo box and hexadecimal checkbox
	wxBoxSizer* searchTypeSizer = new wxBoxSizer(wxHORIZONTAL);
	searchTypeSizer->Add(cmbSearchType, 0, wxEXPAND);
	searchTypeSizer->Add(chkHexadecimal, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, 5);

	mainSizer->Add(searchTypeSizer, 0, wxUP | wxLEFT | wxRIGHT, 5);

	// Big / Little Endianess radio button
	wxBoxSizer* endianessSizer = new wxBoxSizer(wxHORIZONTAL);
	endianessSizer->Add(radLittleEndian, 0, wxALIGN_CENTER_VERTICAL);
	endianessSizer->Add(radBigEndian, 0, wxLEFT | wxALIGN_CENTER_VERTICAL, 5);

	mainSizer->Add(endianessSizer, 0, wxUP | wxLEFT | wxRIGHT, 5);

	// Address range type text boxes
	wxFlexGridSizer* addrRangeSizer = new wxFlexGridSizer(2, 2, 5);

	addrRangeSizer->Add(new wxStaticText(this, wxID_ANY, L"Start Address"), 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
	addrRangeSizer->Add(txtMemoryStart, 0, wxEXPAND);

	addrRangeSizer->Add(new wxStaticText(this, wxID_ANY, L"End Address"), 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
	addrRangeSizer->Add(txtMemoryEnd, 0, wxEXPAND);

	mainSizer->Add(addrRangeSizer, 0, wxUP | wxLEFT | wxRIGHT, 5);

	wxBoxSizer* resultSizer = new wxBoxSizer(wxHORIZONTAL);
	resultSizer->Add(lblSearchHits, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	resultSizer->Add(lblSearchHitSelected, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	resultSizer->Add(btnPrev, 0, wxRIGHT, 5);
	resultSizer->Add(btnNext, 0, wxRIGHT, 5);

	mainSizer->Add(resultSizer, 0, wxUP | wxLEFT | wxRIGHT, 5);
	this->SetSizer(mainSizer);
	SetDoubleBuffered(true);
}

void CtrlMemSearch::postEvent(wxEventType type, wxString text)
{
	wxCommandEvent event(type, GetId());
	event.SetEventObject(this);
	event.SetClientData(cpu);
	event.SetString(text);
	wxPostEvent(this, event);
}

void CtrlMemSearch::postEvent(wxEventType type, int value)
{
	wxCommandEvent event(type, GetId());
	event.SetEventObject(this);
	event.SetClientData(cpu);
	event.SetInt(value);
	wxPostEvent(this, event);
}

// Set up the thread with out UI params
// Then execute the thread
void CtrlMemSearch::Search(wxCommandEvent& evt)
{
	if (!m_searchResultsMutex.try_lock())
	{
		// Uh, if it doesn't lock then means we somehow tried to search while
		// the search was still running.
		return;
	}
	m_searchResults.clear();
	m_searchResultsMutex.unlock();

	lblSearchHits->SetLabelText(L"Hits: 0");
	btnNext->Enable(false);
	btnPrev->Enable(false);

	u64 startAddress = 0;
	u64 endAddress = 0;

	const auto resStart = StringUtil::FromChars<u64>(CheckHexadecimalString(txtMemoryStart->GetValue()).ToStdString(), 16);
	if (!resStart.has_value())
	{
		wxMessageBox(L"Invalid start address", L"Error", wxOK | wxICON_ERROR);
		return;
	}
	startAddress = resStart.value();

	const auto resEnd = StringUtil::FromChars<u64>(CheckHexadecimalString(txtMemoryEnd->GetValue()).ToStdString(), 16);
	if (!resStart.has_value())
	{
		wxMessageBox(L"Invalid end address", L"Error", wxOK | wxICON_ERROR);
		return;
	}
	endAddress = resEnd.value();

	m_SearchThread->m_start = startAddress;
	m_SearchThread->m_end = endAddress;
	m_SearchThread->m_type = static_cast<SEARCHTYPE>(cmbSearchType->GetSelection());

	// Prepare the search value for the thread

	wxString searchString = txtSearch->GetValue();
	if (m_SearchThread->m_type == SEARCHTYPE::STRING)
	{
		m_SearchThread->m_value_string = searchString.ToUTF8();
	}
	else if (chkHexadecimal->IsChecked())
	{
		const auto res = StringUtil::FromChars<u64>(CheckHexadecimalString(txtSearch->GetValue()).ToStdString(), 16);
		if (!res.has_value())
		{
			wxMessageBox(L"Invalid hexadecimal value", L"Error", wxOK | wxICON_ERROR);
			return;
		}
		m_SearchThread->m_value = radBigEndian->GetValue() ? SwapEndian(res.value(), SEARCHTYPEBITS[cmbSearchType->GetSelection()]) : res.value();
	}
	else
	{
		// According to my logic, we end up here if we aren't searching for a string
		// Aren't in hexadecimal mode.

		// Let's check if we're searching for a signed or unsigned value
		// The thread will handle the interpretation for us.
		m_SearchThread->m_signed = txtSearch->GetValue().First('-');

		// Now we can convert the string to a number

		if (m_SearchThread->m_type == SEARCHTYPE::FLOAT || m_SearchThread->m_type == SEARCHTYPE::DOUBLE)
		{
			// We're searching for a float or double, so we need to convert the string to a double
			const auto res = StringUtil::FromChars<double>(txtSearch->GetValue().ToStdString());
			if (!res.has_value())
			{
				wxMessageBox(L"Invalid floating point value", L"Error", wxOK | wxICON_ERROR);
				return;
			}

			if (m_SearchThread->m_type == SEARCHTYPE::FLOAT)
			{
				const u32 u32SearchValue = bit_cast<u32, float>(res.value());
				m_SearchThread->m_value = u32SearchValue;
			}
			else
			{
				const u64 u64SearchValue = bit_cast<u64, double>(res.value());
				m_SearchThread->m_value = u64SearchValue;
			}
		}
		else
		{
			// We're searching for either a BYTE, WORD, DWORD or QWORD
			
			const auto res = StringUtil::FromChars<u64>(txtSearch->GetValue().ToStdString());
			if(!res.has_value())
			{
				wxMessageBox(L"Invalid decimal search value", L"Error", wxOK | wxICON_ERROR);
				return;
			}

			// We don't need to bitcast here, because the thread already has the correct value type of u64
			m_SearchThread->m_value = radBigEndian->GetValue() ? SwapEndian(res.value(), SEARCHTYPEBITS[cmbSearchType->GetSelection()]) : res.value();
		}
	}

	m_SearchThread->Start();
};

void CtrlMemSearch::onSearchFinished(wxCommandEvent& evt)
{
	// We're done searching, so let's update the UI

	m_searchResultsMutex.lock();

	lblSearchHits->SetLabelText(wxString::Format(L"Hits: %zu", m_searchResults.size()));
	
	// Enable the buttons only if we have results
	// -1 indicates we haven't jumped to a result yet
	m_searchIter = -1;
	lblSearchHitSelected->SetLabelText(L"Cur: 0");
	btnNext->Enable(m_searchResults.size() > 0);
	btnPrev->Enable(m_searchResults.size() > 0);

	m_searchResultsMutex.unlock();

	this->Layout(); // This makes sure the search hit count label doesn't get cut off
}

void CtrlMemSearch::onSearchNext(wxCommandEvent& evt)
{
	m_searchIter++;
	if (m_searchIter == m_searchResults.size())
	{
		m_searchIter = 0;
	}

	lblSearchHitSelected->SetLabelText(wxString::Format(L"Cur: %d",m_searchIter + 1));
	this->Layout();
	postEvent(debEVT_GOTOINMEMORYVIEW, m_searchResults[m_searchIter]);
}

void CtrlMemSearch::onSearchPrev(wxCommandEvent& evt)
{
	m_searchIter--;
	// Unsigned underflow
	if (m_searchIter > m_searchResults.size())
	{
		m_searchIter = m_searchResults.size() - 1;
	}

	lblSearchHitSelected->SetLabelText(wxString::Format(L"Cur: %d",m_searchIter + 1));
	this->Layout();
	postEvent(debEVT_GOTOINMEMORYVIEW, m_searchResults[m_searchIter]);
}

void CtrlMemSearch::onSearchTypeChanged(wxCommandEvent& evt)
{
	// Update the search value text box to match the search type
	switch (static_cast<SEARCHTYPE>(cmbSearchType->GetSelection()))
	{
		case SEARCHTYPE::BYTE:
		case SEARCHTYPE::WORD:
		case SEARCHTYPE::DWORD:
		case SEARCHTYPE::QWORD:
			radBigEndian->Enable(true);
			radLittleEndian->Enable(true);
			chkHexadecimal->Enable(true);
			break;
		case SEARCHTYPE::FLOAT:
		case SEARCHTYPE::DOUBLE:
		case SEARCHTYPE::STRING:
			radBigEndian->Enable(false);
			radLittleEndian->Enable(false);
			chkHexadecimal->Enable(false);
			chkHexadecimal->SetValue(false);
			break;
	}
}

// -- SearchThread

// There might be a prettier way to read from the vtlb
template <class T>
void memSearch(std::vector<u32>& out, u32 start, u32 end, T value, DebugInterface* cpu)
{
	for (u32 addr = start; addr < end; addr += sizeof(T))
	{
		if (!cpu->isValidAddress(addr))
		{
			continue;
		}

		T val = 0;
		if (sizeof(T) == 1)
		{
			val = cpu->read8(addr);
		}
		else if (sizeof(T) == 2)
		{
			val = cpu->read16(addr);
		}
		else if (sizeof(T) == 4)
		{
			if (std::is_same<T, float>::value)
			{
				float fpVal = bit_cast<float, u32>(value);
				// Love floating point
				if (bit_cast<float, u32>(cpu->read32(addr)) < fpVal + 0.001f && bit_cast<float, u32>(cpu->read32(addr)) > fpVal - 0.001f)
				{
					out.push_back(addr);
				}
				continue;
			}

			val = cpu->read32(addr);
		}
		else if (sizeof(T) == 8)
		{
			if (std::is_same<T, double>::value)
			{
				double dbVal = bit_cast<double, u64>(value);
				if (bit_cast<double, u64>(cpu->read64(addr)) < dbVal + 0.001 && bit_cast<double, u64>(cpu->read64(addr)) > dbVal - 0.001)
				{
					out.push_back(addr);
				}
				continue;
			}

			val = cpu->read64(addr);
		}

		if (val == value)
		{
			out.push_back(addr);
		}
	}
}

void SearchThread::ExecuteTaskInThread()
{
	// First, make sure we can lock the mutex
	if (!m_parent->m_searchResultsMutex.try_lock())
	{
		// We couldn't lock the mutex, this thread either didn't unlock, or we've somehow raced
		// with the main UI thread ?
		assert("Failed to lock our memory search mutex?");
		return;
	}

	switch (m_type)
	{
		case SEARCHTYPE::BYTE:
			if (m_signed)
			{
				memSearch<s8>(m_parent->m_searchResults, m_start, m_end, m_value, m_cpu);
			}
			else
			{
				memSearch<u8>(m_parent->m_searchResults, m_start, m_end, m_value, m_cpu);
			}
			break;
		case SEARCHTYPE::WORD:
			if (m_signed)
			{
				memSearch<s16>(m_parent->m_searchResults, m_start, m_end, m_value, m_cpu);
			}
			else
			{
				memSearch<u16>(m_parent->m_searchResults, m_start, m_end, m_value, m_cpu);
			}
			break;
		case SEARCHTYPE::DWORD:
			if (m_signed)
			{
				memSearch<s32>(m_parent->m_searchResults, m_start, m_end, m_value, m_cpu);
			}
			else
			{
				memSearch<u32>(m_parent->m_searchResults, m_start, m_end, m_value, m_cpu);
			}
			break;
		case SEARCHTYPE::QWORD:
			if (m_signed)
			{
				memSearch<s64>(m_parent->m_searchResults, m_start, m_end, m_value, m_cpu);
			}
			else
			{
				memSearch<u64>(m_parent->m_searchResults, m_start, m_end, m_value, m_cpu);
			}
			break;
		case SEARCHTYPE::FLOAT:
			memSearch<float>(m_parent->m_searchResults, m_start, m_end, m_value, m_cpu);
			break;
		case SEARCHTYPE::DOUBLE:
			memSearch<double>(m_parent->m_searchResults, m_start, m_end, m_value, m_cpu);
			break;
		case SEARCHTYPE::STRING:
			for (u32 addr = m_start; addr < m_end; addr++)
			{
				// There is no easy way to do this with the vtable.
				bool match = true;
				for (u32 stringIndex = 0; stringIndex < m_value_string.size(); stringIndex++)
				{
					if ((char)m_cpu->read8(addr + stringIndex) != m_value_string[stringIndex])
					{
						match = false;
						break;
					}
				}

				if (match)
				{
					m_parent->m_searchResults.push_back(addr);
					addr += m_value_string.size() - 1;
				}
			}
			break;
	}

	m_parent->m_searchResultsMutex.unlock();

	wxCommandEvent done(pxEvt_SearchFinished);
	m_parent->GetEventHandler()->AddPendingEvent(done);
}
