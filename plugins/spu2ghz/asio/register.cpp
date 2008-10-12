#include <windows.h>
#include <stdio.h>

typedef struct keylist
{
	HKEY		mainKey;
	HKEY		subKey;
	char		*keyname;
	struct keylist	*next;
} KEYLIST,	*LPKEYLIST;

#define CLSID_STRING_LEN				100
#define MAX_PATH_LEN					360

#define DEV_ERR_SELFREG					-100
#define ERRSREG_MODULE_NOT_FOUND		DEV_ERR_SELFREG-1
#define ERRSREG_MODPATH_NOT_FOUND		DEV_ERR_SELFREG-2
#define ERRSREG_STRING_FROM_CLSID		DEV_ERR_SELFREG-3
#define ERRSREG_CHAR_TO_MULTIBYTE		DEV_ERR_SELFREG-4

#define SZREGSTR_DESCRIPTION			"Description"
#define SZREGSTR_CLSID					"CLSID"
#define SZREGSTR_INPROCSERVER32			"InprocServer32"
#define SZREGSTR_THREADINGMODEL			"ThreadingModel"
#define SZREGSTR_SOFTWARE				"SOFTWARE"
#define SZREGSTR_ASIO					"ASIO"

LONG RegisterAsioDriver (CLSID,char *,char *,char *,char *);
LONG UnregisterAsioDriver (CLSID,char *,char *);

static LONG findRegPath (HKEY,char *);
static LONG createRegPath (HKEY,char *,char *);
static LONG createRegStringValue (HKEY,char *,char *,char *);
static LONG getRegString (HKEY,char *,char *,LPVOID,DWORD);
static LPKEYLIST findAllSubKeys (HKEY,HKEY,DWORD,char *,LPKEYLIST);
static LONG deleteRegPath (HKEY,char *,char *);

static char	subkeybuf[MAX_PATH_LEN];


// ------------------------------------------
// Global Functions 
// ------------------------------------------
LONG RegisterAsioDriver (CLSID clsid,char *szdllname,char *szregname,char *szasiodesc,char *szthreadmodel)
{
	HMODULE			hMod;
	char			szDLLPath[MAX_PATH_LEN];
	char			szModuleName[MAX_PATH_LEN];
	char			szregpath[MAX_PATH_LEN];
	char			szclsid[CLSID_STRING_LEN];
	LPOLESTR		wszCLSID = NULL;
	BOOL			newregentry = FALSE;
	LONG			rc;

	hMod = GetModuleHandle(szdllname);
	if (!hMod) return ERRSREG_MODULE_NOT_FOUND;
	szModuleName[0] = 0;
	GetModuleFileName(hMod,szModuleName,MAX_PATH_LEN);
	if (!szModuleName[0]) return ERRSREG_MODPATH_NOT_FOUND;
	CharLower((LPTSTR)szModuleName);

	rc = (LONG)StringFromCLSID(clsid,&wszCLSID);
	if (rc != S_OK) return ERRSREG_STRING_FROM_CLSID;
	rc = (LONG)WideCharToMultiByte(CP_ACP,0,(LPWSTR)wszCLSID,-1,szclsid,CLSID_STRING_LEN,0,0);
	if (!rc) return ERRSREG_CHAR_TO_MULTIBYTE;

	sprintf(szregpath,"%s\\%s",SZREGSTR_CLSID,szclsid);
	rc = findRegPath(HKEY_CLASSES_ROOT,szregpath);
	if (rc) {
		sprintf(szregpath,"%s\\%s\\%s",SZREGSTR_CLSID,szclsid,SZREGSTR_INPROCSERVER32);
		getRegString (HKEY_CLASSES_ROOT,szregpath,0,(LPVOID)szDLLPath,MAX_PATH_LEN);
		CharLower((LPTSTR)szDLLPath);
		rc = (LONG)strcmp(szDLLPath,szModuleName);
		if (rc) {
			// delete old regentry
			sprintf(szregpath,"%s",SZREGSTR_CLSID);
			deleteRegPath(HKEY_CLASSES_ROOT,szregpath,szclsid);
			newregentry = TRUE;
		}
	}
	else newregentry = TRUE;

	if (newregentry) {
		// HKEY_CLASSES_ROOT\CLSID\{...}
		sprintf(szregpath,"%s",SZREGSTR_CLSID);
		createRegPath (HKEY_CLASSES_ROOT,szregpath,szclsid);
		// HKEY_CLASSES_ROOT\CLSID\{...} -> Description
		sprintf(szregpath,"%s\\%s",SZREGSTR_CLSID,szclsid);
		createRegStringValue(HKEY_CLASSES_ROOT,szregpath,0,szasiodesc);
		// HKEY_CLASSES_ROOT\CLSID\InProcServer32
		sprintf(szregpath,"%s\\%s",SZREGSTR_CLSID,szclsid);
		createRegPath (HKEY_CLASSES_ROOT,szregpath,SZREGSTR_INPROCSERVER32);
		// HKEY_CLASSES_ROOT\CLSID\InProcServer32 -> DLL path
		sprintf(szregpath,"%s\\%s\\%s",SZREGSTR_CLSID,szclsid,SZREGSTR_INPROCSERVER32);
		createRegStringValue(HKEY_CLASSES_ROOT,szregpath,0,szModuleName);
		// HKEY_CLASSES_ROOT\CLSID\InProcServer32 -> ThreadingModel
		createRegStringValue(HKEY_CLASSES_ROOT,szregpath,SZREGSTR_THREADINGMODEL,szthreadmodel);
	}

	// HKEY_LOCAL_MACHINE\SOFTWARE\ASIO
	sprintf(szregpath,"%s\\%s",SZREGSTR_SOFTWARE,SZREGSTR_ASIO);
	rc = findRegPath(HKEY_LOCAL_MACHINE,szregpath);
	if (rc) {
		sprintf(szregpath,"%s\\%s\\%s",SZREGSTR_SOFTWARE,SZREGSTR_ASIO,szregname);
		rc = findRegPath(HKEY_LOCAL_MACHINE,szregpath);
		if (rc) {
			sprintf(szregpath,"%s\\%s",SZREGSTR_SOFTWARE,SZREGSTR_ASIO);
			deleteRegPath(HKEY_LOCAL_MACHINE,szregpath,szregname);
		}
	}
	else {
		// HKEY_LOCAL_MACHINE\SOFTWARE\ASIO
		sprintf(szregpath,"%s",SZREGSTR_SOFTWARE);
		createRegPath (HKEY_LOCAL_MACHINE,szregpath,SZREGSTR_ASIO);
	}

	// HKEY_LOCAL_MACHINE\SOFTWARE\ASIO\<szregname>
	sprintf(szregpath,"%s\\%s",SZREGSTR_SOFTWARE,SZREGSTR_ASIO);
	createRegPath (HKEY_LOCAL_MACHINE,szregpath,szregname);

	// HKEY_LOCAL_MACHINE\SOFTWARE\ASIO\<szregname> -> CLSID 
	// HKEY_LOCAL_MACHINE\SOFTWARE\ASIO\<szregname> -> Description
	sprintf(szregpath,"%s\\%s\\%s",SZREGSTR_SOFTWARE,SZREGSTR_ASIO,szregname);
	createRegStringValue(HKEY_LOCAL_MACHINE,szregpath,SZREGSTR_CLSID,szclsid);
	createRegStringValue(HKEY_LOCAL_MACHINE,szregpath,SZREGSTR_DESCRIPTION,szasiodesc);

	return 0;
}


