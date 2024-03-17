#include "LockFreeStack.h"
#include <iostream>
#include <vector>
#include <thread>
using namespace std;

#define NUM_OF_WORKER_THREAD 8
#define NUM_OF_PUSH_PER_THREAD 10000

LockFreeStack<int> g_LFS;
vector<thread> workerThreads(NUM_OF_WORKER_THREAD);

void WorkerThreadJob() {
	for (int i = 0; i < NUM_OF_PUSH_PER_THREAD; i++) {
		g_LFS.Push(i);

		int out;
		if (!g_LFS.Pop(out)) {
			__debugbreak();
		}
	}
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