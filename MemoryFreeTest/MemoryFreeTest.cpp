#include <Windows.h>
#include <stdlib.h>
#include <iostream>
using namespace std;
//#define NEW_ALLOC

int main() {
	//LPVOID myHeap = HeapAlloc(GetProcessHeap(), 0, 4096);
	////free(myHeap);
	//HeapFree(GetProcessHeap(), 0, myHeap);
	////int* ptr = (int*)myHeap;
	////*ptr = 10;
	//
	//LPVOID lpPage = VirtualAlloc(NULL, 4096, MEM_RESERVE, PAGE_READWRITE);
	//if (lpPage == NULL) {
	//	cout << GetLastError() << endl;
	//}
	//int* ptr = (int*)lpPage;
	//
	//// 쓰기 액세스 위반
	////*ptr = 10;
	//
	//int val = *ptr;

	//HANDLE newHeap = HeapCreate(0, 0, 0);
	//LPVOID newMyHeap = HeapAlloc(newHeap, 0, 4096);
	////HeapFree(GetProcessHeap(), 0, newMyHeap);
	//free(newMyHeap);
	
	//free(newMyHeap);


	//free(myHeap);
	//myHeap = (int*)myHeap + 10;
	//HeapFree(GetProcessHeap(), 0, myHeap);
	//HeapFree(GetProcessHeap(), 0, myHeap);

#if defined(NEW_ALLOC)
	int* a = new int;
	int* b = new int;
	int* c = new int;

	delete b;
	delete b;
#else
	int* a = (int*)malloc(sizeof(int));
	int* b = (int*)malloc(sizeof(int));
	int* c = (int*)malloc(sizeof(int));
	int* d = (int*)malloc(sizeof(int));
	int* e = (int*)malloc(sizeof(int));
	int* f = (int*)malloc(sizeof(int));
	int* g = (int*)malloc(sizeof(int));
	int* h = (int*)malloc(sizeof(int));
	int* h1 = (int*)malloc(sizeof(int));
	int* h2 = (int*)malloc(sizeof(int));
	int* h3 = (int*)malloc(sizeof(int));
	int* h4 = (int*)malloc(sizeof(int));
	int* h5 = (int*)malloc(sizeof(int));
	int* h6 = (int*)malloc(sizeof(int));
	int* h7 = (int*)malloc(sizeof(int));
	int* h8 = (int*)malloc(sizeof(int));
	int* h9 = (int*)malloc(sizeof(int));
	int* h10 = (int*)malloc(sizeof(int));
	int* h11 = (int*)malloc(sizeof(int));
	int* h12 = (int*)malloc(sizeof(int));
	int* h13 = (int*)malloc(sizeof(int));

	free(b);
	free(b);

#endif

	return 0;
}