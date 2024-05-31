#pragma once
#include <Windows.h>
#include <cassert>
#include <list>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <map>
#include <winnt.h>
#include <iostream>

//#define ON_LOG

//#define DATA_VALID_TEST
//#define SIMPLE_MEM_POOL
#define THREAD_SAFE
#define ABOID_ABA_PROBLEM_IMPROVED

#if defined(ON_LOG)
struct stLog {
	//LARGE_INTEGER highQualityTime;
	DWORD	threadID;
	BOOL	isAlloc;
	BOOL	isAfterCommit;
	PBYTE	oldFreeFront;
	PBYTE	newFreeFront;
	BOOL	isMalloc;
};
extern std::vector<stLog> g_LogVector;
extern std::mutex g_LogMtx;
extern std::mutex g_MallocMtx;
extern LONG g_SpinLockFlag;

// g_LogVector 로그 추적
void LogVectorCheck();
void InitLog();
#endif

class LockFreeMemPool
{
public:
	LockFreeMemPool(size_t unitSize, size_t unitCnt);
	~LockFreeMemPool();

	void* Alloc();				// stack pop
	void Free(void* address);	// stack push

private:
	PBYTE m_FreeFront;
	size_t m_UnitSize;
	size_t m_UnitCnt;


	// 락-프리 메모리 풀에서도 ABA Avoid 기법이 필요하다!!!
	short m_Increment;
	const unsigned long long mask = 0x0000'FFFF'FFFF'FFFF;

#if !defined(SIMPLE_MEM_POOL) && defined(DATA_VALID_TEST)
	std::pair<PBYTE, PBYTE> initChunk(size_t unitSize, size_t unitCnt);
#else
	PBYTE initChunk(size_t unitSize, size_t unitCnt);
#endif	// defined(DATA_VALID_TEST)

private:
#if !defined(SIMPLE_MEM_POOL)
	size_t m_ResizeLock;
	std::list<PBYTE> m_ChunkList;
#if defined(DATA_VALID_TEST)
	std::list<std::pair<PBYTE, size_t>> m_DuplicatedList;
	std::unordered_map<PBYTE, PBYTE> m_DupAddressMap;
	size_t m_DupLock;
#endif	// defined(DATA_VALID_TEST)
#endif	// !defined(SIMPLE_MEM_POOL)
};

