#include "bankers.h"

using namespace std;

BankersTable::~BankersTable(){
}

BankersTable::BankersTable getInstance(){
	return instance;
}

char BankersTable::declare(ThreadID tid, char* fname){
	if (fileMap.find(fname) == fileMap.end()){ // Then we have to add current thread to the map.
		wait (btMutex);
		if (fileMap.find(fname) == fileMap.end()){
			fileMap[fname] = new FileInfo(fname);
		}
		signal (btMutex);
	}

	fileMap[fname]->declaredByThreads[tid] = 1;

	return 1;
}

char BankersTable::undeclare(ThreadID tid, char* fname){
	if (fileMap.find(fname) == fileMap.end()){ // Then we have to add current thread to the map.
		wait (btMutex);
		if (fileMap.find(fname) == fileMap.end()){
			fileMap[fname] = new FileInfo(fname);
		}
		signal (btMutex);
	}

	if (1 == fileMap[fname]->openedByThreads[tid]) return 0; // File is opened by this thread and cannot be undeclared.
	fileMap[fname]->declaredByThreads[tid] = 0;

	return 1;
}

char BankersTable::open(ThreadID tid, char* fname){
	FileInfo* fileInfo = fileMap.find(fname);

	wait(btMutex);

	if (fileInfo == fileMap.end() || // If file is not in the file map, means it has not been declared.
		fileInfo->declaredByThreads[tid] != 1){ // If it has not been declared, return 0.
			return 0;
	}

	/*map<ThreadID, char>::iterator iter;
	for (iter = fileInfo->openedByThreads.begin(); iter != fileInfo->openedByThreads.end(); ++iter){
		if (1 == iter->second) return 0; // If the file is already opened by any other thread, return 0.
	}*/

	while(0 == checkSafeSequence(tid, fname)){
		signal(btMutex);
		wait(fileInfo->fileMutex);
		wait(btMutex);
	}


}

char BankersTable::close(ThreadID tid, char* fname){
}

char BankersTable::checkSafeSequence(ThreadID tid, char* fname){
	FileInfo* fileInfo = fileMap.find(fname);



}

BankersTable::BankersTable(){
	btMutex = CreateSemaphore(NULL,1,1,NULL);
}