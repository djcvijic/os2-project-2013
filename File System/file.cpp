#include "file.h"

using namespace std;

char File::write (BytesCnt bytesCnt, char* buffer){
	return myImpl->write(bytesCnt, buffer);
}

BytesCnt File::read (BytesCnt bytesCnt, char* buffer){
	return myImpl->read(bytesCnt, buffer);
}

char File::seek (BytesCnt bytesCnt){
	return myImpl->seek(bytesCnt);
}

BytesCnt File::filePos(){
	return myImpl->filePos();
}

char File::eof (){
	return myImpl->eof();
}

BytesCnt File::getFileSize (){
	return myImpl->getFileSize();
}

char File::truncate (){
	return myImpl->truncate();
}

File::~File(){
	delete myImpl;
}

File::File (){
}