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
					// ��-���� �޸� Ǯ�� Free�� �Ҵ� ������ 0xFDFD..�� �ʱ�ȭ��.
					// �Ǵ� new �����ڷ� ���� �Ҵ��� �ʿ��� �ÿ��� 0xFDFD..�� �ʱ�ȭ�� �� �Ҵ���
					// ���� Alloc�� ȣ���� �ʿ��� �Ҵ���� ������ 0xFDFD..�� �ʱ�ȭ�� ���¿��� ��.
					errorFlag = true;
					DebugBreak();
					return 1;
				}
				if (i > 0 && ptrArr[i] == ptrArr[i - 1]) {
					// ������ �Ҵ���� �ּҰ� �ٽ� �Ҵ�Ǿ����� üũ��.
					errorFlag = true;
					DebugBreak();
					return 1;
				}

				// �Ҵ���� ������ Ư�� �ĺ� ���� ����/
				*ptrArr[i] = i;
			}

			// Free
			for (int i = 0; i < incre; i++) {
				// ��ȯ �� �Ҵ���� ������ ����� �ĺ� ���� �ƴ� ��� �ٸ� �����忡�� ������Ų ��
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