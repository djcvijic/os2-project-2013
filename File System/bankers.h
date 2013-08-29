#pragma once

#include <map>
#include "thread.h"
#include "kernelfs.h"

struct FileInfo{
	char* myName;
	map<ThreadID, char> openedByThreads;
	map<ThreadID, char> declaredByThreads;
	HANDLE fileMutex;

	FileInfo(char* fname){
		myName = fname;
		fileMutex = CreateSemaphore(NULL,1,1,NULL);
	}
}

class BankersTable {
public:
	~BankersTable();

	BankersTable getInstance();

	char declare(ThreadID tid, char* fname);

	char undeclare(ThreadID tid, char* fname);

	char open(ThreadID tid, char* fname);

	char close(ThreadID tid, char* fname);

protected:
	static BankersTable instance = new BankersTable();

	HANDLE btMutex;

	map<char*, FileInfo*, StringComparer> fileMap;

	char checkSafeSequence(ThreadID tid, char* fname);

	BankersTable();
};