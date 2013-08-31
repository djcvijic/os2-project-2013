#include "kernelfs.h"
#include "part.h"
#include "fs.h"
#include <cstring>

using namespace std;

KernelFS::~KernelFS(){
}

char KernelFS::mount(Partition* partition){
	wait (fsMutex);
	int i = 0;
	for (; i < 26; i++){
		if (partitionMap[i].partition == 0){
			partitionMap[i].partition = partition;
			partitionMap[i].accessible = true;
			break;
		}
	}
	signal (fsMutex);
	return (i < 26) ? partitionMap[i].name : 0; // If a mount point was found, return its partition letter, otherwise 0.
}

char KernelFS::unmount(char part){
	wait (fsMutex);
	int i = part - 'A';
	if (partitionMap[i].partition == 0 || partitionMap[i].accessible == false) {
		signal (fsMutex);
		return 0; // Unmounting fails if partition is not mounted, or a similar operation has already been initiated.
	}
	partitionMap[i].accessible = false; // You can't open any new files.
	signal (fsMutex);

	if (wait(partitionMap[i].idle) != 0){
		signal(partitionMap[i].idle);
		return 0; // If wait failed or woken up abruptly.
	}
	partitionMap[i].partition = 0;
	signal(partitionMap[i].idle);
	return 1;
}

char KernelFS::format(char part){
	wait (fsMutex);
	int i = part - 'A';
	if (partitionMap[i].partition == 0 || partitionMap[i].accessible == false) {
		signal (fsMutex);
		return 0;
	}
	partitionMap[i].accessible = false;
	signal (fsMutex);

	if (wait(partitionMap[i].idle) != 0){
		signal(partitionMap[i].idle);
		return 0;
	}
	
	if ((0 == partitionMap[i].partition->getNumOfClusters()) ||
		(0 == partitionMap[i].partition->writeCluster(0, (char*) new RootCluster(0,1)))){ // Write the zero cluster to partition.
			signal(partitionMap[i].idle);
			return 0;
	}
	ClusterNo j = 1;
	ClusterNo* jPointer = new ClusterNo;
	for (; j < partitionMap[i].partition->getNumOfClusters() - 1; j++){ // Go through all remaining clusters and make a free cluster list.
		*jPointer = j + 1;
		partitionMap[i].partition->writeCluster(j, (char*) jPointer);
	}
	*jPointer = 0;
	partitionMap[i].partition->writeCluster(j, (char*) jPointer); // Write 0 into the final cluster, meaning end of list.
	delete jPointer;

	signal(partitionMap[i].idle);
	partitionMap[i].accessible = true;
	return 1;
}

char KernelFS::readRootDir(char part, EntryNum n, Directory &d){
	int i = part - 'A';
	int iSrc = n, iDest = 0;
	RootCluster* tempCluster = new RootCluster();
	if ((partitionMap[i].partition == 0) ||
		(0 == partitionMap[i].partition->getNumOfClusters() == 0))
		return 0; // Returns 0 if partition is not mounted or size 0.
	partitionMap[i].partition->readCluster(0, (char*) tempCluster); // Load the zero root cluster.
	while (iSrc >= ENTRYCNT) { // If n is greater than the number of entries in a root cluster, skip some root clusters until we get to the one where the nth entry is located.
		iSrc -= ENTRYCNT;
		if (tempCluster->nextRootCluster == 0)
			return 0; // If there is no next root cluster, that means n is out of range.
		partitionMap[i].partition->readCluster(tempCluster->nextRootCluster, (char*) tempCluster); // Load the next root cluster.
	}

	wait(partitionMap[i].idle);
	for (; iSrc < ENTRYCNT; iSrc++, iDest++){ // iSrc now points to the nth root entry, and we start storing it and all subsequent entries in dest...
		if (tempCluster->rootDirEntries[iSrc].name == 0){ // ... All the while checking whether the current entry is empty in which case we return iDest, the number of read entries...
			signal(partitionMap[i].idle);
			return iDest;
		}
		d[iDest] = tempCluster->rootDirEntries[iSrc];
	}
	if (tempCluster->nextRootCluster == 0){ // If there is no next root cluster, we have precisely finished reading entries at the end of the first one, and may return...
		signal(partitionMap[i].idle);
		return iDest - 1;
	}
	partitionMap[i].partition->readCluster(tempCluster->nextRootCluster, (char*) tempCluster); // ... Or else we load the next root cluster.
	for (iSrc = 0; iDest < ENTRYCNT; iSrc++, iDest++){ // Now we start from the very first entry, and continue filling up dest from where we left off with iDest.
		if (tempCluster->rootDirEntries[iSrc].name == 0){ // The rest of the loop is same as before.
			signal(partitionMap[i].idle);
			return iDest;
		}
		d[iDest] = tempCluster->rootDirEntries[iSrc];
	}
	signal(partitionMap[i].idle);
	return ENTRYCNT + (tempCluster->rootDirEntries[iSrc].name == 0) ? 0 : 1; // If there are no more entries in the directory, return 64, else return 65.
}

