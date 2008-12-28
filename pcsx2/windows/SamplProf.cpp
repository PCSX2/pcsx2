
#include "PrecompiledHeader.h"

#ifndef _DEBUG

#include "SamplProf.h"
#include <map>
#include <algorithm>

using namespace std;

DWORD GetModuleFromPtr(IN void* ptr,OUT LPSTR lpFilename,IN DWORD nSize)
{
	MEMORY_BASIC_INFORMATION mbi;
	VirtualQuery(ptr,&mbi,sizeof(mbi));
	return GetModuleFileName((HMODULE)mbi.AllocationBase,lpFilename,nSize);
}
struct Module
{
	uptr base;
	uptr end;
	uptr len;
	string name;
	u32 ticks;

	Module(const char* name,void* ptr)
	{
		if (name!=0)
			this->name=name;
		FromAddress(ptr,name==0);
		ticks=0;
	}
	Module(const char* name,void* b,u32 s)
	{
		this->name=name;
		FromValues(b,s);
		ticks=0;
	}
	bool operator<(const Module &other) const
	{
		return ticks>other.ticks;
	}
	string ToString(u32 total_ticks)
	{
		return name + ": " + to_string(ticks*100/(float)total_ticks) + " ";
	}
	bool Inside(uptr val) { return val>=base && val<=end; }
	void FromAddress(void* ptr,bool getname)
	{
		char filename[512];
		char filename2[512];
		static void* ptr_old=0;
		if (ptr_old==ptr)
			return;
		ptr_old=ptr;

		MEMORY_BASIC_INFORMATION mbi;
		VirtualQuery(ptr,&mbi,sizeof(mbi));
		base=(u32)mbi.AllocationBase;
		GetModuleFileName((HMODULE)mbi.AllocationBase,filename,512);
		len=(u8*)mbi.BaseAddress-(u8*)mbi.AllocationBase+mbi.RegionSize;
		
		if (getname)
		{
			name=filename;
			size_t last=name.find_last_of('\\');
			last=last==name.npos?0:last+1;
			name=name.substr(last);
		}

		for(;;)
		{
			VirtualQuery(((u8*)base)+len,&mbi,sizeof(mbi));
			if (!(mbi.Type&MEM_IMAGE))
				break;
			
			if (!GetModuleFileName((HMODULE)mbi.AllocationBase,filename2,512))
				break;

			if (strcmp(filename,filename2)!=0)
				break;
			len+=mbi.RegionSize;
		}
		

		end=base+len-1;
	}
	void FromValues(void* b,u32 s)
	{
		base= (uptr)b;
		len=s;
		end=base+len-1;
	}
};

typedef map<string,Module> MapType;

static vector<Module> ProfModules;
static MapType ProfUnknownHash;

static HANDLE hEmuThread;
static HANDLE hProfThread;

static CRITICAL_SECTION ProfModulesLock;

static volatile bool ProfRunning=false;

void ProfilerRegisterSource(const char* Name,void* buff,u32 sz)
{
	if( ProfRunning )
		EnterCriticalSection( &ProfModulesLock );
	Module tmp(Name,buff,sz);
	ProfModules.push_back(tmp);
	if( ProfRunning )
		LeaveCriticalSection( &ProfModulesLock );
}

void ProfilerRegisterSource(const char* Name,void* function)
{
	if( ProfRunning )
		EnterCriticalSection( &ProfModulesLock );
	Module tmp(Name,function);
	ProfModules.push_back(tmp);
	if( ProfRunning )
		LeaveCriticalSection( &ProfModulesLock );
}

int __stdcall ProfilerThread(void* nada)
{
	ProfUnknownHash.clear();
	u32 tick_count=0;

	while(ProfRunning)
	{
		Sleep(6);

		if (tick_count>400)
		{
			string rv="|";
			u32 subtotal=0;
			for (size_t i=0;i<ProfModules.size();i++)
			{
				rv+=ProfModules[i].ToString(tick_count);
				subtotal+=ProfModules[i].ticks;
				ProfModules[i].ticks=0;
			}

			rv+=" Total " + to_string(subtotal*100/(float)tick_count) + "\n|";
			vector<MapType::mapped_type> lst;
			for (MapType::iterator i=ProfUnknownHash.begin();i!=ProfUnknownHash.end();i++)
			{
				lst.push_back(i->second);
			}

			sort(lst.begin(),lst.end());
			for (size_t i=0;i<lst.size();i++)
			{
				rv+=lst[i].ToString(tick_count);
			}

			SysPrintf("+Sampling Profiler Results-\n%s\n+>\n",rv.c_str());

			tick_count=0;

			ProfUnknownHash.clear();
		}

		tick_count++;

		CONTEXT ctx;
		ctx.ContextFlags= CONTEXT_FULL;
		GetThreadContext(hEmuThread,&ctx);

		EnterCriticalSection( &ProfModulesLock );
		size_t i;
		for(i=0;i<ProfModules.size();i++)
			if (ProfModules[i].Inside(ctx.Eip)) break;

		if( i < ProfModules.size() )
		{
			ProfModules[i].ticks++;
			LeaveCriticalSection( &ProfModulesLock );
			continue;
		}
		LeaveCriticalSection( &ProfModulesLock );

		char modulename[512];

		DWORD sz=GetModuleFromPtr((void*)ctx.Eip,modulename,512);
		string modulenam;
		if (sz==0)
			modulenam="[Unknown]";
		else
			modulenam=modulename;

		map<string,Module>::iterator iter=ProfUnknownHash.find(modulenam);
		if (iter!=ProfUnknownHash.end())
		{
			iter->second.ticks++;
			continue;
		}

		Module tmp(sz==0?modulenam.c_str():0,(void*)ctx.Eip);
		tmp.ticks++;

		ProfUnknownHash.insert(MapType::value_type(modulenam, tmp));
	}

	return -1;
}

void ProfilerInit()
{
	if (ProfRunning)
		return;

	//Console::Write( "Profiler Thread Initializing..." );
	ProfRunning=true;
	DuplicateHandle(GetCurrentProcess(), 
		GetCurrentThread(), 
		GetCurrentProcess(),
		&(HANDLE)hEmuThread, 
		0,
		FALSE,
		DUPLICATE_SAME_ACCESS);

	InitializeCriticalSection( &ProfModulesLock );

	hProfThread=CreateThread(0,0,(LPTHREAD_START_ROUTINE)ProfilerThread,0,0,0);
	SetThreadPriority(hProfThread,THREAD_PRIORITY_HIGHEST);
	//Console::WriteLn( " Done!" );
}

void ProfilerTerm()
{
	//Console::Write( "Profiler Terminating..." );
	if (!ProfRunning)
		return;
	ProfRunning=false;
	ResumeThread(hProfThread);
	WaitForSingleObject(hProfThread,INFINITE);
	CloseHandle(hProfThread);
	CloseHandle(hEmuThread);
	DeleteCriticalSection( &ProfModulesLock );
	//Console::WriteLn( " Done!" );
}

void ProfilerSetEnabled(bool Enabled)
{
	if (!ProfRunning)
		return;

	if (Enabled)
		ResumeThread(hProfThread);
	else
		SuspendThread(hProfThread);
}

#endif
