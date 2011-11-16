#include "acidmail.h"

DWORD WINAPI StubCheckMail(LPVOID pData);
extern HWND hwndMain;

Acidmail::Acidmail():
	CheckThread(0),
	iNumMsgsOld(0),
	bError(false),
	bAbort(false)
{
	// Get .rc vars
	iTimer = GetRCInt("AcidMailTimer", 5);
	iNumSubjects = GetRCInt("AcidMailSubjects", 0);
	GetRCLine("AcidMailMailCmd", sMailCmd, MAX_LINE_LENGTH, "!none");
	GetRCLine("AcidMailNewMailCmd", sNewMailCmd, MAX_LINE_LENGTH, "!none");
	GetRCLine("AcidMailNoMailCmd", sNoMailCmd, MAX_LINE_LENGTH, "!none");
	GetRCLine("AcidMailZeroMailCmd", sZeroMailCmd, MAX_LINE_LENGTH, "!none");;
	GetRCLine("AcidMailFoundMailCmd", sFoundMailCmd, MAX_LINE_LENGTH, "!none");;
	GetRCLine("AcidMailCheckMailCmd", sCheckMailCmd, MAX_LINE_LENGTH, "!none");
	GetRCLine("AcidMailDoneCheckCmd", sDoneCheckCmd, MAX_LINE_LENGTH, "!none");
	GetRCLine("AcidMailErrorCmd", sErrorCmd, MAX_LINE_LENGTH, "!none");

	// Set evars
	LSSetVariable("AcidMailNumMail", "0");

	// Get *AcidMailbox lines, parse them and make new mailboxes
	if (GetMailboxes())
	{
		// check mail for the first time if timer != 0
		if (iTimer != 0)		
			CheckMail();
	}
}

Acidmail::~Acidmail()
{
	// Kill timer
	KillTimer(hwndMain, 0);

	// Kill thread
	if (CheckThread)
	{
		bAbort = true;
		WaitForSingleObject(CheckThread, INFINITE);
		CloseHandle(CheckThread);
	}

	// Delete Mailboxes
	for (mailboxiter = mailboxlist.begin(); mailboxiter != mailboxlist.end(); mailboxiter++ )
		delete *mailboxiter;
}

int Acidmail::GetMailboxes()
{
	LPVOID f = LCOpen(NULL);
	if(f != NULL)
	{
		char lineBuffer[MAX_LINE_LENGTH] = {0};

		while(LCReadNextConfig(f, "*AcidMailbox", lineBuffer, MAX_LINE_LENGTH))
		{
			char temp[16], name[64], type[64], server[128], user[64], pass[64] = {0};
			char *buffers[] = {temp, name ,type, server, user};
			
			if(LCTokenize(lineBuffer, buffers, 5, pass) >= 2)
			{
				Mailbox *newmailbox = new Mailbox(name, type, server, user, pass);
				
				if ( newmailbox )
				{	
					mailboxlist.insert(mailboxlist.end(), newmailbox);
				}
			}
		}
		LCClose(f);
		return 1;
	}
	return 0;
}

int Acidmail::CheckMail()
{
    DWORD dwDummy = 0;

    if (GetExitCodeThread(CheckThread, &dwDummy) != STILL_ACTIVE)
    {
        if (CheckThread)
        {
			WaitForSingleObject(CheckThread, INFINITE);
            CloseHandle(CheckThread);
        }

		CheckThread = CreateThread(NULL, 0, ::StubCheckMail,
            (LPVOID)this, 0, &dwDummy);
    }

    return (CheckThread != NULL);
}

int Acidmail::CheckMailThread()
{
	KillTimer(hwndMain, 0); // Kill the timer to prevent the multiple threads from running
	char snum[8];
	iNumMsgs = 0;
	if (!mailboxlist.empty())
	{
		LSExecute(NULL, sCheckMailCmd, 0); //Fire CheckMailCmd before checking
		for (mailboxiter = mailboxlist.begin(); mailboxiter != mailboxlist.end(); mailboxiter++ )
		{
			if (bAbort) // Don't check mail if the main thread wants this thread to close
				return 0;
			Mail mail;
			mail = (*mailboxiter)->CheckMail();
			iNumMsgs += mail.iNumMail;
			if (!bError)
				bError = mail.bError;
		}
		sprintf(snum, "%d", iNumMsgs);
		LSSetVariable("AcidMailNumMail", snum);
		if (iNumMsgsOld != iNumMsgs) // Fire some other commands after checking
		{
			if (iNumMsgsOld == 0)
				LSExecute(NULL, sMailCmd, 0);
			if (iNumMsgs == 0)
				LSExecute(NULL, sNoMailCmd, 0);
			if (iNumMsgs != 0)
				LSExecute(NULL, sNewMailCmd, 0);
			iNumMsgsOld = iNumMsgs;
		}
		if (iNumMsgs == 0)
			LSExecute(NULL, sZeroMailCmd, 0);
		else
			LSExecute(NULL, sFoundMailCmd, 0);
		LSExecute(NULL, sDoneCheckCmd, 0);
		if (bError)
            LSExecute(NULL, sErrorCmd, 0);
		bError = false;
	}
	SetTimer(hwndMain, 0, iTimer*60000, NULL); //(Re)Set timer when thread is done. Sends WM_TIMER message after time
	return 0;
}

// Function called by thread
DWORD WINAPI StubCheckMail(LPVOID pData)
{
	Acidmail* pMail = reinterpret_cast<Acidmail*>(pData);

    if (pMail)
    {
        return pMail->CheckMailThread() ? 1 : 0;
    }
    return 0;
}

void Acidmail::ClearMail()
{
    iNumMsgs = 0; iNumMsgsOld = 0;
	for (mailboxiter = mailboxlist.begin(); mailboxiter != mailboxlist.end(); mailboxiter++ )
	{
		(*mailboxiter)->ClearMail();
	}
	LSSetVariable("AcidMailNumMail", "0");
	LSExecute(NULL, sNoMailCmd, 0);
}

void Acidmail::SetPass(char* server, char* user, char* pass)
{
	char temp[256];
	sprintf(temp, "%s/%s", server, user);
	Mailbox::Encrypt(pass);
	WriteProfileString("AcidMail", temp, pass);
}
