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
	BankersTable::getInstance().close(fileInfo.openedBy, fileInfo.fname);
	signal(fileInfo.fileMutex);
}

static File* KernelFile::infoToFile(FileInfo& fileInfo){
	File* newFile = new File();
	KernelFile* newKernelFile = new KernelFile(fileInfo);
	newFile->myImpl = newKernelFile;
	return newFile;
}

KernelFile::KernelFile (FileInfo& fileInfo){
	this->fileInfo = fileInfo;
	cursor = 0;
}