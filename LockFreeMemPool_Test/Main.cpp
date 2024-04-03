#include "LockFreeMemPool.h"
#include <process.h>
#include <iostream>

#pragma comment(lib, "LockFreeMemoryPool")

#define NUM_OF_THREADS	8
#define TEST_LOOP		10000
#define NUM_OF_LOOP		100
#define NUM_OF_PTR		100

//LockFreeMemPool g_LFMP(sizeof(int), 0);
LockFreeMemPool g_LFMP(sizeof(int), NUM_OF_THREADS * NUM_OF_PTR);
bool errorFlag = false;

unsigned WINAPI TestJob(void* arg) {
	int* ptrArr[NUM_OF_PTR + 1];

	for (int k = 0; k < NUM_OF_LOOP; k++) {
		for (int incre = 1; incre <= NUM_OF_PTR; incre++) {

			// Alloc
			for (int i = 0; i < incre; i++) {
				ptrArr[i] = (int*)g_LFMP.Alloc();
				int initVal = *ptrArr[i];
				if (initVal != 0xFDFDFDFD) {
					errorFlag = true;
					DebugBreak();
					return 1;
				}
				if (i > 0 && ptrArr[i] == ptrArr[i - 1]) {
					//LogVectorCheck();
					errorFlag = true;
					DebugBreak();
					return 1;
				}
				*ptrArr[i] = i;
			}

			// Free
			for (int i = 0; i < incre; i++) {
				if (*ptrArr[i] != i) {
					errorFlag = true;
					DebugBreak();
					return 1;
				}

				g_LFMP.Free(ptrArr[i]);
			}
		}
	}
	return 0;
}

int main() {
	for (int i = 0; i < TEST_LOOP; i++) {
		InitLog();
		HANDLE hThreads[NUM_OF_THREADS];
		for (int i = 0; i < NUM_OF_THREADS; i++) {
			hThreads[i] = (HANDLE)_beginthreadex(NULL, 0, TestJob, NULL, 0, NULL);
		}

		WaitForMultipleObjects(NUM_OF_THREADS, hThreads, TRUE, INFINITE);

		if (errorFlag) {
			LogVectorCheck();
		}

		for (int i = 0; i < NUM_OF_THREADS; i++) {
			CloseHandle(hThreads[i]);
		}

		std::cout << "Test..." << std::endl;
	}
	return 0;
}