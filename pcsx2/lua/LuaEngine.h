#pragma once
#ifndef __LUA_ENGINE_H__
#define __LUA_ENGINE_H__


extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

class LuaEngine {
public:
	LuaEngine();
	~LuaEngine() {}
public:
	enum LuaState
	{
		NONE,
		NOT_OPEN,
		OPEN,
		RUNNING,
		RESUME,
	};
	void setState(LuaState _state);
	LuaState getState() { return state; }

	void setFileName(wxString filename);
	bool Load();
	void Resume();
	void Close();

	bool isSelf(lua_State* l) { return (l == Lthread); }

	// registry
	void registryBefore(int ref) { refCallBefore = ref; }
	void registryAfter(int ref) { refCallAfter = ref; }
	void registryExit(int ref) { refCallExit = ref; }
	void unRegistryBefore();
	void unRegistryAfter();
	void unRegistryExit();
	void callBefore();
	void callAfter();
	void callExit();

private:
	lua_State* L;
	lua_State* Lthread;
	wxString file;
	LuaState state;

	int refCallBefore;
	int refCallAfter;
	int refCallExit;

private:
	void CallbackError(wxString cat,lua_State *L);

	void unRegistry(int & refCall);
	void callLuaFunc(int & refCall);

};


#endif

