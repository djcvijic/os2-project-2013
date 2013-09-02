#pragma once

#include "types.h"
#include <map>
#include "thread.h"
#include "kernelfs.h"
#include "fs.h"

using namespace std;

class BankersTable {
public:
	map<char*, FileInfo, StringComparer> openFileMap;

	~BankersTable();

	static BankersTable& getInstance();

	char declare(ThreadID tid, char* fname);

	char undeclare(ThreadID tid, char* fname);

	char open(ThreadID tid, char* fname);

	char close(ThreadID tid, char* fname);

	char checkSafeSequence(ThreadID tid, char* fname);

protected:
	BankersTable();

	static BankersTable instance;

	map<ThreadID, ThreadInfo> tableByThread;

	char checkSafeSequence(ThreadInfo threadInfo, BankersTable tempBankersTable);
};