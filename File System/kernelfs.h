#pragma once

#include "fs.h"
#include "thread.h"
#include <map>

struct PartitionMapEntry{
	char name;
	Partition* partition;
	bool accessible;
	HANDLE idle;
	PartitionMapEntry(int partitionNumber){
		name = 'A' + partitionNumber
		partition = 0;
		accessible = false;
		idle = CreateSemaphore(NULL,1,1,NULL);
	}
}

struct RootCluster{
	ClusterNo nextRootCluster;
	ClusterNo prevRootCluster; // In the case of ClusterZero, this is firstFreeCluster
	Directory rootDirEntries;

	RootCluster(){
	}

	RootCluster(ClusterNo next, ClusterNo prev){
		nextRootCluster = next;
		prevRootCluster = prev;
		for (EntryNum i = 0; i < ENTRYCNT; i++)
			rootDirEntries[i].name = 0;
	}
}

const unsigned long INDEXCNT = 511;
typedef ClusterNo FileIndex[INDEXCNT];

struct IndexCluster{
	ClusterNo nextIndexCluster;
	FileIndex fileIndex;

	IndexCluster(ClusterNo next = 0){
		nextIndexCluster = next;
		for (EntryNum i = 0; i < INDEXCNT; i++)
			fileIndex[i] = 0;
	}
}

typedef int ThreadID;

struct ThreadDescriptor{
	ThreadID myID;
	map<File*, int> openedMap;
	map<File*, int> declaredMap;

	ThreadDescriptor(ThreadID id){
		myID = id;
		openedMap = new map<File*, int>();
		declaredMap = new map<File*, int>();
	}
}

class KernelFS {
public:
	~KernelFS();

	char mount(Partition* partition);

	char unmount(char part);

	char format(char part);

	char readRootDir(char part, EntryNum n, Directory &d);

	char doesExist(char* fname);

	char declare(char* fname, int mode);

	File* open(char* fname);

	char deleteFile(char* fname);

	KernelFS();

protected:
	File* findFile(char* fname);

	PartitionMapEntry* partitionMap;

	HANDLE fsMutex;

	map<ThreadID, ThreadDescriptor*> threadMap;

	map<char*, File*> fileMap;
};