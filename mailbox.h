#if !defined(MAILBOX_H)
#define MAILBOX_H

#include <winsock2.h>
#include <ws2tcpip.h>

struct Mail
{
	Mail();
	unsigned int iNumMail;
	bool bError;
};

struct Error
{
	Error(bool Sock, int lvl, const char* errstr, const char* erradd);
	bool bSock;
	int iLvl;
	const char* sErrstr;
	const char* sErradd;
};

class Mailbox
{
public:
	Mailbox(char* name, char* type, char* server, char* user, char* pass);
	~Mailbox();
	Mail CheckMail();
	void ClearMail();
	static void Encrypt(char* pass);

private:
	int Pop3Check(char*& buf);
	void PopCmd(char*& buf);
	int ImapCheck(char*& buf);
	void ImapCmd(char*& buf);
	void GetPass();
	void ErrorHandler(Error& errstc);

	char sName[64], sType[64], sServer[128], sUser[64], sPass[64], sEvar[72],
		sSetting[72], sFolder[32], sErrorVar[72], sPort[8];
	unsigned int iSubjects;
	Mail mail;
	SOCKET s;
};

#endif // MAILBOX_H