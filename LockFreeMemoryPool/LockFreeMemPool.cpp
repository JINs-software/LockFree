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
			// �̹� �Ҵ�� �ּҸ� �ٽ� �Ҵ��� ������ Ȯ��
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
			// �ٸ� �����忡�� �Ҵ�� �ּҸ� ��ȯ�� ������ Ȯ�� or �Ҵ���� �ּҸ� ��ȯ�� ������ Ȯ��
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
// [������] LockFreeMemPool::LockFreeMemPool(size_t unitSize, size_t unitCnt)
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
// [�Ҹ���] LockFreeMemPool::~LockFreeMemPool()
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
// [�Ҵ�] void* LockFreeMemPool::Alloc()
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
	// ������ ���� ��Ȳ�� �߻��� �� �ִ�.
	// (1) threadA : freeFront == 0xA000���� while ���ǹ� ����, ù ��° ����(freeFront ?= NULL) ���
	// (2) threadB : freeFront == 0xA000���� while ���ǹ� ����, ù ��° ���� �׸��� InterlockedCompareExchangePointer ����
	//															m_FreeFront	= NULL(0xA000 ��ϸ� ���� ��Ȳ) ���� �� �� ��° ���� ���
	// (3) threadA : InterlockedCompareExchangePointer ����, �׷��� m_FreeFront�� ��������� �ʾ���. �ٸ� ��ȯ �� ret = 0xA000 ����
	// (4) threadA : ���� �������� freeFront�� NULL�̱⿡ �������� ��������.
	//				=> ������ ���⿡�� ret != NULL�̱⿡ ������ �ּҰ� Alloc ��ȯ��!!
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
			// �� �̻� �Ҵ��� ���� ���� ����
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
			// ���⼭ Sleep(0) �Ǵ� SwitchToThread �Լ��� ȣ���Ͽ� �� Ÿ�� �����̽��� �����ϴ� ���� ���??
			// Sleep(0)�� �ߺ��Ͽ� ���� �� ȣ���ϴ� ���� �ǹ̴�??
			// �ǹ̰� �ִٸ� �� ���� ȣ���ϴ� ���� ������??
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
			// �� �̻� �Ҵ��� ���� ���� ����
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

					// ��Ƽ-�����忡�� ����ȭ ������ �߻��� ������ ����ȴ�.
					// m_FreeFront == NULL ���� Ȯ���ϰ� ���ο� ûũ�� �Ҵ�޾� m_FreeFront�� ûũ�� ����Ű���� �ϱ� ��
					// �ٸ� �����忡�� Free�� ó���ϴ� �������� m_FreeFront�� ��ȯ�� �޸𸮸� ����ų �� �ִ�.
					// �� �� �Ʒ� �ڵ尡 ����Ǹ�, ��ȯ�� �޸𸮴� �����ȴ�. 
					m_FreeFront = newChunk;

					// �ذ� �����? 
					// Resize �۾� ���� Free�� ���´�.  m_ResizeLock �� ������ pop���� ������� ����.
				}
				
				m_ResizeLock = 0;
			}
		}
		else {
			// ���⼭ Sleep(0) �Ǵ� SwitchToThread �Լ��� ȣ���Ͽ� �� Ÿ�� �����̽��� �����ϴ� ���� ���??
			// Sleep(0)�� �ߺ��Ͽ� ���� �� ȣ���ϴ� ���� �ǹ̴�??
			// �ǹ̰� �ִٸ� �� ���� ȣ���ϴ� ���� ������??
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
// [��ȯ] void LockFreeMemPool::Free(void* address)
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
			freeFront = m_FreeFront;						// ���� �ٶ󺸰� �ִ� freeFront (��Ʈ �ĺ��� ����)
			*(PUINT_PTR)ptr = (UINT_PTR)freeFront & mask;	// ���� �ٶ󺸰� �ִ� freeFront�� next
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
// [ûũ �Ҵ�(���� �Լ�)] 
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
			*(PUINT_PTR)dupPtr = copyPtr;	// ������ �����ͷ� ����
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
		// increment �߰�
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