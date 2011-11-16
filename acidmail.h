#if !defined(ACIDMAIL_H)
#define ACIDMAIL_H

#include <list>
#include "..\lsapi.h"
#include "mailbox.h"

class Acidmail
{
public:
	Acidmail();
	~Acidmail();
	int CheckMail();
	void ClearMail();
	int CheckMailThread();
	void SetPass(char* server, char* user, char* pass);

	unsigned int iNumSubjects;

private:
	int GetMailboxes();

	std::list<Mailbox*> mailboxlist;
	std::list<Mailbox*>::iterator mailboxiter;
	
	HANDLE CheckThread;
	unsigned int iNumMsgs, iNumMsgsOld, iTimer;
	char sMailCmd[MAX_LINE_LENGTH], sNewMailCmd[MAX_LINE_LENGTH], 
		sNoMailCmd[MAX_LINE_LENGTH], sZeroMailCmd[MAX_LINE_LENGTH],
		sFoundMailCmd[MAX_LINE_LENGTH], sCheckMailCmd[MAX_LINE_LENGTH],
		sDoneCheckCmd[MAX_LINE_LENGTH],	sErrorCmd[MAX_LINE_LENGTH];
	bool bError, bAbort;
};

#endif // ACIDMAIL_H