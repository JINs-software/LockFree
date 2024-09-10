#include "LockFreeMemPool.h"
#include <process.h>
#include <iostream>

#pragma comment(lib, "LockFree")

#define NUM_OF_THREADS	8
#define NUM_OF_LOOP		1000
#define NUM_OF_PTR		100

LockFreeMemPool g_LFMP(sizeof(int), 0);
//LockFreeMemPool g_LFMP(sizeof(int), NUM_OF_THREADS * NUM_OF_PTR);
bool errorFlag = false;

unsigned WINAPI TestJob(void* arg) {
	int* ptrArr[NUM_OF_PTR + 1];

	for (int k = 0; k < NUM_OF_LOOP; k++) {
		for (int incre = 1; incre <= NUM_OF_PTR; incre++) {

			// Alloc
			for (int i = 0; i < incre; i++) {
				ptrArr[i] = (int*)g_LFMP.Alloc();
				if (*ptrArr[i] != 0xFDFDFDFD) {			
					// 락-프리 메모리 풀은 Free시 할당 영역을 0xFDFD..로 초기화함.
					// 또는 new 연산자로 동적 할당이 필요할 시에도 0xFDFD..로 초기화한 후 할당함
					// 따라서 Alloc을 호출한 쪽에서 할당받은 영역은 0xFDFD..로 초기화된 상태여야 함.
					errorFlag = true;
					DebugBreak();
					return 1;
				}
				if (i > 0 && ptrArr[i] == ptrArr[i - 1]) {
					// 직전에 할당받은 주소가 다시 할당되었는지 체크함.
					errorFlag = true;
					DebugBreak();
					return 1;
				}

				// 할당받은 공간에 특정 식별 값을 저장/
				*ptrArr[i] = i;
			}

			// Free
			for (int i = 0; i < incre; i++) {
				// 반환 전 할당받은 공간에 저장된 식별 값이 아닌 경우 다른 스레드에서 오염시킨 것
				if (*ptrArr[i] != i) {
					errorFlag = true;
					DebugBreak();
					return 1;
				}

				g_LFMP.Free(ptrArr[i]);
			}
		}
	}

	std::cout << "Thread Work Finish!" << std::endl;
	return 0;
}

int main() {
	size_t testLoopCnt = 0;
	while (true) {

#if defined(ON_LOG)
		InitLog();
#endif
		HANDLE hThreads[NUM_OF_THREADS];
		for (int i = 0; i < NUM_OF_THREADS; i++) {
			hThreads[i] = (HANDLE)_beginthreadex(NULL, 0, TestJob, NULL, 0, NULL);
		}

		WaitForMultipleObjects(NUM_OF_THREADS, hThreads, TRUE, INFINITE);

		if (errorFlag) {
#if defined(ON_LOG)
			LogVectorCheck();
#endif
		}

		for (int i = 0; i < NUM_OF_THREADS; i++) {
			CloseHandle(hThreads[i]);
		}

		std::cout << "All Thread's works Done !!! / TestLoop: " << testLoopCnt++ << std::endl;
	}
	return 0;
}