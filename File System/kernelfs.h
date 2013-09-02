#pragma once

#include "types.h"
#include "part.h"
#include "fs.h"
#include "thread.h"
#include "bankers.h"

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
	FileLocation findFile(char* fname);

	Entry createFile(char* fname);

	PartitionMapEntry* partitionMap;

	HANDLE fsMutex;

	BankersTable bankersTable;
};