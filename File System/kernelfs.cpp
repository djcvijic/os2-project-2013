#include "kernelfs.h"
#include "part.h"
#include "fs.h"
#include "kernelfile.h"
#include <cstring>
#include <map>

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
	RootCluster* zeroRootCluster = new RootCluster(0,1);
	if ((0 == partitionMap[i].partition->getNumOfClusters()) ||
		(0 == partitionMap[i].partition->writeCluster(0, (char*) zeroRootCluster))){ // Write the zero cluster to partition.
			delete zeroRootCluster;
			signal(partitionMap[i].idle);
			return 0;
	}
	delete zeroRootCluster;
	ClusterNo j = 1;
	ClusterNo* jPointer = new ClusterNo();
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
		(0 == partitionMap[i].partition->getNumOfClusters() == 0)){
			delete tempCluster;
			return 0; // Returns 0 if partition is not mounted or size 0.
	}
	partitionMap[i].partition->readCluster(0, (char*) tempCluster); // Load the zero root cluster.
	while (iSrc >= ENTRYCNT) { // If n is greater than the number of entries in a root cluster, skip some root clusters until we get to the one where the nth entry is located.
		iSrc -= ENTRYCNT;
		if (tempCluster->nextRootCluster == 0){
			delete tempCluster;
			return 0; // If there is no next root cluster, that means n is out of range.
		}
		partitionMap[i].partition->readCluster(tempCluster->nextRootCluster, (char*) tempCluster); // Load the next root cluster.
	}

	wait(partitionMap[i].idle);
	for (; iSrc < ENTRYCNT; iSrc++, iDest++){ // iSrc now points to the nth root entry, and we start storing it and all subsequent entries in dest...
		if (tempCluster->rootDirEntries[iSrc].name == 0){ // ... All the while checking whether the current entry is empty in which case we return iDest, the number of read entries...
			delete tempCluster;
			signal(partitionMap[i].idle);
			return iDest;
		}
		d[iDest] = tempCluster->rootDirEntries[iSrc];
	}
	if (tempCluster->nextRootCluster == 0){ // If there is no next root cluster, we have precisely finished reading entries at the end of the first one, and may return...
		delete tempCluster;
		signal(partitionMap[i].idle);
		return iDest - 1;
	}
	partitionMap[i].partition->readCluster(tempCluster->nextRootCluster, (char*) tempCluster); // ... Or else we load the next root cluster.
	for (iSrc = 0; iDest < ENTRYCNT; iSrc++, iDest++){ // Now we start from the very first entry, and continue filling up dest from where we left off with iDest.
		if (tempCluster->rootDirEntries[iSrc].name == 0){ // The rest of the loop is same as before.
			delete tempCluster;
			signal(partitionMap[i].idle);
			return iDest;
		}
		d[iDest] = tempCluster->rootDirEntries[iSrc];
	}
	char noMoreEntries = (tempCluster->rootDirEntries[iSrc].name == 0);
	delete tempCluster;
	signal(partitionMap[i].idle);
	return ENTRYCNT + (noMoreEntries) ? 0 : 1; // If there are no more entries in the directory, return 64, else return 65.
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
	File* retVal;
	FileLocation fileLocation;
	ThreadID tid = GetCurrentThreadId();
	wait(fsMutex);

	while(0 == bankersTable.checkSafeSequence(tid, fname)){
		signal(fsMutex);
		wait(bankersTable.openFileMap[fname].fileMutex);
		wait(fsMutex);
	}

	bankersTable.open(tid, fname);
	fileLocation = findFile(fname);
	if (0 == fileLocation) fileLocation = createFile(fname);
	bankersTable.openFileMap[fname].fileLocation = fileLocation;
	retVal = KernelFile::infoToFile(bankersTable.openFileMap[fname]);
	signal(fsMutex);
	return retVal;
}

