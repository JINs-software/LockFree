#include "LockFreeMemPool.h"

LockFreeMemPool::LockFreeMemPool(size_t unitSize, size_t unitCnt)
	: m_UnitSize(unitSize), m_UnitCnt(unitCnt)
{
	m_ListFront = m_FreeFront = (PBYTE)malloc((m_UnitSize + sizeof(UINT_PTR)) * m_UnitCnt);
	PBYTE ptr = m_FreeFront;
	for (size_t idx = 0; idx < m_UnitCnt - 1; idx++) {
		ptr += m_UnitSize;
		assert(ptr != NULL);
		*(PUINT_PTR)ptr = (UINT_PTR)(ptr + sizeof(UINT_PTR));
		ptr += sizeof(UINT_PTR);
	}

	ptr += m_UnitSize;
	assert(ptr != NULL);
	*(PUINT_PTR)ptr = NULL;
}

LockFreeMemPool::~LockFreeMemPool() {
	free(m_ListFront);
}

void* LockFreeMemPool::Alloc()
{
	void* ret = NULL;
	if (m_FreeFront != NULL) {
		ret = m_FreeFront;
		m_FreeFront = (PBYTE)(*(PUINT_PTR)(m_FreeFront + m_UnitSize));
	}
	return ret;
}

void LockFreeMemPool::Free(void* address)
{
	PBYTE ptr = (PBYTE)address;
	ptr += m_UnitSize;
	*(PUINT_PTR)ptr = (UINT_PTR)m_FreeFront;
	m_FreeFront = (PBYTE)address;
}
