#include "LockFreeStack.h"
#include <iostream>
#include <vector>
#include <thread>
using namespace std;

#define NUM_OF_WORKER_THREAD 1
#define NUM_OF_LOOP_PER_THREAD 100
#define NUM_OF_LOOP_PER_PUSHPOP 10

LockFreeStack<int> g_LFS;
vector<thread> workerThreads(NUM_OF_WORKER_THREAD);

void WorkerThreadJob() {
	for (int i = 0; i < NUM_OF_LOOP_PER_THREAD; i++) {
		for (int j = 0; j < NUM_OF_LOOP_PER_PUSHPOP; j++) {
			g_LFS.Push(i);
		}
		
	
		for (int j = 0; j < NUM_OF_LOOP_PER_PUSHPOP; j++) {
			int out;
			if (!g_LFS.Pop(out)) {
				cout << "g_LFS.Pop Failed...!" << endl;
				__debugbreak();
			}
		}
	}
	
	//while (true) {
	//	g_LFS.Push(0);
	//	int out;
	//	g_LFS.Pop(out);
	//	
	//}
}

int main() {

	for (int i = 0; i < NUM_OF_WORKER_THREAD; i++) {
		workerThreads[i] = thread(WorkerThreadJob);
	}
	

	for (int i = 0; i < NUM_OF_WORKER_THREAD; i++) {
		if (workerThreads[i].joinable()) {
			workerThreads[i].join();
		}
	}

	cout << "LFS Size: " << g_LFS.GetSize() << endl;
}