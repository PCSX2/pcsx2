#include <windows.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
using namespace std;

#include "tinyxml.h"

extern "C" {
#	include "../ps2etypes.h"
#	include "../patch.h"

#	ifdef __WIN32__
	struct AppData {
		HWND hWnd;           // Main window handle
		HINSTANCE hInstance; // Application instance
		HMENU hMenu;         // Main window menu
		HANDLE hConsole;
	} extern gApp;
#	endif

	void SysPrintf(char *fmt, ...);
	int LoadPatch(char *patchfile);
}

#include "../cheatscpp.h"

int LoadGroup(TiXmlNode *group, int parent);

Group::Group(int nParent,bool nEnabled, string &nTitle):
		parentIndex(nParent),enabled(nEnabled),title(nTitle)
{
}

Patch::Patch(int patch, int grp, bool en, string &ttl):
	title(ttl),
	group(grp),
	enabled(en),
	patchIndex(patch)
{
}

Patch Patch::operator =(const Patch&p)
{
	title.assign(p.title);
	group=p.group;
	enabled=p.enabled;
	patchIndex=p.patchIndex;
	return *this;
}


vector<Group> groups;
vector<Patch> patches;

int LoadPatch(char *crc)
{
	char pfile[256];
	sprintf(pfile,"patches\\%s.xml",crc);

	patchnumber=0;

	TiXmlDocument doc( pfile );
	bool loadOkay = doc.LoadFile();
	if ( !loadOkay )
	{
		SysPrintf("XML Patch Loader: Could not load file '%s'. Error='%s'.\n", pfile, doc.ErrorDesc() );
		return -1;
	}

	TiXmlNode *root = doc.FirstChild("GAME");
	if(!root)
	{
		SysPrintf("XML Patch Loader: Root node is not GAME, invalid patch file.\n");
		return -1;
	}

	TiXmlElement *rootelement = root->ToElement();

	const char *title=rootelement->Attribute("title");
	if(title)
		SysPrintf("XML Patch Loader: Game Title: %s\n",title);

	int result=LoadGroup(root,-1);
	if(result) {
		patchnumber=0;
		return result;
	}

#ifdef __WIN32__
	if (gApp.hConsole) 
	{
		if(title)
			SetConsoleTitle(title);
		else
			SetConsoleTitle("<No Title>");
	}
		
#endif

	return 0;
}


