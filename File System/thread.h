#pragma once

#include <cstdlib>
#include <windows.h>

#define signal(x) ReleaseSemaphore(x,1,NULL) // Returns 0 if it fails.
#define wait(x) WaitForSingleObject(x,INFINITE) // Returns 0 if wakeup was caused by signal.