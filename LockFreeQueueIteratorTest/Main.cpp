#include "LockFreeQueue.h"

#include <thread>
#include <conio.h>
using namespace std;

int g_Sum;
int g_SumCmp1;
int g_SumCmp2;

bool DequeueWorkStop;
bool DequeueIterWorkStop;

LockFreeQueue<int> g_LFQ;

void EnqueueFunc() {
	for (int i = 0; i < 100000; i++) {
		g_LFQ.Enqueue(i);
		g_Sum += i;
	}
}
void DequeueFunc() {
	while (!DequeueWorkStop) {
		int val;
		if (g_LFQ.Dequeue(val)) {
			g_SumCmp1 += val;
		}
	}
}
void DequeueIterFunc() {
	while (!DequeueIterWorkStop) {
		LockFreeQueue<int>::iterator iter = g_LFQ.Dequeue();
		while (true) {
			int val;
			if (iter.pop(val)) {
				g_SumCmp2 += val;
			}
			else {
				break;
			}
		}
	}
}

int main() {
	thread th1(EnqueueFunc);
	thread th2(DequeueFunc);
	thread th3(DequeueIterFunc);

	char ctr;
	while (true) {
		if (_kbhit()) {
			ctr = _getch();
			if (ctr == 's' || ctr == 'S') {
				DebugBreak();
				PrintLog();
			}
		}
	}

	th2.join();
	th3.join();
	th1.join();

}