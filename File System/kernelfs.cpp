#include "kernelfs.h"
#include "part.h"
#include "fs.h"
#include <cstring>

using namespace std;

KernelFS::~KernelFS(){
}

char KernelFS::mount(Partition* partition){
	wait (fsMutex);
	for (int i = 0; i < 26; i++){
		if (partitionMap[i].partition == 0){
			partitionMap[i].partition = partition;
			partitionMap[i].accessible = true;
			signal (fsMutex);
			return partitionMap[i].name;
		}
	}
	signal (fsMutex);
	return 0;
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

	if (wait(partitionMap[i].idle) != 0) return 0; // If wait failed or woken up abruptly.
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

	if (wait(partitionMap[i].idle) != 0) return 0;
	
	if ((0 == partitionMap[i].partition->getNumOfClusters()) ||
		(0 == partitionMap[i].partition->writeCluster(0, (char*) new RootCluster(0,1))))
		return 0; // Write the zero cluster to partition.
	ClusterNo j;
	ClusterNo* jPointer = new ClusterNo;
	for (j = 1; j < partitionMap[i].partition->getNumOfClusters() - 1; j++){ // Go through all remaining clusters and make a free cluster list.
		*jPointer = j + 1;
		partitionMap[i].partition->writeCluster(j, (char*) jPointer);
	}
	*jPointer = 0;
	partitionMap[i].partition->writeCluster(j, (char*) jPointer); // Write 0 into the final cluster, meaning end of list.

	signal(partitionMap[i].idle);
	partitionMap[i].accessible = true;
	return 1;
}

char KernelFS::readRootDir(char part, EntryNum n, Directory &d){
	int i = part - 'A';
	int iSrc = n, iDest;
	Directory dest;
	RootCluster* tempCluster = new RootCluster();
	if ((partitionMap[i].partition == 0) ||
		(0 == partitionMap[i].partition->getNumOfClusters() == 0))
		return 0; // Load the zero root cluster. Returns 0 if partition is not mounted or size 0.
	partitionMap[i].partition->readCluster(0, (char*) tempCluster);
	while (iSrc >= ENTRYCNT) { // If n is greater than the number of entries in a root cluster, skip some root clusters until we get to the one where the nth entry is located.
		iSrc -= ENTRYCNT;
		if ((tempCluster->nextRootCluster == 0) ||
			(tempCluster->nextRootCluster >= partitionMap[i].partition->getNumOfClusters()))
			return 0; // If there is no next root cluster, or it is out of range, that means n is out of range.
		partitionMap[i].partition->readCluster(tempCluster->nextRootCluster, (char*) tempCluster); // Load the next root cluster.
	}
	for (iDest = 0; iSrc < ENTRYCNT; iSrc++, iDest++){ // iSrc now points to the nth root entry, and we start storing it and all subsequent entries in dest...
		if (tempCluster->rootDirEntries[iSrc].name == 0){ // ... All the while checking whether the current entry is empty in which case we return iDest, the number of read entries...
			d = dest;
			return iDest;
		}
		dest[iDest] = tempCluster->rootDirEntries[iSrc];
	}
	if (tempCluster->nextRootCluster == 0){ // If there is no next root cluster, we have precisely finished reading entries at the end of the first one, and may return...
		d = dest;
		return iDest - 1;
	}
	if (tempCluster->nextRootCluster >= partitionMap[i].partition->getNumOfClusters()) return 0;
	partitionMap[i].partition->readCluster(tempCluster->nextRootCluster, (char*) tempCluster); // ... Or else we load the next root cluster.
	for (iSrc = 0; iDest < ENTRYCNT; iSrc++, iDest++){ // Now we start from the very first entry, and continue filling up dest from where we left off with iDest.
		if (tempCluster->rootDirEntries[iSrc].name == 0){ // The rest of the loop is same as before.
			d = dest;
			return iDest;
		}
		dest[iDest] = tempCluster->rootDirEntries[iSrc];
	}
	return ENTRYCNT + (tempCluster->rootDirEntries[iSrc].name == 0) ? 0 : 1; // If there are no more entries in the directory, return 64, else return 65.
}

File* KernelFS::findFile(char* fname){
	int i = fname[0] - 'A';
	int charDest, charSrc;
	char* relFName = new char[FNAMELEN + 1];
	char* relFExt = new char[FEXTLEN + 1];
	relFName = "        ";
	relFExt = "   ";

	for (charDest = 0, charSrc = 4; fname[charSrc] != '.'; charDest++, charSrc++){ // Load the relative name of the file without the extension.
		if (charDest >= FNAMELEN) return 0; // The file name is above the maximum length.
		relFName[charDest] = fname[charSrc];
	}
	for (charDest = 0, charSrc += 1; fname[charSrc] != 0; charDest++, charSrc++){ // Load the extension of the file.
		if (charDest >= FEXTLEN) return 0; // The file extention is above the maximum length.
		relFExt[charDest] = fname[charSrc];
	}

	RootCluster* tempCluster = new RootCluster();
	if ((partitionMap[i].partition == 0) ||
		(0 == partitionMap[i].partition->getNumOfClusters))
		return 0; // If partition is not mounted or size 0, return 0.
	partitionMap[i].partition->readCluster(0, (char*) tempCluster); // Load zero root cluster.
	while (1){	
		EntryNum entry;
		for (entry = 0; entry < ENTRYCNT; entry++){ // Go through all the entries. If we come across an empty one, there are no more entries and we can return 0.
			if (tempCluster->rootDirEntries[entry].name == 0) return 0;
			if ((0 == strcmp(tempCluster->rootDirEntries[entry].name, relFName)) && // We check the filename...
				(0 == strcmp(tempCluster->rootDirEntries[entry].ext, relFExt))) // ... As well as the extension...
				return new File(tempCluster->rootDirEntries[entry]); // ... And if they are both equal, return 1.
		}
		if ((tempCluster->nextRootCluster == 0) ||
			(tempCluster->nextRootCluster >= partitionMap[i].partition->getNumOfClusters()))
			return 0; // If there is no next cluster, return 0...
		partitionMap[i].partition->readCluster(tempCluster->nextRootCluster, (char*) tempCluster); // ... And if there is, go to the next one and start over.
	}
}

char KernelFS::doesExist(char* fname){
	File* tempFile = findFile(fname);
	try{
		if (tempFile != 0){
			return 1;
		}
		return 0;
	}
	finally{
		delete tempFile;
	}
}

char KernelFS::declare(char* fname, int mode){
	int tid = GetCurrentThreadId();
	File* file = findFile(fname);

	if (0 == file) return 0; // File is nonexistant.

	if (threadMap.find(tid) == threadMap.end()){ // Then we have to add current thread to the map.
		threadMap[tid] = new TheadDescriptor(tid);
	}

	if (1 == mode){
		threadMap[tid]->declaredMap[file] = 1;
	}
	else{
		if (file->openedBy != 0) return 0; // File is open and cannot be undeclared.
		threadMap[tid]->declaredMap[file] = 0;
	}

	return 1;
}

File* KernelFS::open(char* fname){
}

char KernelFS::deleteFile(char* fname){
}

KernelFS::KernelFS(){
	partitionMap = new PartitionMapEntry[26];
	for (int i = 0; i < 26; i++){
		partitionMap[i] = new PartitionMapEntry(i);
	}

	fsMutex = CreateSemaphore(NULL,1,1,NULL);

	threadMap = new map<ThreadID, ThreadDescriptor*>();

	fileMap = new map<char*, File*>();
}