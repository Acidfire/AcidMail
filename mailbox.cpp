#include "..\litestep248\lsapi\lsapi.h"
#include <string.h>
#include <winerror.h>
#include <windows.h>
#include "mailbox.h"
#include "acidmail.h"
#include "version.h"
#include "resource.h"

#define BUFSIZE 2048

// Globals
BOOL CALLBACK GetPassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM /* lParam */);
extern HINSTANCE hwndInstance;
char temp_name[64] = {0}, temp_pass[64] = {0};

Mailbox::Mailbox(char* name, unsigned numSubjects):
	mail()
{
	// Copy args
	strcpy(sName, name);
	iSubjects = numSubjects;
	
	// Copy the rest of the args derived from the mailbox name
	strcpy(sSetting, name);
	strcat(sSetting, "server");
	if(GetRCString(sSetting, sServer, NULL, MAX_LINE_LENGTH))
	{
		strcpy(sSetting, name);
		strcat(sSetting, "type");
		GetRCString(sSetting, sType, "pop3", MAX_LINE_LENGTH);
		strcpy(sSetting, name);
		strcat(sSetting, "user");
		GetRCString(sSetting, sUser, "anonymous", MAX_LINE_LENGTH);
		strcpy(sSetting, name);
		strcat(sSetting, "port");
		if (!strcmp("imap", sType)) GetRCString(sSetting, sPort, "143", MAX_LINE_LENGTH);
		else GetRCString(sSetting, sPort, "110", MAX_LINE_LENGTH);
		strcpy(sSetting, name);
		strcat(sSetting, "password");
		GetRCString(sSetting, sPass, "", MAX_LINE_LENGTH);
		strcpy(sSetting, name);
		strcat(sSetting, "folder");
		GetRCString(sSetting, sFolder, "inbox", MAX_LINE_LENGTH);

		// If no password was found in config, get it from database/dialog
		if (!(*sPass)) GetPass();

		// Set evars
		strcpy(sEvar, "AcidMail");
		strcat(sEvar, name);
		LSSetVariable(sEvar, "0");
		strcpy(sErrorVar, sEvar);
		strcat(sErrorVar, "Error");
		LSSetVariable(sErrorVar, "none");		
	}
	else
	{
		ErrorHandler(Error(false, LOG_ERROR, "No host found in config files", NULL));
		mail.bError = true;		
	}
}

Mailbox::~Mailbox()
{

}

