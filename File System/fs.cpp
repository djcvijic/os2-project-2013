#include "fs.h"
#include "kernelfs.h"
#include "part.h"

using namespace std;

FS::~FS(){
}

char FS::mount(Partition* partition){
	return myImpl->mount(partition);
}

char FS::unmount(char part){
	return myImpl->unmount(part);
}

char FS::format(char part){
	return myImpl->format(part);
}

char FS::readRootDir(char part, EntryNum n, Directory &d){
	return myImpl->readRootDir(part,n,d);
}

char FS::doesExist(char* fname){
	return myImpl->doesExist(fname);
}

char FS::declare(char* fname, int mode){
	return myImpl->declare(fname,mode);
}

File* FS::open(char* fname){
	return myImpl->open(fname);
}

char FS::deleteFile(char* fname){
	return myImpl->deleteFile(fname);
}

FS::FS(){
}

FS::myImpl = new KernelFS();