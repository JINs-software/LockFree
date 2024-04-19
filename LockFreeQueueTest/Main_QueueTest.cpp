#include "LockFreeQueue.h"
#include <iostream>
#include <vector>
#include <thread>
#include <map>
#include <conio.h>
using namespace std;

#define NUM_OF_WORKER_THREAD 16
#define NUM_OF_LOOP_PER_THREAD 100000
#define NUM_OF_ITER 3

const DWORD ENQUEUE_TEST_DATA = 0x1234'5678;
std::map<DWORD, vector<int>> DATA_VALIDATE_TEST_VECTOR;

LockFreeQueue<DWORD> g_LFQ;

unsigned WINAPI WorkerThreadJob(void* arg) {
	DWORD thID = GetThreadId(GetCurrentThread());
	for (int i = 0; i < NUM_OF_LOOP_PER_THREAD; i++) {
		//g_LFQ.Enqueue(ENQUEUE_TEST_DATA);
		//
		//DWORD out;
		//if (!g_LFQ.Dequeue(out)) {
		//	// 갯수가 안 맞음
		//	DebugBreak();
		//	g_LFQ.PrintLog();
		//	DebugBreak();
		//}
		//else if (out != ENQUEUE_TEST_DATA) {
		//	// 데이터 손상
		//	DebugBreak();
		//	g_LFQ.PrintLog();
		//	DebugBreak();
		//}

		for (int j = 1; j <= NUM_OF_ITER; j++) {
			// 1, 2, 3, ....100
			for (int k = 1; k <= j; k++) {
				g_LFQ.Enqueue(i);
			}
			for (int k = 1; k <= j; k++) {
				DWORD out;
				if (!g_LFQ.Dequeue(out)) {
#if defined(LOGGING)
					DebugBreak();
					g_LFQ.PrintLog();
#endif
					DebugBreak();
				}

				DATA_VALIDATE_TEST_VECTOR[thID][out]++;
			}
		}
	}

	std::cout << "Worker thread Finish!" << std::endl;
	return 0;
}

int main() {

	size_t testCnt = 0;
	while (true) {
		HANDLE workerThreads[NUM_OF_WORKER_THREAD];
		for (int i = 0; i < NUM_OF_WORKER_THREAD; i++) {
			workerThreads[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThreadJob, NULL, CREATE_SUSPENDED, NULL);
			if (workerThreads[i] == NULL) {
				std::cout << "스레드 생성 실패" << std::endl;
				DebugBreak();
			}
			else {
				DWORD thID = GetThreadId(workerThreads[i]);
				DATA_VALIDATE_TEST_VECTOR.insert({ thID, std::vector<int>(NUM_OF_LOOP_PER_THREAD) });
			}
		}
		for (int i = 0; i < NUM_OF_WORKER_THREAD; i++) {
			ResumeThread(workerThreads[i]);
		}


		while (true) {
			DWORD ret = WaitForMultipleObjects(NUM_OF_WORKER_THREAD, workerThreads, TRUE, 1);
			if (WAIT_OBJECT_0 <= ret && ret <= WAIT_OBJECT_0 + 1) {
				break;
			}

			char ctr;
			if (_kbhit()) {
				ctr = _getch();
				if (ctr == 's' || ctr == 'S') {
#if defined(LOGGING)
					DebugBreak();
					g_LFQ.PrintLog();
#endif
					DebugBreak();
				}
			}
		}

		std::vector<int> DATA_VALIDATE_TEST_SUM(NUM_OF_LOOP_PER_THREAD, 0);
		for (int i = 0; i < NUM_OF_WORKER_THREAD; i++) {
			DWORD thID = GetThreadId(workerThreads[i]);
			for (int k = 0; k < NUM_OF_LOOP_PER_THREAD; k++) {
				DATA_VALIDATE_TEST_SUM[k] += DATA_VALIDATE_TEST_VECTOR[thID][k];
			}
			CloseHandle(workerThreads[i]);
		}

		int cmpNum = NUM_OF_WORKER_THREAD * (((1 + NUM_OF_ITER) / (double)2) * NUM_OF_ITER);
		for (int k = 0; k < NUM_OF_LOOP_PER_THREAD; k++) {
			if (DATA_VALIDATE_TEST_SUM[k] != cmpNum) {
				DebugBreak();
			}
		}

		std::cout << "TestCnt: " << testCnt++ << std::endl;
		std::cout << "cmpNum: " << cmpNum << std::endl;

		DATA_VALIDATE_TEST_VECTOR.clear();
#if defined(LOGGING)
		g_LFQ.ClearLog();
#endif
	}
}