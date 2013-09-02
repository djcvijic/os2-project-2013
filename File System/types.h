#pragma once

#include "thread.h"
#include "part.h"
#include <map>

using namespace std;

typedef unsigned long BytesCnt; 
typedef unsigned long EntryNum;
typedef unsigned long ThreadID;

const unsigned long ENTRYCNT=64; 
const unsigned int FNAMELEN=8;
const unsigned int FEXTLEN=3;

struct Entry {
	char name[FNAMELEN];
	char ext[FEXTLEN];
	char reserved;
	unsigned long firstIndexCluster; 
	unsigned long size;
};

typedef Entry Directory[ENTRYCNT];

struct PartitionMapEntry{
	char name;
	Partition* partition;
	bool accessible;
	HANDLE idle;
	PartitionMapEntry(int partitionNumber){
		name = 'A' + partitionNumber;
		partition = 0;
		accessible = false;
		idle = CreateSemaphore(NULL,1,1,NULL);
	}
};

struct RootCluster{
	ClusterNo nextRootCluster;
	ClusterNo prevRootCluster; // In the case of ClusterZero, this is firstFreeCluster
	Directory rootDirEntries;
	char reserved[760];

	RootCluster(){
	}

	RootCluster(ClusterNo next, ClusterNo prev){
		nextRootCluster = next;
		prevRootCluster = prev;
		for (EntryNum i = 0; i < ENTRYCNT; i++)
			rootDirEntries[i].name = {0, 0, 0, 0, 0, 0, 0, 0};
	}
};

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
};

struct FileLocation{
	Entry entry;
	ClusterNo clusterNo;
	EntryNum entryNum;
};

struct StringComparer{
    bool operator()(const char* x, const char* y)
    {
         return strcmp(x,y) < 0;
    }
};

struct FileInfo{
	char* fname;
	HANDLE fileMutex;
	ThreadID openedBy;
	Entry entry;

	FileInfo(char* fname){
		this->fname = fname;
		fileMutex = CreateSemaphore(NULL,1,1,NULL);
		openedBy = -1;
	}
};

struct ThreadInfo{
	ThreadID tid;
	map<char*, FileInfo, StringComparer> declaredFiles;
	map<char*, FileInfo, StringComparer> openedFiles;

	ThreadInfo(ThreadID tid){
		this->tid = tid;
	}
};