int LoadGroup(TiXmlNode *group,int gParent)
{

	TiXmlElement *groupelement = group->ToElement();

	const char *gtitle=groupelement->Attribute("title");
	if(gtitle)
		SysPrintf("XML Patch Loader: Group Title: %s\n",gtitle);

	const char *enable=groupelement->Attribute("enabled");
	bool gEnabled=true;
	if(enable)
	{
		if(strcmp(enable,"false")==0)
		{
			SysPrintf("XML Patch Loader: Group is disabled.\n");
			gEnabled=false;
		}
	}

	TiXmlNode *comment = group->FirstChild("COMMENT");
	if(comment)
	{
		TiXmlElement *cmelement = comment->ToElement();
		const char *comment = cmelement->GetText();
		if(comment)
			SysPrintf("XML Patch Loader: Group Comment:\n%s\n---\n",comment);
	}

	string t;
	
	if(gtitle)
		t.assign(gtitle);
	else
		t.clear();
	
	Group gp=Group(gParent,gEnabled,t);
	groups.push_back(gp);

	int gIndex=groups.size()-1;


	TiXmlNode *fastmemory=group->FirstChild("FASTMEMORY");
	if(fastmemory!=NULL)
		SetFastMemory(1);

	TiXmlNode *roundmode=group->FirstChild("ROUNDMODE");
	if(roundmode!=NULL)
	{
		int eetype=0x0000;
		int vutype=0x6000;

		TiXmlElement *rm=roundmode->ToElement();
		if(rm!=NULL)
		{
			const char *eetext=rm->Attribute("ee");
			const char *vutext=rm->Attribute("vu");

			if(eetext != NULL) {
				eetype = 0xffff;
				if( stricmp(eetext, "near") == 0 ) {
					eetype = 0x0000;
				}
				else if( stricmp(eetext, "down") == 0 ) {
					eetype = 0x2000;
				}
				else if( stricmp(eetext, "up") == 0 ) {
					eetype = 0x4000;
				}
				else if( stricmp(eetext, "chop") == 0 ) {
					eetype = 0x6000;
				}
			}

			if(vutext != NULL) {
				vutype = 0xffff;
				if( stricmp(vutext, "near") == 0 ) {
					vutype = 0x0000;
				}
				else if( stricmp(vutext, "down") == 0 ) {
					vutype = 0x2000;
				}
				else if( stricmp(vutext, "up") == 0 ) {
					vutype = 0x4000;
				}
				else if( stricmp(vutext, "chop") == 0 ) {
					vutype = 0x6000;
				}
			}
		}
		if(( eetype == 0xffff )||( vutype == 0xffff )) {
			printf("XML Patch Loader: WARNING: Invalid value in ROUNDMODE.\n");
		}
		else {
			SetRoundMode(eetype,vutype);
		}
	}

	TiXmlNode *cpatch = group->FirstChild("PATCH");
	while(cpatch)
	{
		TiXmlElement *celement = cpatch->ToElement();
		if(!celement)
		{
			SysPrintf("XML Patch Loader: ERROR: Couldn't convert node to element.\n" );
			return -1;
		}


		const char *ptitle=celement->Attribute("title");
		const char *penable=celement->Attribute("enabled");
		const char *applymode=celement->Attribute("applymode");
		const char *place=celement->Attribute("place");
		const char *address=celement->Attribute("address");
		const char *size=celement->Attribute("size");
		const char *value=celement->Attribute("value");

		if(ptitle) {
			SysPrintf("XML Patch Loader: Patch title: %s\n", ptitle);
		}

		bool penabled=gEnabled;
		if(penable)
		{
			if(strcmp(penable,"false")==0)
			{
				SysPrintf("XML Patch Loader: Patch is disabled.\n");
				penabled=false;
			}
		}

		if(!applymode) applymode="frame";
		if(!place) place="EE";
		if(!address) {
			SysPrintf("XML Patch Loader: ERROR: Patch doesn't contain an address.\n");
			return -1;
		}
		if(!value) {
			SysPrintf("XML Patch Loader: ERROR: Patch doesn't contain a value.\n");
			return -1;
		}
		if(!size) {
			SysPrintf("XML Patch Loader: WARNING: Patch doesn't contain the size. Trying to deduce from the value size.\n");
			switch(strlen(value))
			{
				case 8:
				case 7:
				case 6:
				case 5:
					size="32";
					break;
				case 4:
				case 3:
					size="16";
					break;
				case 2:
				case 1:
					size="8";
					break;
				case 0:
					size="0";
					break;
				default:
					size="64";
					break;
			}
		}

		if(strcmp(applymode,"startup")==0)
		{
			patch[patchnumber].placetopatch=0;
		} else
		if(strcmp(applymode,"vsync")==0)
		{
			patch[patchnumber].placetopatch=1;
		} else
		{
			SysPrintf("XML Patch Loader: ERROR: Invalid applymode attribute.\n");
			patchnumber=0;
			return -1;
		}
		
		if(strcmp(place,"EE")==0)
		{
			patch[patchnumber].cpu=1;
		} else
		if(strcmp(place,"IOP")==0)
		{
			patch[patchnumber].cpu=2;
		} else
		{
			SysPrintf("XML Patch Loader: ERROR: Invalid place attribute.\n");
			patchnumber=0;
			return -1;
		}

		if(strcmp(size,"64")==0)
		{
			patch[patchnumber].type=4;
		} else
		if(strcmp(size,"32")==0)
		{
			patch[patchnumber].type=3;
		} else
		if(strcmp(size,"16")==0)
		{
			patch[patchnumber].type=2;
		} else
		if(strcmp(size,"8")==0)
		{
			patch[patchnumber].type=1;
		} else
		{
			SysPrintf("XML Patch Loader: ERROR: Invalid size attribute.\n");
			patchnumber=0;
			return -1;
		}

		sscanf( address, "%X", &patch[ patchnumber ].addr );
		sscanf( value, "%I64X", &patch[ patchnumber ].data );

		patch[patchnumber].enabled=penabled?1:0;

		string pt;

		if(ptitle)
			pt.assign(ptitle);
		else
			pt.clear();
		
		Patch p=Patch(patchnumber,gIndex,penabled,pt);
		patches.push_back(p);

		patchnumber++;

		cpatch = cpatch->NextSibling("PATCH");
	}

	cpatch = group->FirstChild("GROUP");
	while(cpatch) {
		int result=LoadGroup(cpatch,gIndex);
		if(result) return result;
		cpatch = cpatch->NextSibling("GROUP");
	}

	return 0;
}
