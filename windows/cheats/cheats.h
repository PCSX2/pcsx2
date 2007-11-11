#ifndef CHEATS_H_INCLUDED
#define CHEATS_H_INCLUDED

#ifndef __cplusplus
typedef enum ebool
{
	false,
	true
} bool;
#endif

extern HINSTANCE pInstance;
extern bool FirstShow;

void AddCheat(HINSTANCE hInstance, HWND hParent);
void ShowFinder(HINSTANCE hInstance, HWND hParent);
void ShowCheats(HINSTANCE hInstance, HWND hParent);

#endif//CHEATS_H_INCLUDED