char KernelFS::deleteFile(char* fname){
	ThreadID tid = GetCurrentThreadId();
	int i = fname[0] - 'A';
	File* deletingFile;

	wait(fsMutex);
	bankersTable.undeclare(tid, fname);

	if ((bankersTable.openFileMap.find(fname) != bankersTable.openFileMap.end()) &&
		(bankersTable.openFilemap[fname].openedBy != -1)){ // File is still open and cannot be deleted.
			signal(fsMutex);
			return 0;
	}

	for( map<ThreadID,ThreadInfo>::const_iterator it = bankersTable.tableByThread.begin(); it != bankersTable.tableByThread.end(); ++it ) // Search all threads.
	{
		if (it->second.declaredFiles.find(fname) != it->second.declaredFiles.end()) { // File is still declared by some other threads.
			signal(fsMutex);
			return 0;
		}
	}

	// Getting here means file is not open, and is not declared by any threads.
	// Now for actually deleting all index and data clusters...

	FileLocation fileLocation = bankersTable.openFileMap[fname].fileLocation;
	ClusterNo indexClusterNo = fileLocation.entry.firstIndexCluster; // Setting the index cluster number to the first index cluster number.
	ClusterNo dataClusterNo;
	ClusterNo freeClusterNo;
	IndexCluster* indexCluster = new IndexCluster();
	RootCluster* rootCluster = new RootCluster();

	partitionMap[i].partition->readCluster(0, (char*) rootCluster); // Reading the zero root cluster.
	freeClusterNo = rootCluster->prevRootCluster; // Saving the first free cluster number.
	rootCluster->prevRootCluster = indexClusterNo;
	partitionMap[i].partition->writeCluster(0, (char*) rootCluster); // Chaining the head of free clusters to the file's first index cluster.

	while(1){
		partitionMap[i].partition->readCluster(indexClusterNo, (char*) indexCluster);

		if (0 == indexCluster->fileIndex[0]){ // If this index had no data, treat the index as its own last data cluster.
			dataClusterNo = indexClusterNo;
		} else {
			partitionMap[i].partition->writeCluster(indexClusterNo, (char*) indexCluster->fileIndex[0]); // Chaining the index cluster to its first data cluster.

			for (EntryNum e = 0; e < INDEXCNT; e += 1){ // Go through all file indices in that index cluster and chain them together. At the end, dataClusterNo will point to the last used data cluster.
				if (indexCluster->fileIndex[e] == 0) break;
				dataClusterNo = indexCluster->fileIndex[e];
				if (indexCluster->fileIndex[e + 1] != 0){
					partitionMap[i].partition->writeCluster(dataClusterNo, (char*) indexCluster->fileIndex[e + 1]);
				}
			}
		}

		indexClusterNo = indexCluster->nextIndexCluster;
		if (0 == indexClusterNo) break; // Go to the next index cluster, and if it doesn't exist, go to final step.
		partitionMap[i].partition->writeCluster(dataClusterNo, (char*) indexClusterNo); // Otherwise chain the last data cluster to the next index cluster.
	}

	partitionMap[i].partition->writeCluster(dataClusterNo, (char*) freeClusterNo); // Last step: anything we last found used, be it a data or index cluster, gets chained with all the old free clusters.

	// At this point all of the file's index and data clusters have been cleared and rechained.
	// Now we must remove its entry from the directory, and clean up the directory.

	EntryNum e = fileLocation.entryNum;
	ClusterNo rootClusterNo = fileLocation.clusterNo;
	partitionMap[i].partition->readCluster(rootClusterNo, (char*) rootCluster);
	rootCluster->rootDirEntries[e] = 0; // This kills the crab.

	while (1){ // Loop will break the first time we copy a zero into entry "e".
		if (e < ENTRYNUM - 1){ // Normal case (e < 63), just copy the successive entry into it.
			rootCluster->rootDirEntries[e] = rootCluster->rootDirEntries[e + 1];
			if (0 == rootCluster->rootDirEntries[e]) break;
			e += 1;
		} else { // This is where e == 63. The next element, if it exists, is in the next root cluster.
			if (0 == rootCluster->nextRootCluster){ // If there is no next cluster, entry "e" becomes empty.
				rootCluster->rootDirEntries[e] = 0;
				break;
			} else { // If there is a next cluster...
				RootCluster* tempRootCluster = new RootCluster();
				partitionMap[i].partition->readCluster(rootCluster->nextRootCluster, (char*) tempRootCluster); // ... we load it into a temporary pointer...
				rootCluster->rootDirEntries[e] = tempRootCluster->rootDirEntries[0]; // ... copy its zero entry into "e"...
				e = 0; // ... reset "e" to zero...
				partitionMap[i].partition->writeCluster(rootClusterNo, (char*) rootCluster); // ... save the old cluster...
				rootClusterNo = tempRootCluster->nextRootCluster; // ... update the root cluster number...
				delete rootCluster;
				rootCluster = tempRootCluster; // ... and replace the old cluster with the new one.
			}
		}
	}

	if ((0 != e) || (0 == rootClusterNo)){ // Normal situation: we ended up somewhere in the middle of the last root cluster. Also, ignore for zero root cluster.
		partitionMap[i].partition->writeCluster(rootClusterNo, (char*) rootCluster);
	} else { // This means we just emptied the last root cluster and must delete it.
		RootCluster* tempRootCluster = new RootCluster();
		partitionMap[i].partition->readCluster(rootCluster->prevRootCluster, (char*) tempRootCluster); // Load the previous root cluster.
		prevRootCluster->nextRootCluster = 0;
		partitionMap[i].partition->writeCluster(rootCluster->prevRootCluster, (char*) tempRootCluster); // This sets the previous root cluster's "next" pointer to zero.

		partitionMap[i].partition->readCluster(0, (char*) tempRootCluster); // Reading the zero root cluster.
		partitionMap[i].partition->writeCluster(rootClusterNo, (char*) tempRootCluster->prevRootCluster); // Chain the newly empty cluster to the list of free clusters.
		tempRootCluster->prevRootCluster = rootClusterNo; // Chain the zero root cluster to the newly empty one.
		partitionMap[i].partition->writeCluster(0, (char*) tempRootCluster); // Save the zero root cluster.
		delete tempRootCluster;
	}

	delete rootCluster;
	delete indexCluster;
	signal(fsMutex);
	return 1;
}

