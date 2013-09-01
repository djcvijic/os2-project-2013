#pragma once

#include <map>
#include "thread.h"
#include "kernelfs.h"

struct StringComparer{
    bool operator()(const char* x, const char* y)
    {
         return strcmp(x,y) < 0;
    }
};

struct FileInfo{
	char* fname;
	HANDLE fileMutex;
	tid openedBy;
	FileLocation fileLocation;

	inline bool operator==(const FileInfo& lhs, const FileInfo& rhs){ return (0 == strcmp(lhs.fname, rhs.fname)); }

	FileInfo(char* fname){
		this->fname = fname;
		fileMutex = CreateSemaphore(NULL,1,1,NULL);
		openedBy = -1;
	}
}

struct ThreadInfo{
	ThreadID tid;
	map<char*, FileInfo, StringComparer> declaredFiles;
	map<char*, FileInfo, StringComparer> openedFiles;

	inline bool operator==(const ThreadInfo& lhs, const ThreadInfo& rhs){ return (lhs.tid == rhs.tid); }

	ThreadInfo(tid){
		this.tid = tid;
	}
}

class BankersTable {
public:
	map<char*, FileInfo, StringComparer> openFileMap;

	~BankersTable();

	static BankersTable getInstance();

	char declare(ThreadID tid, char* fname);

	char undeclare(ThreadID tid, char* fname);

	char open(ThreadID tid, char* fname);

	char close(ThreadID tid, char* fname);

	char checkSafeSequence(ThreadID tid, char* fname);

protected:
	static BankersTable instance = new BankersTable();

	map<ThreadID, ThreadInfo> tableByThread;

	char checkSafeSequence(ThreadInfo threadInfo, BankersTable tempBankersTable);

	BankersTable();
};