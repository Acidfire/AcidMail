#if !defined(MAIN_H)
#define MAIN_H

#include "acidmail.h"

Acidmail *acidmail;
HWND hwndParent;
HWND hwndMain;
HINSTANCE hwndInstance;

// Bang command callback functions
void BangCheckMail(HWND hwndOwner, const char *args);
void BangClearNew(HWND hwndOwner, const char *args);
void BangSetPass(HWND hwndOwner, const char *args);

const int msgs[] = {
    LM_GETREVID,
	LM_REFRESH,
    0};

extern "C"
{
  __declspec( dllexport ) int initModuleEx(HWND hParent, HINSTANCE hInstance, const char *path);
  __declspec( dllexport ) void quitModule(HINSTANCE hInstance);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL __stdcall _DllMainCRTStartup(HINSTANCE hInst, DWORD fdwReason, LPVOID lpvRes);

// Global function in mailbox.cpp
//BOOL CALLBACK GetPassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM /* lParam */);

#endif // MAIN_H