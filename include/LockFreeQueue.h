#pragma once
#include <Windows.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include "LockFreeMemPool.h"
#pragma comment(lib, "LockFreeMemoryPool")

#define SIMPLE_ENQUEUE
//#define SWAP_CAS_LOCATION

template<typename T>
class LockFreeQueue {
private:
    struct Node {
        T data;
        Node* next;
    };
    struct iterator {

        iterator(Node*);
    };

    LockFreeMemPool LFMP;

    Node*   m_Head;        // 시작노드를 포인트한다.
    Node*   m_Tail;        // 마지막노드를 포인트한다.
    LONG    m_Size;

    short m_Increment;
    const unsigned long long mask = 0x0000'FFFF'FFFF'FFFF;

public:
    LockFreeQueue() 
        : LFMP(sizeof(T), 0), m_Increment(0), m_Size(0)
    {
        m_Head = (Node*)LFMP.Alloc();
        m_Head->next = NULL;
        m_Tail = m_Head;
    }
    inline LONG GetSize() {
        return m_Size;
    }

    void Enqueue(T t) {
        Node* newNode = (Node*)LFMP.Alloc();
        if (newNode == NULL) {
            DebugBreak();
        }

        newNode->data = t;
#if defined(SWAP_CAS_LOCATION)
        newNode->next = NULL;
#else
        newNode->next = (Node*)0xFFFF'FFFF'FFFF'FFFF;
#endif

        UINT_PTR incrementPart = InterlockedIncrement16(&m_Increment);	// 64 = 16
        incrementPart <<= (64 - 16);
        UINT_PTR managedPtr = ((UINT_PTR)newNode ^ incrementPart);

        while (true) {
            Node* tail = m_Tail;
            Node* originTail = (Node*)((UINT_PTR)tail & mask);
            Node* next = originTail->next;

#if defined(SIMPLE_ENQUEUE)
            if (next == NULL) {
                if (InterlockedCompareExchangePointer((PVOID*)&originTail->next, (PVOID)managedPtr, next) == next) {
                    m_Tail = (Node*)managedPtr;
                    newNode->next = NULL;
                    break;
                }
            }
#elif defined(SWAP_CAS_LOCATION)
            if (InterlockedCompareExchangePointer((PVOID*)&m_Tail, (PVOID)managedPtr, tail) == tail) {
                originTail->next = (Node*)managedPtr;
                break;
            }
#endif
        }
        InterlockedIncrement(&m_Size);
    }

    bool Dequeue(T& t, bool singleReader = false) {
        if (InterlockedDecrement(&m_Size) < 0) {
            InterlockedIncrement(&m_Size);
            return false;
        }
        if (!singleReader) {
            while (true) {
                Node* head = m_Head;
                Node* originHead = (Node*)((UINT_PTR)head & mask);
                Node* next = originHead->next;

                // 아래 조건문에서 m_head == head가 충족한다는 것은 위 head와 next는 동일한 노드에서 읽은 것이 보장된다.
                // 이미 MSB부터 일부에 증분 비트로 노드를 식별하는 기법이 들어가 있기 때문.
                if (m_Head == head) {
                    if (next == NULL || next == (Node*)0xFFFF'FFFF'FFFF'FFFF) {
                        return false;
                    }
                    else {
                        Node* originHeadNext = (Node*)((UINT_PTR)next & mask);
                        t = originHeadNext->data;
                        if (InterlockedCompareExchangePointer((PVOID*)&m_Head, next, head) == head) {
                            LFMP.Free(originHead);
                            break;
                        }
                    }
                }
            }
        }
        else {
            Node* head = m_Head;
            Node* originHead = (Node*)((UINT_PTR)head & mask);
            Node* next = originHead->next;

            if (m_Head != head) {
                DebugBreak();  // 큐 사이즈 > 0을 확인하였기에 예외처리 
                return false;
            }
            else {
                if (next == NULL || next == (Node*)0xFFFF'FFFF'FFFF'FFFF) {
                    DebugBreak();  // 큐 사이즈 > 0을 확인하였기에 예외처리 
                    return false;
                }
                else {
                    Node* originHeadNext = (Node*)((UINT_PTR)next & mask);
                    t = originHeadNext->data;
                    m_Head = next;
                    LFMP.Free(originHead);
                }
            }
        }

        return true;
    }

    iterator Dequeue() {
        Node* dummy = (Node*)LFMP.Alloc();
        if (newNode == NULL) {
            DebugBreak();
        }

        dummy->data = t;
#if defined(SIMPLE_ENQUEUE)
        dummy->next = (Node*)0xFFFF'FFFF'FFFF'FFFF;
#elif defined(SWAP_CAS_LOCATION)
        DebugBreak();
        // to do: SWAP_CAS_LOCATION 모드 구현
#endif

        UINT_PTR incrementPart = InterlockedIncrement16(&m_Increment);	// 64 = 16
        incrementPart <<= (64 - 16);
        UINT_PTR managedDummyPtr = ((UINT_PTR)dummy ^ incrementPart);

        while (true) {
            Node* tail = m_Tail;
            Node* originTail = (Node*)((UINT_PTR)tail & mask);
            Node* next = originTail->next;

#if defined(SIMPLE_ENQUEUE)
            if (next == NULL) {
                if (InterlockedCompareExchangePointer((PVOID*)&originTail->next, (PVOID)0xFFFF'FFFF'FFFF'FFFF, next) == next) {
                    //m_Tail = (Node*)managedDummyPtr;
                    break;
                }
            }
#elif defined(SWAP_CAS_LOCATION)
            DebugBreak();
            // to do: SWAP_CAS_LOCATION 모드 구현
            // ... 
#endif
        }

        /////////////////////////////////////////
        // 이 시점 이후부턴 추가적인 Enqueue 없음
        /////////////////////////////////////////
        UINT queueSize = InterlockedExchange(&m_Size, 0);

        /////////////////////////////////////////
        // 이 시점 이후부턴 추가적인 Dequeue 없음
        // 단, 진행중인 Dequeue는 존재할 수 있음
        /////////////////////////////////////////
        Node* head;
        while (true) {
            head = m_Head;
            if (InterlockedCompareExchange(&m_Head, managedDummyPtr, m_Head) == m_Head) {
                break;
            }
        }

        Node* beforeTail = m_Tail;
        m_Tail = managedDummyPtr;
        beforeTail->next = NULL;


        return iterator(head);
    }
};