Entry KernelFS::findFile(char* fname){
	int i = fname[0] - 'A';
	int charDest, charSrc;
	char relFName[] = "        ";
	char relFExt[] = "   ";

	for (charDest = 0, charSrc = 4; fname[charSrc] != '.'; charDest++, charSrc++){ // Load the relative name of the file without the extension.
		if (charDest >= FNAMELEN) return 0; // The file name is above the maximum length.
		relFName[charDest] = fname[charSrc];
	}
	charSrc += 1;
	for (charDest = 0; fname[charSrc] != 0; charDest++, charSrc++){ // Load the extension of the file.
		if (charDest >= FEXTLEN) return 0; // The file extention is above the maximum length.
		relFExt[charDest] = fname[charSrc];
	}

	RootCluster* tempCluster = new RootCluster();
	if ((partitionMap[i].partition == 0) ||
		(0 == partitionMap[i].partition->getNumOfClusters))
		return 0; // If partition is not mounted or size 0, return 0.

	partitionMap[i].partition->readCluster(0, (char*) tempCluster); // Load zero root cluster.
	while (1){	
		EntryNum entry = 0;
		for (; entry < ENTRYCNT; entry++){ // Go through all the entries. If we come across an empty one, there are no more entries and we can return 0.
			if (tempCluster->rootDirEntries[entry].name == 0){
				return 0;
			}
			if ((0 == strcmp(tempCluster->rootDirEntries[entry].name, relFName)) && // We check the filename...
				(0 == strcmp(tempCluster->rootDirEntries[entry].ext, relFExt))){ // ... As well as the extension...
					return tempCluster->rootDirEntries[entry]; // ... And if they are both equal, return 1.
			}
		}
		if (tempCluster->nextRootCluster == 0){
			return 0; // If there is no next cluster, return 0...
		}
		partitionMap[i].partition->readCluster(tempCluster->nextRootCluster, (char*) tempCluster); // ... And if there is, go to the next one and start over.
	}
}

char KernelFS::doesExist(char* fname){
	return (0 != findFile(fname));
}

char KernelFS::declare(char* fname, int mode){
	char retVal;
	ThreadID tid = GetCurrentThreadId();
	wait(fsMutex);

	if (1 == mode){
		retVal = bankersTable.declare(tid, fname);
	} else {
		retVal = bankersTable.undeclare(tid, fname);
	}

	signal(fsMutex);
	return retVal;
}

File* KernelFS::open(char* fname){
	ThreadID tid = GetCurrentThreadId();
	wait(fsMutex);

	while(0 == bankersTable.checkSafeSequence(tid, fname)){
		signal(fsMutex);
		wait(bankersTable.openFileMap.find(fname).fileMutex);
		wait(fsMutex);
	}

	signal(fsMutex);
}

char KernelFS::deleteFile(char* fname){
}

KernelFS::KernelFS(){
	partitionMap = new PartitionMapEntry[26];
	for (int i = 0; i < 26; i++){
		partitionMap[i] = new PartitionMapEntry(i);
	}

	fsMutex = CreateSemaphore(NULL,1,1,NULL);

	bankersTable = BankersTable::getInstance();
}