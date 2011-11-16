#include "..\litestep248\lsapi\lsapi.h"
#include <windows.h>
#include "main.h"
#include "version.h"

// Litestep calls this function after it loads the module. This is where the
// module should register bang commands and create windows. If initialization
// succeeds, return zero. If an error occurs, return a nonzero value.
int initModuleEx(HWND hParent, HINSTANCE hInstance, const char *path)
{   
	// Ignore attempts to load the module more than once
	if (acidmail)
	{
		return 1;
	}
	
	hwndParent = hParent;
	hwndInstance = hInstance;
	
	// Create window for message handling
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;		// the window procedure
	wc.hInstance = hInstance;		// hInstance of DLL
	wc.lpszClassName = szAppName;	// the window class name

	if (!RegisterClass(&wc)) 
	{
		MessageBox(hwndParent, "Error: Could not register window class",
            szAppName, MB_OK | MB_ICONERROR);
		return 2;
	}

	hwndMain = CreateWindowEx(WS_EX_TOOLWINDOW, szAppName, NULL, WS_POPUP, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);

    if (!hwndMain)
    {
        MessageBox(hwndParent, "Error: Could create main window",
            szAppName, MB_OK | MB_ICONERROR);
		UnregisterClass(szAppName, hInstance);
        return 3;
    }
	
	// Initiate winsock
	WSADATA wsa;
		if (WSAStartup(MAKEWORD(2,2),&wsa))
		{
			MessageBox(hwndParent, "Could not initialize winsock",
				szAppName, MB_OK | MB_ICONERROR);
			return 4;
		}

	// Register messages to Litestep
	SendMessage(hwndParent, LM_REGISTERMESSAGE, (WPARAM)hwndMain, (LPARAM)msgs);

	// Register bang commands
	AddBangCommand("!AcidMailCheckMail", BangCheckMail);
    AddBangCommand("!AcidMailClearNew", BangClearNew);
	AddBangCommand("!AcidMailSetPass", BangSetPass);

	// Create main class instance
	acidmail = new Acidmail();

	return 0;
}

// Litestep calls this function before it unloads the module. This function
// should unregister bang commands and destroy windows.
void quitModule(HINSTANCE hInstance)
{   
	//Unregister messages to Litestep
	SendMessage(hwndParent, LM_UNREGISTERMESSAGE, (WPARAM)hwndMain, (LPARAM)msgs);

	// Destroy message window
	DestroyWindow(hwndMain);
	UnregisterClass(szAppName, hInstance);

	// Unregister bang commands
	RemoveBangCommand("!AcidMailCheckMail");
    RemoveBangCommand("!AcidMailClearNew");

	// Destroy main class instance
	delete acidmail;
	acidmail = NULL;
	
	// Deactivate winsock
	WSACleanup();
}
// Bang Commands
void BangCheckMail(HWND /*hwndOwner*/, const char * /*args*/)
{
	acidmail->CheckMail();
}

void BangClearNew(HWND /*hwndOwner*/, const char * /*args*/)
{
    acidmail->ClearMail();
}

void BangSetPass(HWND /*hwndOwner*/, const char * args)
{
	char server[128], user[64], pass[64];
	char *buffers[] = {server, user, pass};
	LCTokenize(args, buffers, 3, NULL);
	acidmail->SetPass(server, user, pass);
}

// Message processor
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	LPSTR buf = (LPSTR)lParam;
	switch (msg)
	{
		case LM_GETREVID:	
			(wParam == 0 || wParam == 1) ? strcpy(buf, szVersion) : strcpy(buf, "");
            return strlen(buf);
			break;
		
		case LM_REFRESH:
			delete acidmail;
			acidmail = new Acidmail();
			break;
		
		case WM_TIMER:
            acidmail->CheckMail();
			break;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

BOOL __stdcall _DllMainCRTStartup(HINSTANCE hInst, DWORD fdwReason, LPVOID lpvRes)
{
	// We don't need thread notifications for what we're doing.  Thus, get
	// rid of them, thereby eliminating some of the overhead of this DLL
	DisableThreadLibraryCalls(hInst);

	return TRUE;
}