LONG UnregisterAsioDriver (CLSID clsid,char *szdllname,char *szregname)
{
	LONG			rc;
	HMODULE			hMod;
	char			szregpath[MAX_PATH_LEN];
	char			szModuleName[MAX_PATH_LEN];
	char			szclsid[CLSID_STRING_LEN];
	LPOLESTR		wszCLSID = NULL;


	hMod = GetModuleHandle(szdllname);
	if (!hMod) return ERRSREG_MODULE_NOT_FOUND;
	szModuleName[0] = 0;
	GetModuleFileName(hMod,szModuleName,MAX_PATH_LEN);
	if (!szModuleName[0]) return ERRSREG_MODPATH_NOT_FOUND;
	CharLower((LPTSTR)szModuleName);

	rc = (LONG)StringFromCLSID(clsid,&wszCLSID) ;
	if (rc != S_OK) return ERRSREG_STRING_FROM_CLSID;
	rc = (LONG)WideCharToMultiByte(CP_ACP,0,(LPWSTR)wszCLSID,-1,szclsid,CLSID_STRING_LEN,0,0);
	if (!rc) return ERRSREG_CHAR_TO_MULTIBYTE;


	sprintf(szregpath,"%s\\%s",SZREGSTR_CLSID,szclsid);
	rc = findRegPath(HKEY_CLASSES_ROOT,szregpath);
	if (rc) {
		// delete old regentry
		sprintf(szregpath,"%s",SZREGSTR_CLSID);
		deleteRegPath(HKEY_CLASSES_ROOT,szregpath,szclsid);
	}


	// HKEY_LOCAL_MACHINE\SOFTWARE\ASIO
	sprintf(szregpath,"%s\\%s",SZREGSTR_SOFTWARE,SZREGSTR_ASIO);
	rc = findRegPath(HKEY_LOCAL_MACHINE,szregpath);
	if (rc) {
		sprintf(szregpath,"%s\\%s\\%s",SZREGSTR_SOFTWARE,SZREGSTR_ASIO,szregname);
		rc = findRegPath(HKEY_LOCAL_MACHINE,szregpath);
		if (rc) {
			sprintf(szregpath,"%s\\%s",SZREGSTR_SOFTWARE,SZREGSTR_ASIO);
			deleteRegPath(HKEY_LOCAL_MACHINE,szregpath,szregname);
		}
	}

	return 0;
}


