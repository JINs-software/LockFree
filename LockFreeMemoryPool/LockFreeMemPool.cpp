#include "LockFreeMemPool.h"
#include <winnt.h>
#include <iomanip>

//#define BEFORE_COMMIT_LOG
//#define AFTER_COMMIT_LOG

std::vector<stLog> g_LogVector;
std::mutex g_LogMtx;
std::mutex g_MallocMtx;
LONG g_SpinLockFlag;
std::mutex g_LogPrintMtx;

//struct stLog {
//	DWORD	threadID;
//	BOOL	isAlloc;
//	PBYTE	oldFreeFront;
//	PBYTE	newFreeFront;
//};

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

///////////////////////////////////////////////////////////////////////////////////////////////////////
// [생성자] LockFreeMemPool::LockFreeMemPool(size_t unitSize, size_t unitCnt)
///////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(SIMPLE_MEM_POOL)
LockFreeMemPool::LockFreeMemPool(size_t unitSize, size_t unitCnt)
	: m_UnitSize(unitSize), m_UnitCnt(unitCnt), m_FreeFront(NULL), m_Increment(0)
{

	////////////////// LOG ////////////////////
	//g_LogVector.reserve(10000);
	///////////////////////////////////////////

	if (unitCnt != 0) {
		m_FreeFront = initChunk(m_UnitSize, m_UnitCnt);
	}
}
#elif defined(DATA_VALID_TEST)
LockFreeMemPool::LockFreeMemPool(size_t unitSize, size_t unitCnt)
	: m_UnitSize(unitSize), m_UnitCnt(unitCnt), m_FreeFront(NULL), m_ResizeLock(0)
{
	if (unitCnt != 0) {
		std::pair<PBYTE, PBYTE> chunk = initChunk(m_UnitSize, m_UnitCnt);
		m_ChunkList.push_back(chunk.first);
		m_DuplicatedList.push_back({ chunk.second, (m_UnitSize + sizeof(UINT_PTR)) * m_UnitCnt });
		m_FreeFront = chunk.first;
		m_DupLock = 0;
	}
}
#else
LockFreeMemPool::LockFreeMemPool(size_t unitSize, size_t unitCnt)
	: m_UnitSize(unitSize), m_UnitCnt(unitCnt), m_FreeFront(NULL), m_ResizeLock(0)
{
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
	// nothing to do
}
#elif defined(DATA_VALID_TEST)
LockFreeMemPool::~LockFreeMemPool() {
	//free(m_ListFront);
	for (auto chunk : m_ChunkList) {
		std::pair<PBYTE, size_t> duplicated = *(m_DuplicatedList.begin());
		m_DuplicatedList.pop_front();

		if (memcmp(chunk, duplicated.first, duplicated.second) != 0) {
			__debugbreak();
		}

		free(duplicated.first);
		free(chunk);
	}
}
#else
LockFreeMemPool::~LockFreeMemPool() {
	//free(m_ListFront);
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

#if defined(THREAD_SAFE)
	PBYTE freeFront;
	PBYTE nextFreeFront;

#if defined(BEFORE_COMMIT_LOG)
	//////////////////////////// LOG ////////////////////////////
	while (0 != InterlockedCompareExchange(&g_SpinLockFlag, 1, 0)) {}
	g_LogVector.push_back({ threadID, TRUE, FALSE, NULL, NULL });
	g_SpinLockFlag = 0;
	/////////////////////////////////////////////////////////////
#endif

	do {
		freeFront = m_FreeFront;
		nextFreeFront = (PBYTE)((UINT_PTR)freeFront & mask) + m_UnitSize;
	} while (freeFront && freeFront != (ret = InterlockedCompareExchangePointer((PVOID*)&m_FreeFront, (PVOID)(*(PUINT_PTR)nextFreeFront), freeFront)));

#else
	if (m_FreeFront != NULL) {
		ret = m_FreeFront;
		m_FreeFront = (PBYTE)(*(PUINT_PTR)(m_FreeFront + m_UnitSize));
	}
#endif

	bool ismalloc = false;
	//if (ret == NULL) {
	// 다음과 같은 상황이 발생할 수 있다.
	// (1) threadA : freeFront == 0xA000으로 while 조건문 진입, 첫 번째 조건(freeFront ?= NULL) 통과
	// (2) threadB : freeFront == 0xA000으로 while 조건문 진입, 첫 번째 조건 그리고 InterlockedCompareExchangePointer 수행
	//															m_FreeFront	= NULL(0xA000 블록만 남은 상황) 변경 후 두 번째 조건 통과
	// (3) threadA : InterlockedCompareExchangePointer 수행, 그러나 m_FreeFront가 변경되지는 않았음. 다만 반환 시 ret = 0xA000 대입
	// (4) threadA : 다음 루프에서 freeFront가 NULL이기에 루프에서 빠져나옴.
	//				=> 문제는 여기에서 ret != NULL이기에 동일한 주소가 Alloc 반환됨!!
	if(freeFront == NULL) {
		
		{
			// is malloc thread-safe???
			std::lock_guard<std::mutex> lockGuard(g_MallocMtx);
			ret = malloc(m_UnitSize + sizeof(UINT_PTR));
			 
		}
		if (ret == NULL) {
			DebugBreak();
		}
		//ret = malloc(m_UnitSize + sizeof(UINT_PTR));
		memset(ret, 0xFDFDFDFD, m_UnitSize + sizeof(UINT_PTR));
		ismalloc = true;
	}
	else {
		ret = (void*)((UINT_PTR)ret & mask);
	}

#if defined(AFTER_COMMIT_LOG)
	//////////////////////////// LOG ////////////////////////////
	while (0 != InterlockedCompareExchange(&g_SpinLockFlag, 1, 0)) {}
	if (freeFront) {
		g_LogVector.push_back({ threadID, TRUE, TRUE, freeFront, (PBYTE)(*(PUINT_PTR)nextFreeFront), ismalloc });
	}
	else {
		g_LogVector.push_back({ threadID, TRUE, TRUE, (PBYTE)ret, (PBYTE)freeFront, ismalloc });
	}
	//g_SpinLockFlag = 0;
	InterlockedExchange(&g_SpinLockFlag, 0);
	/////////////////////////////////////////////////////////////
#endif

	return ret;
}
#elif defined(ABOID_ABA_PROBLEM_IMPROVED)
void* LockFreeMemPool::Alloc()
{
	void* ret = NULL;

#if defined(THREAD_SAFE)
	UINT_PTR increment = InterlockedIncrement16(&m_Increment);
	increment <<= (64 - 16);

	PBYTE freeFront;
	PBYTE nextFreeFront;
	do {
		freeFront = m_FreeFront;
		nextFreeFront = (PBYTE)((UINT_PTR)freeFront & mask) + m_UnitSize;
	} while (((UINT_PTR)freeFront & mask) && freeFront != (ret = InterlockedCompareExchangePointer((PVOID*)&m_FreeFront, (PVOID)((*(PUINT_PTR)nextFreeFront) ^ increment), freeFront)));

#else
	if (m_FreeFront != NULL) {
		ret = m_FreeFront;
		m_FreeFront = (PBYTE)(*(PUINT_PTR)(m_FreeFront + m_UnitSize));
	}
#endif

	bool ismalloc = false;
	if (((UINT_PTR)freeFront & mask) == NULL) {
		ret = malloc(m_UnitSize + sizeof(UINT_PTR));

		if (ret == NULL) {
			DebugBreak();
		}

		//ret = malloc(m_UnitSize + sizeof(UINT_PTR));
		memset(ret, 0xFDFDFDFD, m_UnitSize + sizeof(UINT_PTR));
		ismalloc = true;
	}
	else {
		ret = (void*)((UINT_PTR)ret & mask);
	}

	return ret;
}
#elif defined(DATA_VALID_TEST)
void* LockFreeMemPool::Alloc()
{
	void* ret = NULL;

	while (true) {
#if defined(THREAD_SAFE)
		PBYTE freeFront;
		do {
			freeFront = m_FreeFront;
		} while (freeFront && freeFront != (ret = InterlockedCompareExchangePointer((PVOID*)&m_FreeFront, (PBYTE)(*(PUINT_PTR)(freeFront + m_UnitSize)), freeFront)));
#else
		if (m_FreeFront != NULL) {
			ret = m_FreeFront;
			m_FreeFront = (PBYTE)(*(PUINT_PTR)(m_FreeFront + m_UnitSize));
		}
#endif


#if defined(THREAD_SAFE)
		if (ret == NULL) {
			// 더 이상 할당할 가용 공간 부족
			if (0 == InterlockedCompareExchange(&m_ResizeLock, 1, 0)) {
				if (m_FreeFront == NULL)
				{
					// The #if, #elif, #else, and #endif directives can nest in the text portions of other #if directives.
					// Each nested #else, #elif, or #endif directive belongs to the closest preceding #if directive.
					std::pair<PBYTE, PBYTE> newChunk;
					size_t allocCnt;
					if (m_UnitCnt == 0) {
						newChunk = initChunk(m_UnitSize, 1);
						if (newChunk.first == NULL || newChunk.second == NULL) __debugbreak();
						allocCnt = 1;
						m_UnitCnt = 1;
					}
					else {
						newChunk = initChunk(m_UnitSize, m_UnitCnt);
						if (newChunk.first == NULL || newChunk.second == NULL) __debugbreak();
						allocCnt = m_UnitCnt;
						m_UnitCnt *= 2;
					}

					m_ChunkList.push_back(newChunk.first);
					m_FreeFront = newChunk.first;
					m_DuplicatedList.push_back({ newChunk.second, (m_UnitSize + sizeof(UINT_PTR)) * allocCnt });
				}

				m_ResizeLock = 0;
			}
		}
		else {
			// 여기서 Sleep(0) 또는 SwitchToThread 함수를 호출하여 한 타임 슬라이싱을 포기하는 것은 어떨까??
			// Sleep(0)을 중복하여 여러 번 호출하는 것의 의미는??
			// 의미가 있다면 몇 번을 호출하는 것이 좋을까??
			Sleep(0);
			break;
		}
#else
		if (ret == NULL) {
			PBYTE newChunk = NULL;//initChunk(m_UnitSize, m_UnitSize);
			if (m_UnitCnt == 0) {
				newChunk = initChunk(m_UnitSize, 1);
				if (newChunk == NULL) __debugbreak();
				m_UnitCnt = 1;
			}
			else {
				newChunk = initChunk(m_UnitSize, m_UnitCnt);
				if (newChunk == NULL) __debugbreak();
				m_UnitCnt *= 2;
			}

			m_ChunkList.push_back(newChunk);
			m_FreeFront = newChunk;
		}
		else {
			break;
		}
#endif
	}

	return ret;
}
#else
void* LockFreeMemPool::Alloc()
{
	void* ret = NULL;

	while (true) {
#if defined(THREAD_SAFE)
		PBYTE freeFront;
		do {
			freeFront = m_FreeFront;
		} while (freeFront && freeFront != (ret = InterlockedCompareExchangePointer((PVOID*)&m_FreeFront, (PBYTE)(*(PUINT_PTR)(freeFront + m_UnitSize)), freeFront)));
#else
		if (m_FreeFront != NULL) {
			ret = m_FreeFront;
			m_FreeFront = (PBYTE)(*(PUINT_PTR)(m_FreeFront + m_UnitSize));
		}
#endif


#if defined(THREAD_SAFE)
		if (ret == NULL) {
			// 더 이상 할당할 가용 공간 부족
			if (0 == InterlockedCompareExchange(&m_ResizeLock, 1, 0)) {
				if (m_FreeFront == NULL)
				{

					PBYTE newChunk = NULL;//initChunk(m_UnitSize, m_UnitSize);
					if (m_UnitCnt == 0) {
						newChunk = initChunk(m_UnitSize, 1);
						if (newChunk == NULL) __debugbreak();
						m_UnitCnt = 1;
					}
					else {
						newChunk = initChunk(m_UnitSize, m_UnitCnt);
						if (newChunk == NULL) __debugbreak();
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
			// 여기서 Sleep(0) 또는 SwitchToThread 함수를 호출하여 한 타임 슬라이싱을 포기하는 것은 어떨까??
			// Sleep(0)을 중복하여 여러 번 호출하는 것의 의미는??
			// 의미가 있다면 몇 번을 호출하는 것이 좋을까??
			Sleep(0);
			break;
		}
#else
		if (ret == NULL) {
			PBYTE newChunk = NULL;//initChunk(m_UnitSize, m_UnitSize);
			if (m_UnitCnt == 0) {
				newChunk = initChunk(m_UnitSize, 1);
				if (newChunk == NULL) __debugbreak();
				m_UnitCnt = 1;
			}
			else {
				newChunk = initChunk(m_UnitSize, m_UnitCnt);
				if (newChunk == NULL) __debugbreak();
				m_UnitCnt *= 2;
			}
			
			m_ChunkList.push_back(newChunk);
			m_FreeFront = newChunk;
		}
		else {
			break;
		}
#endif
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
#if defined(THREAD_SAFE)
		PBYTE freeFront;

#if defined(BEFORE_COMMIT_LOG)
		//////////////////////// LOG //////////////////////// 
		while (0 != InterlockedCompareExchange(&g_SpinLockFlag, 1, 0)) {}
		g_LogVector.push_back({ threadID, FALSE, FALSE, NULL, (PBYTE)address });
		g_SpinLockFlag = 0;
		/////////////////////////////////////////////////////
#endif

		do {
			freeFront = m_FreeFront;
			*(PUINT_PTR)ptr = (UINT_PTR)freeFront;
		} while (freeFront != InterlockedCompareExchangePointer((PVOID*)&m_FreeFront, (PVOID)((UINT_PTR)address ^ increment), freeFront));

#if defined(AFTER_COMMIT_LOG)
		//////////////////////// LOG ////////////////////////
		while (0 != InterlockedCompareExchange(&g_SpinLockFlag, 1, 0)) {}
		g_LogVector.push_back({ threadID, FALSE, TRUE, freeFront, (PBYTE)((UINT_PTR)address ^ increment), FALSE });
		//g_SpinLockFlag = 0;
		InterlockedExchange(&g_SpinLockFlag, 0);
		/////////////////////////////////////////////////////
#endif
#else
		* (PUINT_PTR)ptr = (UINT_PTR)m_FreeFront;
		m_FreeFront = (PBYTE)address;
#endif
	}
}
#elif defined(ABOID_ABA_PROBLEM_IMPROVED)
void LockFreeMemPool::Free(void* address)
{
	if (address != NULL) {
		UINT_PTR increment = InterlockedIncrement16(&m_Increment);
		increment <<= (64 - 16);

		PBYTE ptr = (PBYTE)address;
		memset(address, 0xFDFDFDFD, m_UnitSize + sizeof(UINT_PTR));
		ptr += m_UnitSize;
#if defined(THREAD_SAFE)
		PBYTE freeFront;
		do {
			freeFront = m_FreeFront;						// 현재 바라보고 있는 freeFront (비트 식별자 포함)
			*(PUINT_PTR)ptr = (UINT_PTR)freeFront & mask;	// 현재 바라보고 있는 freeFront의 next
		} while (freeFront != InterlockedCompareExchangePointer((PVOID*)&m_FreeFront, (PVOID)((UINT_PTR)address ^ increment), freeFront));

#else
		* (PUINT_PTR)ptr = (UINT_PTR)m_FreeFront;
		m_FreeFront = (PBYTE)address;
#endif
	}
}
#elif defined(DATA_VALID_TEST)
void LockFreeMemPool::Free(void* address)
{
	if (address != NULL) {
		PBYTE ptr = (PBYTE)address;
		memset(address, 0xFDFDFDFD, m_UnitSize);
		ptr += m_UnitSize;
		while (InterlockedCompareExchange(&m_DupLock, 0, 0) != 0) { Sleep(0); }
		PBYTE dupPtr = m_DupAddressMap[(PBYTE)address];
		memset(dupPtr, 0xFDFDFDFD, m_UnitSize);
		dupPtr += m_UnitSize;

#if defined(THREAD_SAFE)
		PBYTE freeFront;
		do {
			freeFront = m_FreeFront;
			*(PUINT_PTR)ptr = (UINT_PTR)freeFront;
			* (PUINT_PTR)dupPtr = (UINT_PTR)freeFront;
		} while (freeFront != InterlockedCompareExchangePointer((PVOID*)&m_FreeFront, (PVOID)address, freeFront));
#else
		* (PUINT_PTR)ptr = (UINT_PTR)m_FreeFront;
		m_FreeFront = (PBYTE)address;
#endif
	}
}
#else
void LockFreeMemPool::Free(void* address)
{
	if (address != NULL) {
		PBYTE ptr = (PBYTE)address;
		memset(address, 0xFDFDFDFD, m_UnitSize);
		ptr += m_UnitSize;


#if defined(THREAD_SAFE)
		PBYTE freeFront;
		do {
			freeFront = m_FreeFront;
			*(PUINT_PTR)ptr = (UINT_PTR)freeFront;
		} while(freeFront != InterlockedCompareExchangePointer((PVOID*)&m_FreeFront, (PVOID)address, freeFront));
#else
		* (PUINT_PTR)ptr = (UINT_PTR)m_FreeFront;
		m_FreeFront = (PBYTE)address;
#endif
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////////////////////////////
// [청크 할당(내부 함수)] 
///////////////////////////////////////////////////////////////////////////////////////////////////////
#if !defined(SIMPLE_MEM_POOL) && defined(DATA_VALID_TEST)
std::pair<PBYTE, PBYTE> LockFreeMemPool::initChunk(size_t unitSize, size_t unitCnt) {
	PBYTE chunk = (PBYTE)malloc((unitSize + sizeof(UINT_PTR)) * unitCnt);
	PBYTE duplicated = (PBYTE)malloc((unitSize + sizeof(UINT_PTR)) * unitCnt);
	memset(chunk, 0xFDFDFDFD, (unitSize + sizeof(UINT_PTR)) * unitCnt);
	memset(duplicated, 0xFDFDFDFD, (unitSize + sizeof(UINT_PTR)) * unitCnt);
	PBYTE ptr = chunk;
	PBYTE dupPtr = duplicated;

	if (InterlockedCompareExchange(&m_DupLock, 1, 0) == 0) {
		m_DupAddressMap.insert({ ptr, dupPtr });
		for (size_t idx = 0; idx < unitCnt - 1; idx++) {
			ptr += unitSize;
			assert(ptr != NULL);
			*(PUINT_PTR)ptr = (UINT_PTR)(ptr + sizeof(UINT_PTR));
			UINT_PTR copyPtr = (UINT_PTR)(ptr + sizeof(UINT_PTR));
			ptr += sizeof(UINT_PTR);

			dupPtr += unitSize;
			assert(dupPtr != NULL);
			*(PUINT_PTR)dupPtr = copyPtr;	// 동일한 포인터로 저장
			dupPtr += sizeof(UINT_PTR);

			m_DupAddressMap.insert({ ptr, dupPtr });
		}
		m_DupLock = 0;
	}

	ptr += unitSize;
	assert(ptr != NULL);
	*(PUINT_PTR)ptr = NULL;
	dupPtr += unitSize;
	assert(dupPtr != NULL);
	*(PUINT_PTR)dupPtr = NULL;

	return { chunk, duplicated };
}
#else
PBYTE LockFreeMemPool::initChunk(size_t unitSize, size_t unitCnt) {
	PBYTE chunk = (PBYTE)malloc((unitSize + sizeof(UINT_PTR)) * unitCnt);
	memset(chunk, 0xFDFD'FDFD, (unitSize + sizeof(UINT_PTR)) * unitCnt);
	PBYTE ptr = chunk;

	for (size_t idx = 0; idx < unitCnt - 1; idx++) {
		ptr += unitSize;
		assert(ptr != NULL);
		// increment 추가
		UINT_PTR increment = InterlockedIncrement16(&m_Increment);
		increment <<= (64 - 16);
		*(PUINT_PTR)ptr = (UINT_PTR)(ptr + sizeof(UINT_PTR)) ^ increment;
		ptr += sizeof(UINT_PTR);
	}

	ptr += unitSize;
	assert(ptr != NULL);
	*(PUINT_PTR)ptr = NULL;

	return chunk;
}
#endif