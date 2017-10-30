#pragma once
#ifndef __LUA_MANAGER_H__
#define __LUA_MANAGER_H__

#include "LuaEngine.h"

#include "TAS/PadData.h"


class LuaManager {
public:
	LuaManager();
	~LuaManager() {}
public:

	void FrameBoundary();

	void SetCanModifyController(bool can);
	void ControllerInterrupt(u8 &data, u8 &port, u16 & BufCount, u8 buf[]);
	PadData & getNowFramePadData();
	void setNowFramePadData(const PadData & pad);

	void setFileName(wxString filename);
	void Stop();
	void Run();
	void Restart();

	LuaEngine * getLuaEnginPtr(lua_State* l);

private:
	LuaEngine lua;
	bool fSetFrameKey;


};
extern LuaManager g_Lua;


#endif