KernelFS::KernelFS(){
	partitionMap = new PartitionMapEntry[26];
	for (int i = 0; i < 26; i++){
		partitionMap[i] = new PartitionMapEntry(i);
	}

	fsMutex = CreateSemaphore(NULL,1,1,NULL);

	bankersTable = BankersTable::getInstance();
}

FileLocation KernelFS::findFile(char* fname){
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
	ClusterNo clusterNo = 0;
	if ((partitionMap[i].partition == 0) ||
		(0 == partitionMap[i].partition->getNumOfClusters)){
			delete tempCluster;
			return 0; // If partition is not mounted or size 0, return 0.
	}

	partitionMap[i].partition->readCluster(clusterNo, (char*) tempCluster); // Load zero root cluster.
	while (1){	
		EntryNum entry = 0;
		for (; entry < ENTRYCNT; entry++){ // Go through all the entries. If we come across an empty one, there are no more entries and we can return 0.
			Entry tempEntry = tempCluster->rootDirEntries[entry];
			if (tempEntry.name == 0){
				delete tempCluster;
				return 0;
			}
			if ((0 == strcmp(tempEntry.name, relFName)) && // We check the filename...
				(0 == strcmp(tempEntry.ext, relFExt))){ // ... As well as the extension...
					delete tempCluster;
					return FileLocation { tempEntry, clusterNo, entry }; // ... And if they are both equal, return 1.
			}
		}
		if (tempCluster->nextRootCluster == 0){
			delete tempCluster;
			return 0; // If there is no next cluster, return 0...
		}
		clusterNo = tempCluster->nextRootCluster;
		partitionMap[i].partition->readCluster(clusterNo, (char*) tempCluster); // ... And if there is, go to the next one and start over.
	}
}

FileLocation KernelFS::createFile(char* fname){
	char part = fname[0];
	int i = part - 'A';
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

	Entry newEntry = Entry { relFName, relFExt, 0, 0, 0 };
	EntryNum entryNum;
	ClusterNo clusterNo = 0;

	RootCluster* tempCluster = new RootCluster();
	if ((partitionMap[i].partition == 0) ||
		(0 == partitionMap[i].partition->getNumOfClusters)){
			delete tempCluster;
			return 0; // If partition is not mounted or size 0, return 0.
	}

	partitionMap[i].partition->readCluster(clusterNo, (char*) tempCluster); // Load zero root cluster.
	while (1){
		for (entryNum = 0; entryNum < ENTRYCNT; entryNum++){ // Go through all the entries. If we come across an empty one, there are no more entries and we can break.
			if (tempCluster->rootDirEntries[entryNum].name == 0){
				break;
			}
		}
		if (tempCluster->nextRootCluster == 0){
			break; // If there is no next cluster, break...
		}
		clusterNo = tempCluster->nextRootCluster;
		partitionMap[i].partition->readCluster(clusterNo, (char*) tempCluster); // ... And if there is, go to the next one and start over.
	}

	RootCluster* zeroRootCluster, freeCluster;
	IndexCluster* newIndexCluster;
	zeroRootCluster = new RootCluster();
	freeCluster = new RootCluster();
	newIndexCluster = new IndexCluster();
	partitionMap[i].partition->readCluster(0, (char*) zeroRootCluster); // Read the zero root cluster.

	if (entryNum == ENTRYCNT){
		ClusterNo freeClusterNo = zeroRootCluster->prevRootCluster; // Get the first free cluster number.
		partitionMap[i].partition->readCluster(freeClusterNo, (char*) freeCluster); // Read the first free cluster.

		zeroRootCluster->prevRootCluster = (ClusterNo) (*freeCluster);

		tempCluster->nextRootCluster = freeClusterNo;
		partitionMap[i].partition->writeCluster(clusterNo, (char*) tempCluster); // Modify the temp cluster to point at the new free one, and save it.

		delete tempCluster;
		tempCluster = new RootCluster(0, clusterNo); // Clean and format the new cluster, and point "prevRootCluster" to the previous one.
		clusterNo = freeClusterNo;
		entryNum = 0; // And after this, we can call the new free cluster the temp cluster, with the next entry number pointing at the first slot.
	}

	// Now we'll point the new entry to the first free cluster, and point the zero root cluster to the next free one.
	newEntry.firstIndexCluster = zeroRootCluster->prevRootCluster;
	partitionMap[i].partition->readCluster(newEntry.firstIndexCluster, (char*) freeCluster);
	partitionMap[i].partition->writeCluster(newEntry.firstIndexCluster, (char*) newIndexCluster);

	zeroRootCluster->prevRootCluster = (ClusterNo) (*freeCluster);
	partitionMap[i].partition->writeCluster(0, (char*) zeroRootCluster);

	tempCluster->rootDirEntries[entryNum] = newEntry;
	partitionMap[i].partition->writeCluster(clusterNo, (char*) tempCluster); // Add the new entry to the cluster, and save it.

	delete tempCluster;
	delete zeroRootCluster;
	delete freeCluster;
	delete newIndexCluster;

	return FileLocation { newEntry, clusterNo, entryNum };
}