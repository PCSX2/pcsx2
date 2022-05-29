/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#pragma once

#include <wx/wx.h>

#include "common/Threading.h"
#include "gui/pxEvents.h"
#include "gui/wxGuiTools.h"
#include "gui/AppTrait.h"

using namespace Threading;

class pxSynchronousCommandEvent;

// --------------------------------------------------------------------------------------
//  BaseDeletableObject
// --------------------------------------------------------------------------------------
// Oh the fruits and joys of multithreaded C++ coding conundrums!  This class provides a way
// to be deleted from arbitraty threads, or to delete themselves (which is considered unsafe
// in C++, though it does typically work).  It also gives objects a second recourse for
// doing fully virtualized cleanup, something C++ also makes impossible because of how it
// implements it's destructor hierarchy.
//
// To utilize virtual destruction, override DoDeletion() and be sure to invoke the base class
// implementation of DoDeletion().
//
// Assertions:
//   This class generates an assertion of the destructor is called from anything other than
//   the main/gui thread.
//
// Rationale:
//   wxWidgets provides a pending deletion feature, but it's specific to wxCore (not wxBase)
//   which means it requires wxApp and all that, which is bad for plugins and the possibility
//   of linking PCSX2 core against a non-WX gui in the future.  It's also not thread safe
//   (sigh).  And, finally, it requires quite a bit of red tape to implement wxObjects because
//   of the wx-custom runtime type information.  So I made my own.
//
class BaseDeletableObject
{
protected:
	std::atomic<bool> m_IsBeingDeleted;

public:
	BaseDeletableObject();
	virtual ~BaseDeletableObject();

	virtual void DeleteSelf();
	virtual bool IsBeingDeleted() { return !!m_IsBeingDeleted; }

	// Returns FALSE if the object is already marked for deletion, or TRUE if the app
	// should schedule the object for deletion.  Only schedule if TRUE is returned, otherwise
	// the object could get deleted twice if two threads try to schedule it at the same time.
	bool MarkForDeletion();

protected:
	// This function is GUI implementation dependent!  It's implemented by PCSX2's AppHost,
	// but if the SysCore is being linked to another front end, you'll need to implement this
	// yourself.  Most GUIs have built in message pumps.  If a platform lacks one then you'll
	// need to implement one yourself (yay?).
	virtual void DoDeletion();
};

// --------------------------------------------------------------------------------------
//  pxAppLog / ConsoleLogSource_App
// --------------------------------------------------------------------------------------

class ConsoleLogSource_App : public ConsoleLogSource
{
	typedef ConsoleLogSource _parent;

public:
	ConsoleLogSource_App();
};

extern ConsoleLogSource_App pxConLog_App;

#define pxAppLog pxConLog_App.IsActive() && pxConLog_App


// --------------------------------------------------------------------------------------
//  ModalButtonPanel
// --------------------------------------------------------------------------------------
class ModalButtonPanel : public wxPanelWithHelpers
{
public:
	ModalButtonPanel(wxWindow* window, const MsgButtons& buttons);
	virtual ~ModalButtonPanel() = default;

	virtual void AddActionButton(wxWindowID id);
	virtual void AddCustomButton(wxWindowID id, const wxString& label);

	virtual void OnActionButtonClicked(wxCommandEvent& evt);
};

typedef std::list<wxEvent*> wxEventList;

// --------------------------------------------------------------------------------------
//  wxAppWithHelpers
// --------------------------------------------------------------------------------------
class wxAppWithHelpers : public wxApp
{
	typedef wxApp _parent;

	wxDECLARE_DYNAMIC_CLASS(wxAppWithHelpers);

protected:
	wxEventList m_IdleEventQueue;
	Threading::MutexRecursive m_IdleEventMutex;
	wxTimer m_IdleEventTimer;

public:
	wxAppWithHelpers();
	virtual ~wxAppWithHelpers() {}

	wxAppTraits* CreateTraits();

	void CleanUp();

	void DeleteObject(BaseDeletableObject& obj);
	void DeleteObject(BaseDeletableObject* obj)
	{
		if (obj == NULL)
			return;
		DeleteObject(*obj);
	}

	void DeleteThread(Threading::pxThread& obj);
	void DeleteThread(Threading::pxThread* obj)
	{
		if (obj == NULL)
			return;
		DeleteThread(*obj);
	}

	void PostCommand(void* clientData, int evtType, int intParam = 0, long longParam = 0, const wxString& stringParam = wxEmptyString);
	void PostCommand(int evtType, int intParam = 0, long longParam = 0, const wxString& stringParam = wxEmptyString);
	void PostMethod(FnType_Void* method);
	void PostIdleMethod(FnType_Void* method);
	void ProcessMethod(FnType_Void* method);

	bool Rpc_TryInvoke(FnType_Void* method);
	bool Rpc_TryInvokeAsync(FnType_Void* method);

	sptr ProcessCommand(void* clientData, int evtType, int intParam = 0, long longParam = 0, const wxString& stringParam = wxEmptyString);
	sptr ProcessCommand(int evtType, int intParam = 0, long longParam = 0, const wxString& stringParam = wxEmptyString);

	void ProcessAction(pxActionEvent& evt);
	void PostAction(const pxActionEvent& evt);

	void Ping();
	bool OnInit();
	//int  OnExit();

	void AddIdleEvent(const wxEvent& evt);

	void PostEvent(const wxEvent& evt);
	bool ProcessEvent(wxEvent& evt);
	bool ProcessEvent(wxEvent* evt);

	bool ProcessEvent(pxActionEvent& evt);
	bool ProcessEvent(pxActionEvent* evt);

protected:
	void IdleEventDispatcher(const wxChar* action = wxEmptyString);

	void OnIdleEvent(wxIdleEvent& evt);
	void OnStartIdleEventTimer(wxCommandEvent& evt);
	void OnIdleEventTimeout(wxTimerEvent& evt);
	void OnDeleteObject(wxCommandEvent& evt);
	void OnDeleteThread(wxCommandEvent& evt);
	void OnSynchronousCommand(pxSynchronousCommandEvent& evt);
	void OnInvokeAction(pxActionEvent& evt);
};

namespace Msgbox
{
	extern int ShowModal(BaseMessageBoxEvent& evt);
	extern int ShowModal(const wxString& title, const wxString& content, const MsgButtons& buttons);
} // namespace Msgbox
