#include "bankers.h"
#include <iostream>

using namespace std;

BankersTable::~BankersTable(){
}

static BankersTable::BankersTable getInstance(){
	return instance;
}

char BankersTable::declare(ThreadID tid, char* fname){
	int threadN, fileN;

	for (threadN = 0; threadN <= tableByThread.size(); threadN += 1){ // Look for the thread in the thread table.
		if (tableByThread[threadN].tid == tid){
			break;
		}
	}

	if (threadN == tableByThread.size()){ // If it doesn't exist, then we have to add it.
		tableByThread.push_back(ThreadInfo(tid));		
	}

	ThreadInfo threadInfo = tableByThread[threadN];

	if (openFileMap.find(fname) == openFileMap.end()){ // If the file has never been declared before
		openFileMap.put(fname, FileInfo(fname)); // Add it to the global map.
	}

	tableByThread[threadN].declaredFiles.push_back(FileInfo(fname)); // Add the file.
	return 1;
}

char BankersTable::undeclare(ThreadID tid, char* fname){
	if (openFileMap.find(fname).openedBy->tid == tid) return 0; // File is opened by this thread and cannot be undeclared.
	
	for (int threadN = 0; threadN <= tableByThread.size(); threadN += 1){ // Look for the thread in the thread table.
		ThreadInfo threadInfo = tableByThread[threadN];

		if (threadInfo.tid == tid){
			for (int fileN = 0; fileN <= threadInfo.declaredFiles.size(); fileN += 1){ // Look for the file in the thread's declared file list.
				if (0 == strcmp(threadInfo.declaredFiles[fileN].fname, fname)){
					tableByThread[threadN].declaredFiles.erase(fileN);
					return 1;
				}
			}
		}
	}

	return 1;
}

char BankersTable::open(ThreadID tid, char* fname){
	FileInfo fileInfo = openFileMap.find(fname);
	int fileN;

	if (fileInfo.openedBy != 0){
		return 0; // If the file is already opened, return 0.
	}

	for (int threadN = 0; threadN <= tableByThread.size(); threadN += 1){ // Look for the thread in the thread table.
		ThreadInfo threadInfo = tableByThread[threadN];

		if (threadInfo.tid == tid){
			for (fileN = 0; fileN <= threadInfo.declaredFiles.size(); fileN += 1){ // Look for the file in the thread's declared file list.
				if (0 == strcmp(threadInfo.declaredFiles[fileN].fname, fname)){ // If you find it, open the file!
					tableByThread[threadN].openedFiles.push_back(FileInfo(fname));
					openFileMap.find(fname)->openedBy = &(tableByThread[threadN]);
					return 1;
				}
			}

			if (fileN == threadInfo.declaredFiles.size()){ // It means the file was not declared by the thread!
				break;
			}
		}
	}

	return 0; // By now either the thread was not found (never declared anything), or the file was not found in the threads declared file list.
}

char BankersTable::close(ThreadID tid, char* fname){
	FileInfo fileInfo = openFileMap.find(fname);

	if (fileInfo.openedBy == 0){
		return 0; // If the file is not opened, return 0.
	}

	for (int threadN = 0; threadN <= tableByThread.size(); threadN += 1){ // Look for the thread in the thread table.
		ThreadInfo threadInfo = tableByThread[threadN];

		if (threadInfo.tid == tid){
			for (int fileN = 0; fileN <= threadInfo.openedFiles.size(); fileN += 1){ // Look for the file in the thread's opened file list.
				if (0 == strcmp(threadInfo.openedFiles[fileN].fname, fname)){ // When you find it, close the file!
					tableByThread[threadN].openedFiles.erase(fileN);
					openFileMap.find(fname)->openedBy = 0;
					return 1;
				}
			}
		}
	}

	return 0; // The program should never really get here...

}

char BankersTable::checkSafeSequence(ThreadID tid, char* fname){
	BankersTable tempBankersTable = *this;

	cout << "!" << endl;

	if (0 == tempBankersTable.open(tid, fname)){
		return 0; // If file is already open, sequence is considered unsafe.
	}

	for (int threadN = 0; threadN < tableByThread.size(); threadN += 1){
		if (1 == checkSafeSequence(tableByThread[threadN], tempBankersTable)) return 1; // A safe sequence exists where threadN goes first.
	}

	return 0; // No safe sequences were found.
}

char BankersTable::checkSafeSequence(ThreadInfo threadInfo, BankersTable tempBankersTable){
	for (int decFileN = 0; decFileN < threadInfo.declaredFiles.size(); decFileN += 1){ // Go through all the files this thread has declared.
		FileInfo decFileInfo = threadInfo.declaredFiles[decFileN];

		if ((tempBankersTable.openFileMap.find(decFileInfo.fname) != tempBankersTable.openFileMap.end()) && // If this file is already opened...
			!(*(tempBankersTable.openFileMap.find(decFileInfo.fname).openedBy) == threadInfo) && // ... but not by this thread...
			(0 == tempBankersTable.open(threadInfo.tid, decFileInfo.fname))){ // ... and cannot be opened by it...
				return 0; // ... this sequence is not safe.
		}
	}
	// Side effect! All files declared by this thread will now be opened by it!
	// Getting here means the sequence is safe so far, and we can go deeper.
	for (int opFileN = 0; opFileN < threadInfo.openedFiles.size(); opFileN += 1){ // Go through all files opened by this thread and close them.
		FileInfo opFileInfo = threadInfo.openedFiles[opFileN];
		tempBankersTable.close(threadInfo.tid, opFileInfo.fname);
	}

	int threadN;

	for (threadN = 0; threadN < tableByThread.size(); threadN += 1){ // Remove this thread from further iterations.
		if (tableByThread[threadN] == threadInfo){
			tableByThread.erase(threadN);
			break;
		}
	}

	if (tableByThread.empty()){ // We got through all the threads, so the sequence we traversed was safe!
		cout << "Safe sequence (starting from last one): " << threadInfo.tid << " ";
		return 1;
	}

	for (threadN = 0; threadN < tableByThread.size(); threadN += 1){ // Find a safe sequence by recursively calling for any of the remaining threads, with the updated table.
		if (1 == checkSafeSequence(tableByThread[threadN], tempBankersTable)){  // A safe sequence exists where threadN goes next.
			cout << threadInfo.tid << " ";
			return 1;
		}
	}

	return 0;
}

BankersTable::BankersTable(){
}