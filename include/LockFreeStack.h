#pragma once
#include <Windows.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <stdexcept>
#include <limits>

#include "LockFreeMemPool.h"	
#ifdef _DEBUG
#pragma comment(lib, "../lib/Debug/LockFree.lib")
#else
#pragma comment(lib, "../lib/Release/LockFree.lib")
#endif

//#define AVOID_ABA_PROBLEM
#define AVOID_ABA_PROBLEM_IMPROVED
#define LOCK_FREE_MEM_POOL
#define NUM_OF_DEFAULT_UNITS 0

template <typename T>
class LockFreeStack
{
	struct Node {
		T data;
		Node* next;

		Node(T _data) : data(_data) {}
	};

private:
	Node* m_Head;
#if defined(AVOID_ABA_PROBLEM) || defined(AVOID_ABA_PROBLEM_IMPROVED)
	short m_Increment;
	const BYTE m_ControlBit = 16;
	const unsigned long long mask = 0x0000'FFFF'FFFF'FFFF;
#endif

#if defined(LOCK_FREE_MEM_POOL)
	LockFreeMemPool m_MemPool;
#endif

public:
#if defined(LOCK_FREE_MEM_POOL) && (defined(AVOID_ABA_PROBLEM) || defined(AVOID_ABA_PROBLEM_IMPROVED))
	LockFreeStack(size_t capacity = NUM_OF_DEFAULT_UNITS)
		: m_Head(NULL), m_Increment(0), m_MemPool(sizeof(Node), capacity) 
	{
		///////////////////////// LOG /////////////////////////
		memset(m_LogArr, 0, sizeof(stLog) * USHRT_MAX + 1);
		m_LogIndex = 0;
		///////////////////////////////////////////////////////
	}
#elif defined(LOCK_FREE_MEM_POOL)
	LockFreeStack(size_t capacity = NUM_OF_DEFAULT_UNITS)
		: m_Head(NULL), m_MemPool(sizeof(Node), capacity) {}
#elif defined(AVOID_ABA_PROBLEM)
	LockFreeStack()
		: m_Head(NULL), m_Increment(0) {}
#else
	LockFreeStack()
		: m_Head(NULL) {}
#endif
	
	void Push(const T& data);
	bool Pop(T& data);
	size_t GetSize();


public:
	///////////////////////// LOG ///////////////////////// 
	void PrintLog();
	///////////////////////////////////////////////////////
private:
	///////////////////////// LOG ///////////////////////// 
	struct stLog {
		DWORD threadID = 0;
		bool isPush = 0;
		bool isCommit = 0;

		UINT_PTR ptr0 = 0;
		UINT_PTR ptr1 = 0;
		UINT_PTR ptr2 = 0;
		UINT_PTR ptr3 = 0;
	};
	stLog m_LogArr[USHRT_MAX + 1U];
	unsigned short m_LogIndex;
	std::mutex g_LogPrintMtx;
	///////////////////////////////////////////////////////
};

