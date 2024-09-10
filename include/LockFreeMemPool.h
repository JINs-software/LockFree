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

//#define SIMPLE_MEM_POOL
#define ABOID_ABA_PROBLEM_IMPROVED

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

	// In a lock-free memory pool, the ABA avoidance technique is still necessary
	short m_Increment;
	const unsigned long long mask = 0x0000'FFFF'FFFF'FFFF;

	PBYTE initChunk(size_t unitSize, size_t unitCnt);

private:
#if !defined(SIMPLE_MEM_POOL)
	size_t m_ResizeLock;
	std::list<PBYTE> m_ChunkList;
#endif
};

///////////////////////////////////////////////////////
// Logs used for lock-free verification
///////////////////////////////////////////////////////
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