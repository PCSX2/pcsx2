/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2008  Pcsx2 Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef _PCSX2_EXCEPTIONS_H_
#define _PCSX2_EXCEPTIONS_H_

#include <string>

namespace Exception
{
	// This exception  exception thrown any time an operation is attempted when an object
	// is in an uninitialized state.
	class InvalidOperation : public std::logic_error
	{
	public:
		virtual ~InvalidOperation() throw() {}
		explicit InvalidOperation( const std::string& msg="Attempted method call is invalid for the current object or program state." ) :
			logic_error( msg ) {}
	};

	// This exception is thrown by the PS2 emulation (R5900, etc) when bad things happen
	// that force the emulation state to terminate.  The GUI should handle them by returning
	// the user to the GUI.
	class CpuStateShutdown : public std::runtime_error
	{
	public:
		virtual ~CpuStateShutdown() throw() {}
		explicit CpuStateShutdown( const std::string& msg="The PS2 emulated state was shut down unexpectedly." ) :
			runtime_error( msg ) {}
	};

	// Exception thrown by SaveState class when a critical plugin or gzread
	class FreezePluginFailure : public std::runtime_error
	{
	public:
		std::string plugin_name;		// name of the plugin
		std::string freeze_action;

		virtual ~FreezePluginFailure() throw() {}
		explicit FreezePluginFailure( const std::string& plugin, const std::string& action ) :
			runtime_error( plugin + " plugin returned an error while " + action + " the state." )
		,	plugin_name( plugin )
		,	freeze_action( action ){}
	};

	class UnsupportedStateVersion : public std::runtime_error
	{
	public:
		virtual ~UnsupportedStateVersion() throw() {}
		explicit UnsupportedStateVersion( const std::string& msg="Unknown or unsupported savestate version." ) :
			runtime_error( msg ) {}
	};

	class PluginFailure : public std::runtime_error
	{
	public:
		std::string plugin_name;		// name of the plugin

		virtual ~PluginFailure() throw() {}
		explicit PluginFailure( const std::string& plugin, const std::string& msg = "An error occured in the " ) :
			runtime_error( plugin + msg + " Plugin" )
		,	plugin_name( plugin ) {}
	};

	/**** BEGIN STREAMING EXCEPTIONS ****/

	// Generic stream error.  Contains the name of the stream and a message.
	// This exception is usually thrown via derrived classes, except in the (rare) case of a generic / unknown error.
	class Stream : public std::runtime_error
	{
	public:
		std::string stream_name;		// name of the stream (if applicable)

		virtual ~Stream() throw() {}

		// copy construct!
		Stream( const Stream& src ) :
			std::runtime_error( src.what() )
		,	stream_name( src.stream_name ) {}

		explicit Stream(
			const std::string& objname=std::string(),
			const std::string& msg="Invalid stream object" ) :
		  std::runtime_error( msg + ": " + objname )
		, stream_name( objname ) {}
	};

	// Exception thrown when a corrupted or truncated savestate is encountered.
	class BadSavedState : public Stream
	{
	public:
		virtual ~BadSavedState() throw() {}
		explicit BadSavedState(
			const std::string& objname=std::string(),
			const std::string& msg="Corrupted data or end of file encountered while loading savestate" ) :
		Stream( objname, msg ) {}
	};

	// Exception thrown when an attempt to open a non-existant file is made.
	// (this exception can also mean file permissions are invalid)
	class FileNotFound : public Stream
	{
	public:
		virtual ~FileNotFound() throw() {}
		explicit FileNotFound(
			const std::string& objname=std::string(),
			const std::string& msg="File not found or permission denied" ) :
		Stream( objname, msg ) {}
	};
}

#endif