template<typename T>
inline void LockFreeStack<T>::Push(const T& data)
{
	///////////////////////// LOG ///////////////////////// 
	DWORD thID = GetThreadId(GetCurrentThread());
	///////////////////////////////////////////////////////

	//Node* newNode = new Node(data);
	//
	//do {
	//	newNode->next = m_Head;
	//} while (newNode->next != InterlockedCompareExchangePointer((PVOID*)&m_Head, (PVOID)newNode, (PVOID)newNode->next));
	// => 최적화 시 문제가 발생

#if defined(LOCK_FREE_MEM_POOL)
	Node* newNode = (Node*)m_MemPool.Alloc();
	if (newNode == NULL) {
		DebugBreak();		// 메모리 풀에서 할당에 실패하는 경우는 없다고 가정
		return;
	}
	else {
		if ((UINT_PTR)newNode->next != 0xFDFD'FDFD'FDFD'FDFD) {
			__debugbreak();
		}
	}
	newNode->data = data;
#else
	Node* newNode = new Node(data);
	if (newNode == NULL) {
		__debugbreak();
	}
#endif	

	// BEFORE COMMIT
#if defined(AVOID_ABA_PROBLEM)
	UINT_PTR incrementPart = InterlockedIncrement16(&m_Increment);	// 64 = 16
	incrementPart <<= (64 - m_ControlBit);
	UINT_PTR managedPtr = ((UINT_PTR)newNode ^ incrementPart);

	Node* oldNode;
	do {
		oldNode = m_Head;
		newNode->next = oldNode;
		///////////////////////// LOG ///////////////////////// 
		unsigned short logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
		m_LogArr[logIdx].threadID = thID;
		m_LogArr[logIdx].isPush = true;					// Push
		m_LogArr[logIdx].isCommit = false;              // before push commit
		m_LogArr[logIdx].ptr0 = (UINT_PTR)oldNode;      // 현재 바라보고 있는 head
		m_LogArr[logIdx].ptr1 = (UINT_PTR)managedPtr;   // Push 예정 노드 주소
		///////////////////////////////////////////////////////
	} while (oldNode != InterlockedCompareExchangePointer((PVOID*)&m_Head, (PVOID)managedPtr, (PVOID)oldNode));
#elif defined(AVOID_ABA_PROBLEM_IMPROVED)
	UINT_PTR incrementPart = InterlockedIncrement16(&m_Increment);	// 64 = 16
	incrementPart <<= (64 - m_ControlBit);
	UINT_PTR managedPtr = ((UINT_PTR)newNode ^ incrementPart);

	Node* oldNode;
	do {
		oldNode = m_Head;
		newNode->next = (Node*)((UINT_PTR)oldNode & mask);
		///////////////////////// LOG ///////////////////////// 
		unsigned short logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
		m_LogArr[logIdx].threadID = thID;
		m_LogArr[logIdx].isPush = true;					// Push
		m_LogArr[logIdx].isCommit = false;              // before push commit
		m_LogArr[logIdx].ptr0 = (UINT_PTR)oldNode;      // 현재 바라보고 있는 head
		m_LogArr[logIdx].ptr1 = (UINT_PTR)managedPtr;   // Push 예정 노드 주소
		///////////////////////////////////////////////////////
	} while (oldNode != InterlockedCompareExchangePointer((PVOID*)&m_Head, (PVOID)managedPtr, (PVOID)oldNode));
#else
	Node* oldNode;

	do {
		oldNode = m_Head;
		newNode->next = oldNode;

		///////////////////////// LOG ///////////////////////// 
		unsigned short logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
		m_LogArr[logIdx].threadID = thID;
		m_LogArr[logIdx].isPush = true;					// Push
		m_LogArr[logIdx].isCommit = false;              // before push commit
		m_LogArr[logIdx].ptr0 = (UINT_PTR)oldNode;      // 현재 바라보고 있는 head
		m_LogArr[logIdx].ptr1 = (UINT_PTR)newNode;   // Push 예정 노드 주소
		///////////////////////////////////////////////////////
	} while (oldNode != InterlockedCompareExchangePointer((PVOID*)&m_Head, (PVOID)newNode, (PVOID)oldNode));
#endif

	// AFTER COMMIT
#if defined(AVOID_ABA_PROBLEM)
	///////////////////////// LOG ///////////////////////// 
	unsigned short logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
	m_LogArr[logIdx].threadID = thID;
	m_LogArr[logIdx].isPush = true;					// Push
	m_LogArr[logIdx].isCommit = true;				// after push commit
	m_LogArr[logIdx].ptr0 = (UINT_PTR)oldNode;      // 현재 바라보고 있던 head
	m_LogArr[logIdx].ptr1 = (UINT_PTR)managedPtr;   // Push된 노드 주소
	///////////////////////////////////////////////////////
#elif defined(AVOID_ABA_PROBLEM_IMPROVED)
	///////////////////////// LOG ///////////////////////// 
	unsigned short logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
	m_LogArr[logIdx].threadID = thID;
	m_LogArr[logIdx].isPush = true;					// Push
	m_LogArr[logIdx].isCommit = true;				// after push commit
	m_LogArr[logIdx].ptr0 = (UINT_PTR)oldNode;      // 현재 바라보고 있던 head
	m_LogArr[logIdx].ptr1 = (UINT_PTR)managedPtr;   // Push된 노드 주소
	///////////////////////////////////////////////////////
#else
	///////////////////////// LOG ///////////////////////// 
	unsigned short logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
	m_LogArr[logIdx].threadID = thID;
	m_LogArr[logIdx].isPush = true;					// Push
	m_LogArr[logIdx].isCommit = true;				// after push commit
	m_LogArr[logIdx].ptr0 = (UINT_PTR)oldNode;      // 현재 바라보고 있던 head
	m_LogArr[logIdx].ptr1 = (UINT_PTR)newNode;   // Push된 노드 주소
	///////////////////////////////////////////////////////
#endif
}

