#pragma once

#include "types.h"

class KernelFS;
class Partition;
class File;

class FS {
public:
	~FS ();

	static char mount(Partition* partition); //montira particiju
	// vraca dodeljeno slovo
	// ili 0 u slucaju neuspeha

	static char unmount(char part); //demontira particiju oznacenu datim
	// slovom vraca 0 u slucaju neuspeha ili 1 u slucaju uspeha

	static char format(char part); // formatira particiju sa datim slovom;
	// vraca 0 u slucaju neuspeha ili 1 u slucaju uspeha

	static char readRootDir(char part, EntryNum n, Directory &d); 
	//prvim argumentom se zadaje particija, drugim redni broj
	//validnog ulaza od kog se po?inje ?itanje,
	//treci argument je adresa na kojoj se smesta procitani niz ulaza 

	static char doesExist(char* fname); //argument je naziv fajla zadat
	//apsolutnom putanjom

	static char declare(char* fname, int mode); 
	//prvi argument je naziv fajla zadat apsolutnom putanjom,
	//drugi mod: 
	// 1 – najava koriscenja, 0 – odjava koriscenja zadatog fajla 

	static File* open(char* fname);

	static char deleteFile(char* fname);

protected:
	FS ();

	static const KernelFS *myImpl = new KernelFS();
};