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

#include "Console.h"
#include "common/Assertions.h"
#include "common/StringUtil.h"

// --------------------------------------------------------------------------------------
//  TraceLogDescriptor
// --------------------------------------------------------------------------------------
// Provides textual information for use by UIs; to give the end user a selection screen for
// enabling/disabling logs, and also for saving the log settings to INI.
//
struct TraceLogDescriptor
{
	// short name, alphanumerics only: used for saving/loading options.
	const char* ShortName;

	// Standard UI name for this log source.  Used in menus, options dialogs.
	const char* Name;

	// Length description for use as a tooltip or menu item description.
	const char* Description;

	const char* GetShortName() const
	{
		pxAssumeDev(Name, "Tracelog descriptors require a valid name!");
		return ShortName ? ShortName : Name;
	}
};

// --------------------------------------------------------------------------------------
//  BaseTraceLogSource
// --------------------------------------------------------------------------------------
// This class houses the base attributes for any trace log (to file or to console), which
// only includes the logfile's name, description, and enabled bit (for UIs and ini files),
// and an IsActive() method for determining if the log should be written or not.
//
// Derived classes then provide their own Write/DoWrite functions that format and write
// the log to the intended target(s).
//
// All individual calls to log write functions should be responsible for checking the
// status of the log via the IsActive() method manually (typically done via macro).  This
// is done in favor of internal checks because most logs include detailed/formatted
// information, which itself can take a lot of cpu power to prepare.  If the IsActive()
// check is done top-level, the parameters' calculations can be skipped.  If the IsActive()
// check is done internally as part of the Write/Format calls, all parameters have to be
// resolved regardless of if the log is actually active.
//
class BaseTraceLogSource
{
protected:
	const TraceLogDescriptor* m_Descriptor;

public:
	// Indicates if the user has enabled this specific log.  This boolean only represents
	// the configured status of this log, and does *NOT* actually mean the log is active
	// even when TRUE.  Because many tracelogs have master enablers that act on a group
	// of logs, logging checks should always use IsActive() instead to determine if a log
	// should be processed or not.
	bool Enabled;

protected:
	BaseTraceLogSource()
		: m_Descriptor(NULL)
		, Enabled(false)
	{
	}

public:
		BaseTraceLogSource(const TraceLogDescriptor* desc)
	{
		pxAssumeDev(desc, "Trace logs must have a valid (non-NULL) descriptor.");
		Enabled = false;
		m_Descriptor = desc;
	}

	// Provides a categorical identifier, typically in "group.subgroup.subgroup" form.
	// (use periods in favor of colons, since they do not require escape characters when
	// written to ini/config files).
	virtual std::string GetCategory() const { return std::string(); }

	// This method should be used to determine if a log should be generated or not.
	// See the class overview comments for details on how and why this method should
	// be used.
	virtual bool IsActive() const { return Enabled; }

	virtual const char* GetShortName() const { return m_Descriptor->GetShortName(); }
	virtual const char* GetName() const { return m_Descriptor->Name; }
	virtual const char* GetDescription() const
	{
		return (m_Descriptor->Description != NULL) ? m_Descriptor->Description : "";
	}

	virtual bool HasDescription() const { return m_Descriptor->Description != NULL; }
};

// --------------------------------------------------------------------------------------
//  TextFileTraceLog
// --------------------------------------------------------------------------------------
// This class is tailored for performance logging to file.  It does not support console
// colors or wide/unicode text conversion.
//
class TextFileTraceLog : public BaseTraceLogSource
{
public:
	TextFileTraceLog(const TraceLogDescriptor* desc)
		: BaseTraceLogSource(desc)
	{
	}

	bool Write(const char* fmt, ...) const
	{
		va_list list;
		va_start(list, fmt);
		WriteV(fmt, list);
		va_end(list);

		return false;
	}

	bool WriteV(const char* fmt, va_list list) const
	{
		std::string ascii;
		ApplyPrefix(ascii);
		ascii += StringUtil::StdStringFromFormatV(fmt, list);
		DoWrite(ascii.c_str());
		return false;
	}

	virtual void ApplyPrefix(std::string& ascii) const {}
	virtual void DoWrite(const char* fmt) const = 0;
};

// --------------------------------------------------------------------------------------
//  ConsoleLogSource
// --------------------------------------------------------------------------------------
// This class is tailored for logging to console.  It applies default console color attributes
// to all writes, and supports both char and wxChar (Ascii and UF8/16) formatting.
//
class ConsoleLogSource : public BaseTraceLogSource
{
public:
	ConsoleColors DefaultColor;

protected:
	ConsoleLogSource()
		: DefaultColor(Color_Gray)
	{
	}

public:
		ConsoleLogSource(const TraceLogDescriptor* desc, ConsoleColors defaultColor = Color_Gray)
		: BaseTraceLogSource(desc)
	{
		DefaultColor = defaultColor;
	}

	// Writes to the console using the source's default color.  Note that the source's default
	// color will always be used, thus ConsoleColorScope() will not be effectual unless the
	// console's default color is Color_Default.
	bool Write(const char* fmt, ...) const
	{
		va_list list;
		va_start(list, fmt);
		WriteV(fmt, list);
		va_end(list);

		return false;
	}

	// Writes to the console using the specified color.  This overrides the default color setting
	// for this log.
	bool Write(ConsoleColors color, const char* fmt, ...) const
	{
		va_list list;
		va_start(list, fmt);
		WriteV(color, fmt, list);
		va_end(list);

		return false;
	}

	bool WriteV(const char* fmt, va_list list) const;

	bool WriteV(ConsoleColors color, const char* fmt, va_list list) const;
};
