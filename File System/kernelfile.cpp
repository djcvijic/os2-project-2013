#include "kernelfile.h"

#pragma once

char KernelFile::write (BytesCnt bytesCnt, char* buffer){
	return 0;
}

BytesCnt KernelFile::read (BytesCnt bytesCnt, char* buffer){
	return 0;
}

char KernelFile::seek (BytesCnt bytesCnt){
	return 0;
}

BytesCnt KernelFile::filePos(){
	return 0;
}

char KernelFile::eof (){
	return 0;
}

BytesCnt KernelFile::getFileSize (){
	return 0;
}

char KernelFile::truncate (){
	return 0;
}

KernelFile::~KernelFile(){
}

static File* KernelFile::infoToFile(FileInfo fileInfo){
	return 0;
}

KernelFile::KernelFile (){
}