Mail Mailbox::CheckMail()
{
	s = INVALID_SOCKET;
	try
	{
		char data[BUFSIZE];
		char* buf = data;

		// structs for socket info
		struct addrinfo aiHints, *aiList = NULL, *ptr = NULL;

		ZeroMemory( &aiHints, sizeof(aiHints) ); //Clear aiHints before use
		aiHints.ai_family = AF_UNSPEC;
		aiHints.ai_socktype = SOCK_STREAM;
		aiHints.ai_protocol = IPPROTO_TCP;

		// get socket info
		if ((getaddrinfo(sServer, sPort, &aiHints, &aiList)) != 0)
			throw Error(false, LOG_WARNING, "Could not find host:", sServer);

		// loop sockets
		for (ptr=aiList; (ptr != NULL) && (s == INVALID_SOCKET) ;ptr=ptr->ai_next)
		{
			// Create a SOCKET for connecting to server
			s = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
			if (s == INVALID_SOCKET) continue;
			if (connect(s, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) // Connect to server
				s = INVALID_SOCKET;
		}

		if (s == INVALID_SOCKET)
		{
			char errnum[32];
			sprintf(errnum, "%d", WSAGetLastError());
			throw Error(true, LOG_WARNING, "Could not connect to host, socket error number:", errnum);
		}

		if (!strcmp("pop3", sType))
			mail.iNumMail = Pop3Check(buf);
		else if(!strcmp("imap", sType))
			mail.iNumMail = ImapCheck(buf);
		else
			throw Error(true, LOG_ERROR, "Invalid mailbox type:", sType);

		// Try to shutdown grasefully, otherwise simply closesocket()
		if (!shutdown(s, SD_SEND))
			while(recv(s, buf, BUFSIZE, 0) > 0);
		closesocket(s);

		freeaddrinfo(aiList); // aiList needs to be freed after use

		char snum[8];
		sprintf(snum, "%d", mail.iNumMail);
		LSSetVariable(sEvar, snum);
		mail.bError = false;
	}
	catch (Error& error)
	{
		ErrorHandler(error);
		mail.bError = true;
	}
	return mail;
}

int Mailbox::Pop3Check(char*& buf)
{
	unsigned ans = 0;

	sprintf(buf, " ");
	PopCmd(buf);

	sprintf(buf, "user %s\r\n" ,sUser);
	PopCmd(buf);

	sprintf(buf, "pass %s\r\n" ,sPass);
	PopCmd(buf);

	sprintf(buf, "stat\r\n");
	PopCmd(buf);

	char test[8];
	strcpy(test, strtok(buf, " "));
	if (test != NULL)
	{
		ans = atoi(strtok(NULL, " "));

		// Get subjects and write to vars
		int subs;
		if (iSubjects < ans)
			subs = iSubjects;
		else
			subs = ans;

		for (int ansc = ans; subs > 0; subs--)
		{
			sprintf(buf, "top %d 0\r\n", subs);
			PopCmd(buf);
			char tmpbuf[8];
			recv(s, tmpbuf, 8, 0);
			char * ptr = strstr(buf, "Subject: ");
			if(ptr)
			{
				strtok(ptr, " ");
				ptr = strtok(NULL, "\r");
				char sEvarSub[140], sSubject[1024];
				sprintf(sEvarSub, "%s%s%d", sEvar, "Sub", subs);
				sprintf(sSubject, "\"%s\"", ptr);
				LSSetVariable(sEvarSub, sSubject);
			}
		}
	}
	else
		throw Error(true, LOG_ERROR, "Didn't get number of mail:", strtok(buf, "\r"));

	send(s, "quit\r\n", 6, 0);
	return ans;
}

void Mailbox::PopCmd(char*& buf)
{
	if(send(s, buf, static_cast<int>(strlen(buf)), 0) <= 0)
		throw Error(true, LOG_WARNING, "Connection error", NULL);

	if(recv(s, buf, BUFSIZE, 0) <= 0)
		throw Error(true, LOG_WARNING, "Connection error", NULL);

	if(!strstr(buf, "+OK"))
		throw Error(true, LOG_WARNING, "Bad responce:", strtok(buf, "\r"));
}

int Mailbox::ImapCheck(char*& buf)
{
	unsigned ans = 0;

	sprintf(buf, " ");
	ImapCmd(buf);
	if(buf[2] != 'O' || buf[3] != 'K') throw Error(true, LOG_WARNING, "Bad response:", strtok(buf, "\r"));

	sprintf(buf, "a01 login %s %s\r\n", sUser, sPass);
	ImapCmd(buf);
	if(buf[4] != 'O' || buf[5] != 'K') throw Error(true, LOG_WARNING, "Bad response:", strtok(buf, "\r"));

	sprintf(buf, "a02 examine %s\r\n", sFolder);
	ImapCmd(buf);
	if(!strstr(buf, "OK")) throw Error(true, LOG_WARNING, "Bad response:", strtok(buf, "\r"));

	send(s, "a03 logout\r\n", 12, 0);

/*	char * ptr = strstr(buf, "RECENT");
	if(ptr)
	{
		ptr-=3;
		while (*ptr != ' ')
			ptr-=1;
		ans = atoi(strtok(ptr, " "));
		for (int subs = iSubjects; subs > 0; subs--)
		{

		}
	}*/

	else throw Error(true, LOG_ERROR, "Didn't get number of mail:", strtok(buf, "\r"));
	return ans;
}

void Mailbox::ImapCmd(char *&buf)
{
	if(send(s, buf, static_cast<int>(strlen(buf)), 0) <= 0)
		throw Error(true, LOG_WARNING, "Connection error", NULL);
	
	if(recv(s, buf, BUFSIZE, 0) <= 0)
		throw Error(true, LOG_WARNING, "Connection error", NULL);
}

void Mailbox::ClearMail()
{
	mail.iNumMail = 0;
	mail.bError = false;
	LSSetVariable(sEvar, "0");
}

void Mailbox::GetPass()
{
	char temp[256];
	sprintf(temp, "%s/%s", sServer, sUser);
	GetProfileString("AcidMail", temp, "", temp, 256);
	if (!strlen(temp))
	{
		strcpy(temp_name, sName);
		DialogBox(hwndInstance, MAKEINTRESOURCE(IDD_GETPASS), NULL, GetPassProc);			
		if (strlen(temp_pass)) 
		{
			strcpy(sPass, temp_pass);
			sprintf(temp, "%s/%s", sServer, sUser);
			Encrypt(&temp_pass[0]);
			WriteProfileString("AcidMail", temp, temp_pass);
		}
		else
			strcpy(sPass, "");
		memset(&(temp_pass), 0, sizeof(temp_pass));
	}
	else
		Encrypt(temp);
		strcpy(sPass, temp);
}

void Mailbox::Encrypt(char* pass)
{
	for (; *pass; *pass++)
	{
		*pass=*pass ^ 0x5405;
	}
}

void Mailbox::ErrorHandler(Error& errstc)
{
	if(errstc.bSock) closesocket(s);

	char sError[1022], sError2[1024];
	sprintf(sError, "Error with mailbox: %s. %s %s", sName, errstc.sErrstr, errstc.sErradd);
	LSLog(errstc.iLvl, szAppName, sError);
	
	sprintf(sError2, "\"%s\"", sError);
	LSSetVariable(sErrorVar, sError2);
}

BOOL CALLBACK GetPassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM /* lParam */)
{
	switch (msg)
	{
		case WM_INITDIALOG: 
		{
			char temp[256] = "";
			sprintf(temp, "%s Password", temp_name);
			SetWindowText(hwnd, temp);
			return TRUE;
		}

		case WM_COMMAND:
		{
			if (HIWORD(wParam) == BN_CLICKED)
			{
				switch (LOWORD(wParam))
				{
					case IDOK:
					{
						GetDlgItemText(hwnd, IDC_PASSWORD, temp_pass, 64);
					}
					case IDCANCEL:
					{
						EndDialog(hwnd, 0);
					}
					break;
				}
			}
		}
		break;
	}

	return false;
}

// Simple default constructor for the Mail struct
Mail::Mail()
{
	iNumMail = 0;
	bError = false;
}

// Constructor for exception error struct
Error::Error(bool Sock, int lvl, const char* errstr, const char* erradd)
{
	bSock = Sock;
	iLvl = lvl;
	sErrstr = errstr;
	sErradd = erradd;
}
