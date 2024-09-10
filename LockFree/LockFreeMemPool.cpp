#include "LockFreeMemPool.h"
#include <winnt.h>
#include <iomanip>

/////////////////////////////////////////////////////////////////////////////////////
// [생성자] LockFreeMemPool::LockFreeMemPool(size_t unitSize, size_t unitCnt)
/////////////////////////////////////////////////////////////////////////////////////
#if defined(SIMPLE_MEM_POOL)
LockFreeMemPool::LockFreeMemPool(size_t unitSize, size_t unitCnt)
	: m_UnitSize(unitSize), m_UnitCnt(unitCnt), m_FreeFront(NULL), m_Increment(0) {
	if (unitCnt != 0) { m_FreeFront = initChunk(m_UnitSize, m_UnitCnt); }
}
#else
LockFreeMemPool::LockFreeMemPool(size_t unitSize, size_t unitCnt)
	: m_UnitSize(unitSize), m_UnitCnt(unitCnt), m_FreeFront(NULL), m_ResizeLock(0), m_Increment(0) {
	if (unitCnt != 0) { 
		PBYTE chunk = initChunk(m_UnitSize, m_UnitCnt);
		m_ChunkList.push_back(chunk);
		m_FreeFront = chunk;
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// [소멸자] LockFreeMemPool::~LockFreeMemPool()
///////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(SIMPLE_MEM_POOL)
LockFreeMemPool::~LockFreeMemPool() {
	// Relies on heap cleanup at application termination
}
#else
LockFreeMemPool::~LockFreeMemPool() {
	for (auto chunk : m_ChunkList) {
		free(chunk);
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////
// [할당] void* LockFreeMemPool::Alloc()
///////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(SIMPLE_MEM_POOL)
void* LockFreeMemPool::Alloc()
{
	void* ret = NULL;
	DWORD threadID = GetThreadId(GetCurrentThread());

	PBYTE freeFront;
	PBYTE nextFreeFront;
	do {
		freeFront = m_FreeFront;
		nextFreeFront = (PBYTE)((UINT_PTR)freeFront & mask) + m_UnitSize;
	} while (freeFront && freeFront != (ret = InterlockedCompareExchangePointer((PVOID*)&m_FreeFront, (PVOID)(*(PUINT_PTR)nextFreeFront), freeFront)));

	if(freeFront == NULL) {
		
		ret = malloc(m_UnitSize + sizeof(UINT_PTR));
#if defined(ASSERT)
		if (ret == NULL) DebugBreak();
#endif
		memset(ret, 0xFDFDFDFD, m_UnitSize + sizeof(UINT_PTR));
	}
	else {
		ret = (void*)((UINT_PTR)ret & mask);
	}

	return ret;
}
#elif defined(ABOID_ABA_PROBLEM_IMPROVED)
void* LockFreeMemPool::Alloc()
{
	void* ret = NULL;

	UINT_PTR increment = InterlockedIncrement16(&m_Increment);
	increment <<= (64 - 16);

	PBYTE freeFront;
	PBYTE nextFreeFront;
	do {
		freeFront = m_FreeFront;
		nextFreeFront = (PBYTE)((UINT_PTR)freeFront & mask) + m_UnitSize;
	} while (((UINT_PTR)freeFront & mask) && freeFront != (ret = InterlockedCompareExchangePointer((PVOID*)&m_FreeFront, (PVOID)((*(PUINT_PTR)nextFreeFront) ^ increment), freeFront)));

	if (((UINT_PTR)freeFront & mask) == NULL) {
		ret = malloc(m_UnitSize + sizeof(UINT_PTR));
#if defined(ASSERT)
		if (ret == NULL) DebugBreak();
#endif
#if defined(_DEBUG)
		memset(ret, 0xFDFDFDFD, m_UnitSize + sizeof(UINT_PTR));
#endif
	}
	else { ret = (void*)((UINT_PTR)ret & mask);  }

	return ret;
}
#else
void* LockFreeMemPool::Alloc()
{
	void* ret = NULL;

	while (true) {
		PBYTE freeFront;
		do {
			freeFront = m_FreeFront;
		} while (freeFront && freeFront != (ret = InterlockedCompareExchangePointer((PVOID*)&m_FreeFront, (PBYTE)(*(PUINT_PTR)(freeFront + m_UnitSize)), freeFront)));

		if (ret == NULL) {
			// Insufficient available space for further allocation
			if (InterlockedCompareExchange(&m_ResizeLock, 1, 0) == 0) {
				if (m_FreeFront == NULL)
				{

					PBYTE newChunk = NULL;//initChunk(m_UnitSize, m_UnitSize);
					if (m_UnitCnt == 0) {
						newChunk = initChunk(m_UnitSize, 1);
#if defined(ASSERT)
						if (newChunk == NULL) DebugBreak();
#endif
						m_UnitCnt = 1;
					}
					else {
						newChunk = initChunk(m_UnitSize, m_UnitCnt);
#if defined(ASSERT)
						if (newChunk == NULL) __debugbreak();
#endif
						m_UnitCnt *= 2;
					}

					m_ChunkList.push_back(newChunk);

					// 멀티-스레드에서 동기화 문제가 발생할 것으로 예상된다.
					// m_FreeFront == NULL 임을 확인하고 새로운 청크를 할당받아 m_FreeFront가 청크를 가리키도록 하기 전
					// 다른 스레드에서 Free를 처리하는 과정에서 m_FreeFront가 반환된 메모리를 가리킬 수 있다.
					// 이 때 아래 코드가 수행되면, 반환된 메모리는 누락된다. 
					m_FreeFront = newChunk;

					// 해결 방법은? 
					// Resize 작업 동안 Free를 막는다.  m_ResizeLock 락 변수를 pop에도 적용시켜 본다.
				}
				m_ResizeLock = 0;
			}
		}
		else {
			// Sleep(0) 또는 SwitchToThread 함수를 호출하여 한 타임 슬라이싱을 포기
			Sleep(0);
			break;
		}
	}

	return ret;
}
#endif


///////////////////////////////////////////////////////////////////////////////////////////////////////
// [반환] void LockFreeMemPool::Free(void* address)
///////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(SIMPLE_MEM_POOL)
void LockFreeMemPool::Free(void* address)
{
	DWORD threadID = GetThreadId(GetCurrentThread());
	if (address != NULL) {
		UINT_PTR increment = InterlockedIncrement16(&m_Increment);
		increment <<= (64 - 16);

		PBYTE ptr = (PBYTE)address;
		memset(address, 0xFDFDFDFD, m_UnitSize + sizeof(UINT_PTR));
		ptr += m_UnitSize;

		PBYTE freeFront;
		do {
			freeFront = m_FreeFront;
			*(PUINT_PTR)ptr = (UINT_PTR)freeFront;
		} while (freeFront != InterlockedCompareExchangePointer((PVOID*)&m_FreeFront, (PVOID)((UINT_PTR)address ^ increment), freeFront));
	}
}
#elif defined(ABOID_ABA_PROBLEM_IMPROVED)
void LockFreeMemPool::Free(void* address)
{
	if (address != NULL) {
		UINT_PTR increment = InterlockedIncrement16(&m_Increment);
		increment <<= (64 - 16);

		PBYTE ptr = (PBYTE)address;
#if defined(_DEBUG)
		memset(address, 0xFDFDFDFD, m_UnitSize + sizeof(UINT_PTR));
#endif
		ptr += m_UnitSize;
		PBYTE freeFront;
		do {
			freeFront = m_FreeFront;						// 현재 바라보고 있는 freeFront (비트 식별자 포함)
			*(PUINT_PTR)ptr = (UINT_PTR)freeFront & mask;	// 현재 바라보고 있는 freeFront의 next
		} while (freeFront != InterlockedCompareExchangePointer((PVOID*)&m_FreeFront, (PVOID)((UINT_PTR)address ^ increment), freeFront));
	}
}
#else
void LockFreeMemPool::Free(void* address)
{
	if (address != NULL) {
		PBYTE ptr = (PBYTE)address;
		memset(address, 0xFDFDFDFD, m_UnitSize);
		ptr += m_UnitSize;

		PBYTE freeFront;
		do {
			freeFront = m_FreeFront;
			*(PUINT_PTR)ptr = (UINT_PTR)freeFront;
		} while(freeFront != InterlockedCompareExchangePointer((PVOID*)&m_FreeFront, (PVOID)address, freeFront));
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////////////////////////////
// [청크 할당(내부 함수)] 
///////////////////////////////////////////////////////////////////////////////////////////////////////
PBYTE LockFreeMemPool::initChunk(size_t unitSize, size_t unitCnt) {
	PBYTE chunk = (PBYTE)malloc((unitSize + sizeof(UINT_PTR)) * unitCnt);
#if defined(_DEBUG)
	memset(chunk, 0xFDFD'FDFD, (unitSize + sizeof(UINT_PTR)) * unitCnt);
#endif
	PBYTE ptr = chunk;

	for (size_t idx = 0; idx < unitCnt - 1; idx++) {
		ptr += unitSize;
#if defined(ASSERT)
		if (ptr == NULL) DebugBreak();
#endif
		UINT_PTR increment = InterlockedIncrement16(&m_Increment);
		increment <<= (64 - 16);
		*(PUINT_PTR)ptr = (UINT_PTR)(ptr + sizeof(UINT_PTR)) ^ increment;
		ptr += sizeof(UINT_PTR);
	}

	ptr += unitSize;
#if defined(ASSERT)
	if (ptr == NULL) DebugBreak();
#endif
	*(PUINT_PTR)ptr = NULL;

	return chunk;
}


///////////////////////////////////////////////////////
// Logs used for lock-free verification
///////////////////////////////////////////////////////
#if defined(ON_LOG)
std::vector<stLog> g_LogVector;
std::mutex g_LogMtx;
std::mutex g_MallocMtx;
LONG g_SpinLockFlag;
std::mutex g_LogPrintMtx;

struct stHeadLog {
	PBYTE	oldFreeFront;
	PBYTE	newFreeFront;
};

void InitLog() {
	g_LogVector.clear();
}
void LogVectorCheck() {
	std::lock_guard<std::mutex> lockGuard(g_LogPrintMtx);
	std::cout << "LogVectorCheck" << std::endl;
	const UINT_PTR mask = 0x0000'FFFF'FFFF'FFFF;

	std::map<DWORD, std::vector<stHeadLog>> AllocMap;

	for (int i = 0; i < g_LogVector.size(); i++) {
		stLog& log = g_LogVector[i];
		auto oldAddress = reinterpret_cast<UINT_PTR>(log.oldFreeFront);
		auto newAddress = reinterpret_cast<UINT_PTR>(log.newFreeFront);

		std::cout << "-------------------------------" << std::endl;
		std::cout << "threadID: " << log.threadID << std::endl;
		if (log.isAlloc) {
			if (log.isAfterCommit) {
				std::cout << "-> Alloc Commit" << std::endl;
			}
			else {
				std::cout << "-> Alloc Try" << std::endl;
			}

			std::cout << "Alloc address: 0x" << std::setw(16) << std::setfill('0') << std::hex << oldAddress << std::endl;
			std::cout << "Head: 0x" << std::setw(16) << std::setfill('0') << std::hex << newAddress << std::endl;
			//std::cout << "Alloc address: " << log.oldFreeFront << std::endl;
			//std::cout << "New Head: " << std::hex << log.newFreeFront << std::endl;
		}
		else {
			if (log.isAfterCommit) {
				std::cout << "-> Free Commit" << std::endl;
			}
			else {
				std::cout << "-> Free Try" << std::endl;
			}

			std::cout << "Old Head: 0x" << std::setw(16) << std::setfill('0') << std::hex << oldAddress << std::endl;
			std::cout << "Old Head: 0x" << std::setw(16) << std::setfill('0') << std::hex << newAddress << std::endl;
			//std::cout << "Old Head: " << std::hex << log.oldFreeFront << std::endl;
			//std::cout << "Old Head: " << std::hex << log.newFreeFront << std::endl;
		}

		if (!log.isAfterCommit) {
			continue;
		}

		if (AllocMap.find(log.threadID) == AllocMap.end()) {
			AllocMap.insert({ log.threadID, std::vector<stHeadLog>() });
		}

		if (log.isAlloc) {	// Alloc
			// 이미 할당된 주소를 다시 할당한 것인지 확인
			for (auto iter = AllocMap.begin(); iter != AllocMap.end(); iter++) {
				std::vector<stHeadLog>& headLogs = iter->second;
				for (int k = 0; k < headLogs.size(); k++) {
					if (((UINT_PTR)headLogs[k].oldFreeFront & mask) == ((UINT_PTR)log.oldFreeFront & mask)) {
						DebugBreak();

						std::vector<stLog>& copyVec = g_LogVector;
						for (int t = i + 1; t < copyVec.size(); t++) {
							stLog& temp = copyVec[t];
							if (!temp.isAlloc && temp.isAfterCommit) {
								if (((UINT_PTR)log.oldFreeFront & mask) == ((UINT_PTR)headLogs[k].oldFreeFront & mask)) {
									break;
									DebugBreak();
								}
							}
						}

						DebugBreak();
					}
				}
			}

			AllocMap[log.threadID].push_back({ log.oldFreeFront, log.newFreeFront });
		}
		else {				// Free
			// 다른 스레드에서 할당된 주소를 반환한 것인지 확인 or 할당받은 주소를 반환한 것인지 확인
			for (auto iter = AllocMap.begin(); iter != AllocMap.end(); iter++) {
				std::vector<stHeadLog>& headLogs = iter->second;
				for (int k = 0; k < headLogs.size(); k++) {
					if (iter->first == log.threadID) {
						if (((UINT_PTR)headLogs[k].oldFreeFront & mask) == ((UINT_PTR)log.newFreeFront & mask)) {
							headLogs.erase(headLogs.begin() + k);
							continue;
						}
					}
					else {
						if (((UINT_PTR)headLogs[k].oldFreeFront & mask) == ((UINT_PTR)log.newFreeFront & mask)) {
							DebugBreak();
						}
					}
				}
			}
		}
	}
}
#endif