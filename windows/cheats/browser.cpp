#include <stdio.h>
#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>

#include <vector>
#include <string>

using namespace std;

#include "../cheatscpp.h"

#include "PS2Etypes.h"

extern "C" {
#include "windows/resource.h"
#include "PS2Edefs.h"
#include "Memory.h"

#include "cheats.h"
#include "../../patch.h"
}

HWND hWndBrowser;

HTREEITEM AddTreeItem(HWND treeview, HTREEITEM parent, const char *name, LPARAM lp)
{
	TVINSERTSTRUCT node={
		parent,
		0,
		{
			TVIF_TEXT,
			NULL,
			TVIS_EXPANDED,
			TVIS_EXPANDED,
			const_cast<LPSTR>(name),
			0,
			0,
			0,
			0,
			lp,
			1
		}
	};

	return TreeView_InsertItem(treeview,&node);
}

HTREEITEM AddGroups(HWND treeview, HTREEITEM parent, int parentIndex)
{
	HTREEITEM p=NULL;
	for(unsigned int i=0;i<groups.size();i++)
	{
		if(groups[i].parentIndex==parentIndex)
		{
			p=AddTreeItem(treeview,parent,groups[i].title.c_str(),i);
			if(parent==0)
			{
				TreeView_SetItemState(treeview,p,TVIS_EXPANDED|TVIS_BOLD,0x00F0);
			}
			AddGroups(treeview,p,i);
			for(unsigned int j=0;j<patches.size();j++)
			{
				if(patches[j].group==i)
				{
					if(patches[j].title.length()>0)
					{
						AddTreeItem(treeview,p,patches[j].title.c_str(),0x01000000|j);
					}
				}
			}
		}
	}
	return p;
}

BOOL CALLBACK BrowserProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	int wmId,wmEvent;

	switch(uMsg)
	{

		case WM_PAINT:
			return FALSE;
		case WM_INITDIALOG:
			hWndBrowser=hWnd;
			{
				//Add groups to the Treeview
				HTREEITEM root=AddGroups(GetDlgItem(hWnd,IDC_GROUPS),0, -1);
				TreeView_SetItemState(GetDlgItem(hWnd,IDC_GROUPS),root,TVIS_EXPANDED|TVIS_EXPANDEDONCE|TVIS_EXPANDPARTIAL|TVIS_BOLD,0x00F0);
			}
			break;

		case WM_COMMAND:
			wmId    = LOWORD(wParam); 
			wmEvent = HIWORD(wParam); 
			// Parse the menu selections:
			switch (wmId)
			{
				case IDCANCEL:
					EndDialog(hWnd,1);
					break;

				case IDOK:
					EndDialog(hWnd,1);
					break;

				default:
					return FALSE;
			}
			break;
		default:
			return FALSE;
	}
	return TRUE;
}

void ShowCheats(HINSTANCE hInstance, HWND hParent)
{
	INT_PTR ret=DialogBoxParam(hInstance,MAKEINTRESOURCE(IDD_CHEATS),hParent,(DLGPROC)BrowserProc,1);
}
