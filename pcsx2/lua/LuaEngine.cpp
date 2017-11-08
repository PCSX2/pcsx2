#include "PrecompiledHeader.h"

#include "LuaEngine.h"
#include "LuaFunctions.h"
#include "LuaManager.h"

#include "LuaFrame.h"	// use "LuaFrame"
#include "App.h"	// use "LuaFrame"


LuaEngine:: LuaEngine():
	L(NULL),
	Lthread(NULL),
	state(NONE),
	refCallBefore(LUA_NOREF),
	refCallAfter(LUA_NOREF),
	refCallExit(LUA_NOREF)
{}

// --------------------------------------------------------------------------------------
//  LoadLuaCode
// --------------------------------------------------------------------------------------
void LuaEngine::setFileName(wxString filename)
{
	file = filename;
	setState(NOT_OPEN);
}
bool LuaEngine::Load()
{
	Close();
	if (state != NOT_OPEN)return false;

	L = luaL_newstate();
	Lthread = lua_newthread(L);
	luaL_openlibs(Lthread);	// add lua library

	// luaL_register
#define LUA_REGISTER( funcs , name ) {luaL_newlib(Lthread, funcs); lua_setglobal(Lthread, name);}
	LUA_REGISTER(lua_function_emulib, "emu");
	LUA_REGISTER(lua_function_memorylib, "memory");
	LUA_REGISTER(lua_function_joypadlib, "joypad");
	LUA_REGISTER(lua_function_savestatelib, "savestate");
	LUA_REGISTER(lua_function_movielib, "movie");
	LUA_REGISTER(lua_function_guilib, "gui");
	LUA_REGISTER(lua_function_lualib, "lua");
#undef LUA_REGISTER

	// single refister
	lua_register(Lthread, "print", lua_function_print);


	// load
	if (luaL_loadfile(Lthread, file.c_str() ) != LUA_OK) {
		CallbackError(L"load", Lthread);
		Close();
		return false;
	}
	
	setState(OPEN);
	return true;
}

//--------------------------------------------------
// frameadvance controle
//--------------------------------------------------
void LuaEngine::Resume(void)
{
	
	if (Lthread == NULL)return;
	if ( state != RESUME )return;
	
	g_Lua.SetCanModifyController(true);
	setState(RUNNING);
	int result = lua_resume(Lthread, NULL, 0);
	if (result == LUA_OK) {
		// lua script end
		// But we don't stop the script
		g_Lua.SetCanModifyController(false);
		return;
	}
	else if (result == LUA_YIELD) {
		// not problem
	}
	else {
		// error
		CallbackError("", Lthread);
		Close();
		return;
	}

}

//--------------------------------------------------
// Close
//--------------------------------------------------
void LuaEngine::Close()
{
	if (L == NULL)return;

	callExit();
	
	unRegistryBefore();
	unRegistryAfter();
	unRegistryExit();

	lua_close(L);
	L = NULL;
	Lthread = NULL;
	setState(NOT_OPEN);

}
void LuaEngine::unRegistry(int & refCall)
{
	if (refCall != LUA_NOREF) {
		luaL_unref(Lthread, LUA_REGISTRYINDEX, refCall);
		refCall = LUA_NOREF;
	}
}
void LuaEngine::unRegistryBefore()
{
	unRegistry(refCallBefore);
}
void LuaEngine::unRegistryAfter()
{
	unRegistry(refCallAfter);
}
void LuaEngine::unRegistryExit()
{
	unRegistry(refCallExit);
}

//--------------------------------------------------
// CallbackError
//--------------------------------------------------
void LuaEngine::CallbackError(wxString cat,lua_State *L)
{
	wxString error = lua_tostring(L, -1);
	// TODO - add a LUA console filter!
	Console.WriteLn(Color_StrongBlue, L"[lua]Lua Error(%s):%s", WX_STR(cat), WX_STR(error));
}

//--------------------------------------------------
// change state -> LuaFrame
// LuaFramへのアクセスはすべてここ
//--------------------------------------------------
void LuaEngine::setState(LuaEngine::LuaState _state)
{
	state = _state;
	LuaFrame * frame = wxGetApp().GetLuaFramePtr();
	wxString msg = "";
	
	if (_state == NOT_OPEN) {
		msg = "lua not open.";
		frame->pushStopState();
	}
	else if (_state == OPEN) {
	}
	else if (_state == RUNNING) {
		msg = "lua running.";
	}
	else if (_state == RESUME) {
		msg = "lua resume.";
		frame->pushRunState();
	}
	frame->drawState(wxString::Format(L"%s(%s)", msg, file));
}


//--------------------------------------------------
// call
//--------------------------------------------------
void LuaEngine::callLuaFunc(int & refCall)
{
	if (Lthread == NULL)return;
	if (refCall == LUA_NOREF)return;

	lua_rawgeti(Lthread, LUA_REGISTRYINDEX, refCall);
	if (lua_pcall(Lthread, 0, 0, 0) != LUA_OK)
	{
		// error
		CallbackError(L"regester", Lthread);
	}
}
void LuaEngine::callBefore()
{
	callLuaFunc(refCallBefore);
}
void LuaEngine::callAfter()
{
	callLuaFunc(refCallAfter);
}
void LuaEngine::callExit()
{
	callLuaFunc(refCallExit);
}

