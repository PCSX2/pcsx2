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

#include "PrecompiledHeader.h"
#include "App.h"
#include "IniInterface.h"
#include "common/EventSource.inl"

template class EventSource< IEventListener_CoreThread >;
template class EventSource< IEventListener_AppStatus >;

AppSettingsEventInfo::AppSettingsEventInfo( IniInterface& ini, AppEventType evt_type )
	: AppEventInfo( evt_type )
	, m_ini( ini )
{
}

EventListener_CoreThread::EventListener_CoreThread()
{
	wxGetApp().AddListener( this );
}

EventListener_CoreThread::~EventListener_CoreThread()
{
	wxGetApp().RemoveListener( this );
}

void IEventListener_CoreThread::DispatchEvent( const CoreThreadStatus& status )
{
	switch( status )
	{
		case CoreThread_Indeterminate: break;

		case CoreThread_Started:	CoreThread_OnStarted();			break;
		case CoreThread_Resumed:	CoreThread_OnResumed();			break;
		case CoreThread_Suspended:	CoreThread_OnSuspended();		break;
		case CoreThread_Reset:		CoreThread_OnReset();			break;
		case CoreThread_Stopped:	CoreThread_OnStopped();			break;
		
		jNO_DEFAULT;
	}
}

EventListener_AppStatus::EventListener_AppStatus()
{
	wxGetApp().AddListener( this );
}

EventListener_AppStatus::~EventListener_AppStatus()
{
	wxGetApp().RemoveListener( this );
}

void IEventListener_AppStatus::DispatchEvent( const AppEventInfo& evtinfo )
{
	switch( evtinfo.evt_type )
	{
		case AppStatus_UiSettingsLoaded:
		case AppStatus_UiSettingsSaved:
			AppStatusEvent_OnUiSettingsLoadSave( (const AppSettingsEventInfo&)evtinfo );
		break;

		case AppStatus_VmSettingsLoaded:
		case AppStatus_VmSettingsSaved:
			AppStatusEvent_OnVmSettingsLoadSave( (const AppSettingsEventInfo&)evtinfo );
		break;

		case AppStatus_SettingsApplied:
			AppStatusEvent_OnSettingsApplied();
		break;

		case AppStatus_Exiting:
			AppStatusEvent_OnExit();
		break;
	}
}

void Pcsx2App::DispatchEvent( AppEventType evt )
{
	if( !AffinityAssert_AllowFrom_MainUI() ) return;
	m_evtsrc_AppStatus.Dispatch( AppEventInfo( evt ) );
}

void Pcsx2App::DispatchEvent( CoreThreadStatus evt )
{
	// Clear the sticky key statuses, because hell knows what'll change while PAD
	// is suspended.

	m_kevt.m_shiftDown		= false;
	m_kevt.m_controlDown	= false;
	m_kevt.m_altDown		= false;

	m_evtsrc_CoreThreadStatus.Dispatch( evt );
	ScopedBusyCursor::SetDefault( Cursor_NotBusy );
	CoreThread.RethrowException();
}

void Pcsx2App::DispatchUiSettingsEvent( IniInterface& ini )
{
	if( !AffinityAssert_AllowFrom_MainUI() ) return;
	m_evtsrc_AppStatus.Dispatch( AppSettingsEventInfo( ini, ini.IsSaving() ? AppStatus_UiSettingsSaved : AppStatus_UiSettingsLoaded ) );
}

void Pcsx2App::DispatchVmSettingsEvent( IniInterface& ini )
{
	if( !AffinityAssert_AllowFrom_MainUI() ) return;
	m_evtsrc_AppStatus.Dispatch( AppSettingsEventInfo( ini, ini.IsSaving() ? AppStatus_VmSettingsSaved : AppStatus_VmSettingsLoaded ) );
}


// --------------------------------------------------------------------------------------
//  CoreThreadStatusEvent Implementations
// --------------------------------------------------------------------------------------
CoreThreadStatusEvent::CoreThreadStatusEvent( CoreThreadStatus evt, SynchronousActionState* sema )
	: pxActionEvent( sema )
{
	m_evt = evt;
}

CoreThreadStatusEvent::CoreThreadStatusEvent( CoreThreadStatus evt, SynchronousActionState& sema )
	: pxActionEvent( sema )
{
	m_evt = evt;
}

void CoreThreadStatusEvent::InvokeEvent()
{
	sApp.DispatchEvent( m_evt );
}
