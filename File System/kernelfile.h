#pragma once

#include "types.h"
#include "kernelfs.h"
#include "fs.h"
#include "bankers.h"
#include "file.h"

class KernelFile {
public:
	char write (BytesCnt, char* buffer);

	BytesCnt read (BytesCnt, char* buffer);

	char seek (BytesCnt);

	BytesCnt filePos();

	char eof ();

	BytesCnt getFileSize ();

	char truncate ();

	~KernelFile();

	static File* infoToFile(FileInfo&);

private:
	FileInfo& fileInfo;

	BytesCnt cursor;

	KernelFile (FileInfo&);
};