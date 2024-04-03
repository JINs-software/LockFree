#include "LockFreeStack.h"
#include <iostream>
#include <vector>
#include <thread>
#include <conio.h>
using namespace std;

#define NUM_OF_WORKER_THREAD 3
#define NUM_OF_LOOP_PER_THREAD 10000000
#define NUM_OF_LOOP_PER_PUSHPOP 65535

#if defined(LOCK_FREE_MEM_POOL)
LockFreeStack<int> g_LFS(0);
#else
LockFreeStack<int> g_LFS;
#endif
HANDLE workerThreads[NUM_OF_WORKER_THREAD];

unsigned __stdcall  WorkerThreadJob(void* arg) {
	for (int i = 0; i < NUM_OF_LOOP_PER_THREAD; i++) {
		for (int j = 0; j < NUM_OF_LOOP_PER_PUSHPOP; j++) {
			g_LFS.Push(j);
		}
		
		
		for (int j = 0; j < NUM_OF_LOOP_PER_PUSHPOP; j++) {
			int out;
			if (!g_LFS.Pop(out)) {
				cout << "g_LFS.Pop Failed...!" << endl;
				__debugbreak();
			}
			//cout << out << endl;
		}


		//int out;
		//g_LFS.Push(0);
		//if (!g_LFS.Pop(out)) {
		//	cout << "g_LFS.Pop Failed...!" << endl;
		//	__debugbreak();
		//}

		//cout << i << " ·çÇÁ " << endl;
	}

	return 0;
}

int main() {

	for (int i = 0; i < NUM_OF_WORKER_THREAD; i++) {
		//workerThreads[i] = thread(WorkerThreadJob);
		workerThreads[i] = (HANDLE)_beginthreadex(NULL, 0, WorkerThreadJob, NULL, 0, NULL);
		if (!workerThreads[i]) {
			DebugBreak();
		}
	}
	
	
	while(true) {
		DWORD ret = WaitForMultipleObjects(NUM_OF_WORKER_THREAD, workerThreads, TRUE, 0);
		if (WAIT_OBJECT_0 <= ret && ret < WAIT_OBJECT_0 + NUM_OF_WORKER_THREAD) {
			break;
		}

		char ctr;
		if (_kbhit()) {
			ctr = _getch();
			if (ctr == 's' || ctr == 'S') {
				g_LFS.PrintLog();
				DebugBreak();
			}
		}
	}

	//for (int i = 0; i < 10; i++) {
	//	g_LFS.Push(i);
	//}
	//
	//for (int i = 0; i < 10; i++) {
	//	int out;
	//	g_LFS.Pop(out);
	//}

	cout << "LFS Size: " << g_LFS.GetSize() << endl;
}