template<typename T>
inline bool LockFreeStack<T>::Pop(T& data)
{
	///////////////////////// LOG ///////////////////////// 
	DWORD thID = GetThreadId(GetCurrentThread());
	///////////////////////////////////////////////////////

#if defined(AVOID_ABA_PROBLEM)
	Node* head;
	Node* headOrigin;
	Node* nextNode;
	do {
		head = m_Head;
		headOrigin = (Node*)((UINT_PTR)head & mask);
		// new/delete로 노드를 할당받는 방식이라면, 아래 코드에서 메모리 읽기 액세스 오류가 발생할 수 있다.
		// 만약 다른 스레드에서 head에 해당하는 노드를 삭제한다면, 메모리 액세스 오류가 발생할 수 있기 때문이다. 
		// (페이지 디커밋까지 일어난 상태)

		// 현재 메모리 풀에서 Free로 반환 시 0xFDFDFDFD로 메모리를 초기화시킨다. 
		// 문제가 발생할 수 있는 시나리오는 다음과 같다.
		// (1) threadA가 Pop -> headOrigin 지역 변수 값에 쓰기까지 완료
		// (2) threadB가 
		try {
			nextNode = headOrigin->next;
		}
		catch (std::exception& e) {
			PrintLog();
		}

		///////////////////////// LOG ///////////////////////// 
		unsigned short logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
		m_LogArr[logIdx].threadID = thID;
		m_LogArr[logIdx].isPush = false;				// Pop
		m_LogArr[logIdx].isCommit = false;				// before pop commit
		m_LogArr[logIdx].ptr0 = (UINT_PTR)head;			// 현재 바라보고 있던 head
		m_LogArr[logIdx].ptr1 = (UINT_PTR)nextNode;		// head next
		///////////////////////////////////////////////////////
	} while (head && head != InterlockedCompareExchangePointer((PVOID*)&m_Head, (PVOID)nextNode, (PVOID)head));
#elif defined(AVOID_ABA_PROBLEM_IMPROVED)
	UINT_PTR incrementPart = InterlockedIncrement16(&m_Increment);	// 64 = 16
	incrementPart <<= (64 - m_ControlBit);

	Node* head;
	Node* headOrigin;
	Node* nextNode;
	do {
		head = m_Head;
		headOrigin = (Node*)((UINT_PTR)head & mask);
		nextNode = headOrigin->next;

		///////////////////////// LOG ///////////////////////// 
		unsigned short logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
		m_LogArr[logIdx].threadID = thID;
		m_LogArr[logIdx].isPush = false;									// Pop
		m_LogArr[logIdx].isCommit = false;									// before pop commit
		m_LogArr[logIdx].ptr0 = (UINT_PTR)head;								// 현재 바라보고 있던 head
		m_LogArr[logIdx].ptr1 = ((UINT_PTR)nextNode ^ incrementPart);		// head next
		///////////////////////////////////////////////////////
	} while (headOrigin && head != InterlockedCompareExchangePointer((PVOID*)&m_Head, (PVOID)((UINT_PTR)nextNode ^ incrementPart), (PVOID)head));
#else
	Node* head;
	Node* nextNode;

	do {
		head = m_Head;
		nextNode = head->next;

		///////////////////////// LOG ///////////////////////// 
		unsigned short logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
		m_LogArr[logIdx].threadID = thID;
		m_LogArr[logIdx].isPush = false;				// Pop
		m_LogArr[logIdx].isCommit = false;				// before pop commit
		m_LogArr[logIdx].ptr0 = (UINT_PTR)head;			// 현재 바라보고 있던 head
		m_LogArr[logIdx].ptr1 = (UINT_PTR)nextNode;		// head next
		///////////////////////////////////////////////////////

	} while (head && head != InterlockedCompareExchangePointer((PVOID*)&m_Head, (PVOID)nextNode, (PVOID)head));
#endif
	
	///////////////////////// LOG ///////////////////////// 
	unsigned short logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
	m_LogArr[logIdx].threadID = thID;
	m_LogArr[logIdx].isPush = false;				// Pop
	m_LogArr[logIdx].isCommit = true;				// before pop commit
	m_LogArr[logIdx].ptr0 = (UINT_PTR)head;			// 현재 바라보고 있던 head
	m_LogArr[logIdx].ptr1 = (UINT_PTR)nextNode;		// head next
	///////////////////////////////////////////////////////


#if defined(AVOID_ABA_PROBLEM)
	if (head != NULL) {
		data = headOrigin->data;
#elif defined(AVOID_ABA_PROBLEM_IMPROVED)
	if (headOrigin != NULL) {
		data = headOrigin->data;
#else
	if (head != NULL) {
		data = head->data;
#endif

		// delete 호출 시  Rtlxxxxx 함수에서 발생시키는 디버그 브레이크는 CRT에서 내는 것으로 판단
		// 이 때 디버그 브레이크를 발생시키는 이유는 메모리 읽기 오류가 떴다기 보단, CRT에서 삭제된 포인터를 다시 삭제 요청을 해서 발생하는 것으로 추측
		// 메모리 참조 오류를 CRT에서 참조하여 떴다는 것은 말이 안됌. 이미 이전에 나의 코드에서 메모리 참조 오류가 떴어야 함.

#if defined(AVOID_ABA_PROBLEM) || defined(AVOID_ABA_PROBLEM_IMPROVED)
		Node* deleteNode = headOrigin;
#else
		Node* deleteNode = head;
#endif

#if defined(LOCK_FREE_MEM_POOL)
		m_MemPool.Free(deleteNode);
#else
		delete deleteNode;
#endif
		return true;
	}
	else {
		return false;
	}
}

template<typename T>
inline size_t LockFreeStack<T>::GetSize()
{
	size_t cnt = 0;
#if defined(AVOID_ABA_PROBLEM)
	Node* ptr = (Node*)((UINT_PTR)m_Head & mask);
#else
	Node* ptr = m_Head;
#endif

	while (ptr != NULL) {
		//std::cout << ptr->data << std::endl;
		cnt++;
#if defined(AVOID_ABA_PROBLEM)
		ptr = (Node*)((UINT_PTR)ptr->next & mask);
#else
		ptr = ptr->next;
#endif
	}

	return cnt;
}

template<typename T>
inline void LockFreeStack<T>::PrintLog()
{
	time_t now = time(0);
	struct tm timeinfo;
	char buffer[80];
	localtime_s(&timeinfo, &now);
	strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &timeinfo);
	std::string currentDateTime = std::string(buffer);

	// 파일 경로 생성
	std::string filePath = "./" + currentDateTime + ".txt";

	// 파일 스트림 열기
	std::ofstream outputFile(filePath);

	if (!outputFile) {
		std::cerr << "파일을 열 수 없습니다." << std::endl;
		return;
	}

	outputFile << currentDateTime << std::endl;
	std::lock_guard<std::mutex> lockGuard(g_LogPrintMtx);
	for (size_t idx = 0; idx <= USHRT_MAX; idx++) {
		stLog& log = m_LogArr[idx];
		if (log.threadID == 0) {
			break;
		}

		outputFile << "------------------------------" << std::endl;
		outputFile << idx << std::endl;
		if (log.isPush) {
			if (!log.isCommit) {
				outputFile << "[BEFORE PUSH COMMIT] ";
				outputFile << "Thread: " << std::hex << log.threadID << std::endl;
				outputFile << "now head    :  0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr0 << std::endl;
				outputFile << "will	be head:  0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr1 << std::endl;
			}
			else {
				outputFile << "[AFTER PUSH COMMIT] ";
				outputFile << "Thread: " << std::hex << log.threadID << std::endl;
				outputFile << "before head:   0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr0 << std::endl;
				outputFile << "new head:      0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr1 << std::endl;
			}
		}
		else {
			if (!log.isCommit) {
				outputFile << "[BEFORE POP COMMIT] ";
				outputFile << "Thread: " << std::hex << log.threadID << std::endl;
				outputFile << "now head:      0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr0 << std::endl;
				outputFile << "now head next: 0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr1 << std::endl;
			}
			else {
				outputFile << "[AFTER POP COMMIT] ";
				outputFile << "Thread: " << std::hex << log.threadID << std::endl;
				outputFile << "poped:         0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr0 << std::endl;
				outputFile << "new head:      0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr1 << std::endl;
			}
		}
	}
	// 파일 닫기
	outputFile.close();

	std::cout << "파일이 생성되었습니다: " << filePath << std::endl;
}
