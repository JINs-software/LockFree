#include "LockFreeStack.h"
#include <iostream>
#include <vector>
#include <thread>
using namespace std;

#define NUM_OF_WORKER_THREAD 4
#define NUM_OF_PUSH_PER_THREAD 1000

LockFreeStack<int> g_LFS;
vector<thread> workerThreads(NUM_OF_WORKER_THREAD);

void WorkerThreadJob() {
	for (int i = 0; i < NUM_OF_PUSH_PER_THREAD; i++) {
		g_LFS.Push(i);
	}

	//for (int i = 0; i < NUM_OF_PUSH_PER_THREAD; i++) {
	//	int out;
	//	g_LFS.Pop(out);
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

	int cnt = 0;
	int out;
	while (g_LFS.Pop(out)) { cnt++; }
	cout << cnt << endl;
}