// ------------------------------------------
// Local Functions 
// ------------------------------------------
static LONG findRegPath (HKEY mainkey,char *szregpath)
{
	HKEY	hkey;
	LONG 	cr,rc = -1;
	
	if (szregpath) {
		if ((cr = RegOpenKey(mainkey,szregpath,&hkey)) == ERROR_SUCCESS) {
			RegCloseKey(hkey);
			rc = 1;
		}
		else rc = 0;
	}

	return rc;
}

static LONG createRegPath (HKEY mainkey,char *szregpath,char *sznewpath)
{
	HKEY	hkey,hksub;
	LONG 	cr,rc = -1;
	char	newregpath[MAX_PATH_LEN];
	
	sprintf(newregpath,"%s\\%s",szregpath,sznewpath);
	if (!(cr = findRegPath(mainkey,newregpath))) {
		if ((cr = RegOpenKey(mainkey,szregpath,&hkey)) == ERROR_SUCCESS) {
			if ((cr = RegCreateKey(hkey,sznewpath,&hksub)) == ERROR_SUCCESS) {
				RegCloseKey(hksub);
				rc = 0;
			}
			RegCloseKey(hkey);
		}
	}
	else if (cr > 0) rc = 0;

	return rc;
}

static LONG createRegStringValue (HKEY mainkey,char *szregpath,char *valname,char *szvalstr)
{
	LONG	cr,rc = -1;
	HKEY	hkey;

	if (szregpath) {
		if ((cr = RegOpenKey(mainkey,szregpath,&hkey)) == ERROR_SUCCESS) {
			cr = RegSetValueEx(hkey,(LPCTSTR)valname,0,REG_SZ,(const BYTE *)szvalstr,(DWORD)strlen(szvalstr));
			RegCloseKey(hkey);
			if (cr == ERROR_SUCCESS) rc = 0;
		}
	} 

	return rc;
}


static LONG getRegString (HKEY mainkey,char *szregpath,char *valname,LPVOID pval,DWORD vsize)
{
	HKEY	hkey;
	LONG 	cr,rc = -1;
	DWORD	hsize,htype;

	if (szregpath) {
		if ((cr = RegOpenKey(mainkey,szregpath,&hkey)) == ERROR_SUCCESS) {
			cr = RegQueryValueEx(hkey,valname,0,&htype,0,&hsize);
			if (cr == ERROR_SUCCESS) {
				if (hsize <= vsize) {
					cr = RegQueryValueEx(hkey,valname,0,&htype,(LPBYTE)pval,&hsize);
					rc = 0;
				}
			}
			RegCloseKey(hkey);
		}
	}
	return rc;
}

static LPKEYLIST findAllSubKeys (HKEY hkey,HKEY hksub,DWORD index,char *keyname,LPKEYLIST kl)
{
	HKEY	hknew = 0;
	char	*newkey;
	LONG 	cr;

	if (!hksub) {
		cr = RegOpenKeyEx(hkey,(LPCTSTR)keyname,0,KEY_ALL_ACCESS,&hknew);
		if (cr != ERROR_SUCCESS) return kl;
	}
	else hknew = hksub;
		
	cr = RegEnumKey(hknew,index,(LPTSTR)subkeybuf,MAX_PATH_LEN);
	if (cr == ERROR_SUCCESS) {
		newkey = new char[strlen(subkeybuf)+1];
		strcpy(newkey,subkeybuf);

		kl = findAllSubKeys(hknew,0,0,newkey,kl);
		kl = findAllSubKeys(hkey,hknew,index+1,keyname,kl);
		
		return kl;

	}	
	
	if (!kl->next) {
		kl->next = new KEYLIST[1];
		kl = kl->next;
		kl->mainKey = hkey;
		kl->subKey = hknew;
		kl->keyname = keyname;
		kl->next = 0;
	}

	return kl;
}

static LONG deleteRegPath (HKEY mainkey,char *szregpath,char *szdelpath)
{
	HKEY		hkey;
	LONG 		cr,rc = -1;
	KEYLIST		klist;
	LPKEYLIST	pkl,k;
	char		*keyname = 0;
	
	if ((cr = RegOpenKey(mainkey,szregpath,&hkey)) == ERROR_SUCCESS) {

		keyname = new char[strlen(szdelpath)+1];
		if (!keyname) {
			RegCloseKey(hkey);
			return rc;
		}
		strcpy(keyname,szdelpath);
		klist.next = 0;

		findAllSubKeys(hkey,0,0,keyname,&klist);
	
		if (klist.next) {
			pkl = klist.next;
	
			while (pkl) {
				RegCloseKey(pkl->subKey);
				cr = RegDeleteKey(pkl->mainKey,pkl->keyname);
				delete pkl->keyname;
				k = pkl;
				pkl = pkl->next;
				delete k;
			}
			rc = 0;
		}

		RegCloseKey(hkey);
	}

	return rc;
}

