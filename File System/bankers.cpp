#include "bankers.h"
#include <cstring>
#include <iostream>

using namespace std;

BankersTable::~BankersTable(){
}

static BankersTable& BankersTable::getInstance(){
	return instance;
}

char BankersTable::declare(ThreadID tid, char* fname){
	int threadN, fileN;

	if (tableByThread.find(tid) == tableByThread.end()){ // If the thread doesn't exist, then we have to add it.
		tableByThread[tid] = ThreadInfo(tid);
	}

	if (openFileMap.find(fname) == openFileMap.end()){ // If the file has never been declared before
		openFileMap[fname] = FileInfo(fname); // Add it to the global map.
	}

	if (tableByThread[tid].declaredFiles.find(fname) == tableByThread[tid].declaredFiles.end()){ // If the file has not already been declared by that thread.
		tableByThread[tid].declaredFiles[fname] = FileInfo(fname); // Add the file.
	}

	return 1;
}

char BankersTable::undeclare(ThreadID tid, char* fname){
	if (openFileMap[fname].openedBy == tid) return 0; // File is opened by this thread and cannot be undeclared.
	
	tableByThread[tid].declaredFiles.erase(fname);

	return 1;
}

char BankersTable::open(ThreadID tid, char* fname){
	FileInfo fileInfo = openFileMap[fname];
	int fileN;

	if (fileInfo.openedBy != -1){
		return 0; // If the file is already opened, return 0.
	}

	if (tableByThread.find(tid) == tableByThread.end() ||
		openFileMap.find(fname) == openFileMap.end()){
			return 0; // Either the thread was not found (never declared anything), or the file was not found in the threads declared file list.
	}

	tableByThread[tid].openedFiles[fname] = FileInfo(fname);
	openFileMap[fname].openedBy = tid;
	
	return 1;
}

char BankersTable::close(ThreadID tid, char* fname){
	FileInfo fileInfo = openFileMap[fname];

	if (fileInfo.openedBy == -1){
		return 0; // If the file is not opened, return 0.
	}

	tableByThread[tid].openedFiles.erase(fname);
	openFileMap[fname]->openedBy = -1;
	return 1;
}

char BankersTable::checkSafeSequence(ThreadID tid, char* fname){
	BankersTable tempBankersTable = *this;

	if (0 == tempBankersTable.open(tid, fname)){
		return 0; // If file is already open, sequence is considered unsafe.
	}

	for( map<ThreadID,ThreadInfo>::const_iterator it = tempBankersTable.tableByThread.begin(); it != tempBankersTable.tableByThread.end(); ++it )
    {
		if (1 == checkSafeSequence(it->second, tempBankersTable)){
			cout << "!" << endl;
			return 1; // A safe sequence exists where threadN goes first.
		}
    }

	cout << "No safe sequence :(" << endl;
	return 0; // No safe sequences were found.
}

char BankersTable::checkSafeSequence(ThreadInfo threadInfo, BankersTable tempBankersTable){
	for( map<char*, FileInfo, StringComparer>::const_iterator it = threadInfo.declaredFiles.begin(); it != threadInfo.declaredFiles.end(); ++it ) // Go through all declared files.
	{
		if ((tempBankersTable.openFileMap.find(it->first) != tempBankersTable.openFileMap.end()) && // If this file is already opened...
			(tempBankersTable.openFileMap[it->first].openedBy != threadInfo.tid) && // ... but not by this thread...
			(0 == tempBankersTable.open(threadInfo.tid, it->first))){ // ... and cannot be opened by it...
				return 0; // ... this sequence is not safe.
		}
	}

	// Side effect! All files declared by this thread will now be opened by it!
	// Getting here means the sequence is safe so far, and we can go deeper.

	for( map<char*, FileInfo, StringComparer>::const_iterator it = threadInfo.openedFiles.begin(); it != threadInfo.openedFiles.end(); ++it ) // Go through all files opened by this thread and close them.
	{
		tempBankersTable.close(threadInfo.tid, it->first);
	}

	tempBankersTable.tableByThread.erase(tid); // Remove this thread from further iterations.

	if (tempBankersTable.tableByThread.empty()){ // We got through all the threads, so the sequence we traversed was safe!
		cout << "Safe sequence :D (starting from last one): " << threadInfo.tid << " ";
		return 1;
	}

	for( map<ThreadID,ThreadInfo>::const_iterator it = tempBankersTable.tableByThread.begin(); it != tempBankersTable.tableByThread.end(); ++it )
    {
		if (1 == checkSafeSequence(it->second, tempBankersTable)){  // A safe sequence exists where threadN goes next.
			cout << threadInfo.tid << " ";
			return 1;
		}
    }

	return 0;
}

BankersTable::BankersTable(){
}

BankersTable::instance = BankersTable();