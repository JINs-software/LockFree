#pragma once
#include <Windows.h>
#include <cassert>
class LockFreeMemPool
{
public:
	LockFreeMemPool(size_t unitSize, size_t unitCnt);
	~LockFreeMemPool();

	void* Alloc();
	void Free(void* address);

private:
	// (UINT_PTR)
	// #if defined(_WIN64)
	//	typedef unsigned __int64 UINT_PTR;
	// #else
	//	typedef unsigned int UINT_PTR;
	// #endif
	//UINT_PTR m_ListFront;	
	//UINT_PTR m_FreeFront;

	// (PBYTE)
	// typedef BYTE *PBYTE;
	PBYTE m_ListFront;
	PBYTE m_FreeFront;

	size_t m_UnitSize;
	size_t m_UnitCnt;
};

