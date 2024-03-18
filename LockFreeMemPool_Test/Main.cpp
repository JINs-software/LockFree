#include "LockFreeMemPool.h"

#pragma comment(lib, "LockFreeMemoryPool")

int main() {
	LockFreeMemPool lfmp(sizeof(int), 4);
	int* ptr = (int*)lfmp.Alloc();
	int* ptr1 = (int*)lfmp.Alloc();
	int* ptr2 = (int*)lfmp.Alloc();
	int* ptr3 = (int*)lfmp.Alloc();

	lfmp.Free(ptr2);
	lfmp.Free(ptr3);
	lfmp.Free(ptr);
	lfmp.Free(ptr1);
}