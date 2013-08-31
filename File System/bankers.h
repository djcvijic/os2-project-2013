#pragma once

#include <vector>
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
	Entry entry;
	HANDLE fileMutex;
	ThreadInfo* openedBy;

	inline bool operator==(const FileInfo& lhs, const FileInfo& rhs){ return (0 == strcmp(lhs.fname, rhs.fname)); }

	FileInfo(char* fname){
		this->fname = fname;
		fileMutex = CreateSemaphore(NULL,1,1,NULL);
		openedBy = 0;
	}
}

struct ThreadInfo{
	ThreadID tid;
	vector<FileInfo> declaredFiles;
	vector<FileInfo> openedFiles;

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

	char checkSafeSequence(ThreadID tid, char* fname, char mode);

protected:
	static BankersTable instance = new BankersTable();

	vector<ThreadInfo> tableByThread;

	char checkSafeSequence(ThreadInfo threadInfo, BankersTable tempBankersTable);

	BankersTable();
};