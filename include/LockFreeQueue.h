#pragma once
#include <Windows.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <fstream>
#include <sstream>
#include "LockFreeMemPool.h"
#ifdef _DEBUG
#pragma comment(lib, "../lib/Debug/LockFree.lib")
#else
#pragma comment(lib, "../lib/Release/LockFree.lib")
#endif

using namespace std;

//#define SWAP_CAS_LOCATION
//#define LOCK_FREE_QUEUE_LOG
//#define LOCK_FREE_DEQUEUE_ITERATOR    // Incomplete

template<typename T>
class LockFreeQueue {
private:
    struct Node {
        T data;
        Node* next;
    };

private:
    LockFreeMemPool LFMP;

    Node*   m_Head;        // head node ptr
    Node*   m_Tail;        // tail node ptr
    LONG    m_Size;

    short m_Increment;
    static const unsigned long long mask = 0x0000'FFFF'FFFF'FFFF;

public:
    struct iterator {
        Node*               m_Current;
        LockFreeMemPool*    m_Lfmp;

        iterator(Node* head, LockFreeMemPool* lfmp) : m_Current(head), m_Lfmp(lfmp) {}

        bool pop(T& t) {
            m_Current = (Node*)((UINT_PTR)m_Current & LockFreeQueue::mask);
            if (m_Current == NULL || m_Current == (Node*)0x0000'FFFF'FFFF'FFFF) {
                return false;
            }

            t = m_Current->data;
            Node* newCurrent = m_Current->next;
            m_Lfmp->Free(m_Current);
            m_Current = newCurrent;
        }
    };

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
#if !defined(SWAP_CAS_LOCATION)
        newNode->next = (Node*)0xFFFF'FFFF'FFFF'FFFF;
#else
        newNode->next = NULL;
#endif

        UINT_PTR incrementPart = InterlockedIncrement16(&m_Increment);	// 64 = 16
        incrementPart <<= (64 - 16);
        UINT_PTR managedPtr = ((UINT_PTR)newNode ^ incrementPart);

        USHORT tryCnt = 0;
        while (true) {
            Node* tail = m_Tail;
            Node* originTail = (Node*)((UINT_PTR)tail & mask);
            Node* next = originTail->next;
#if !defined(SWAP_CAS_LOCATION)
            if (InterlockedCompareExchangePointer((PVOID*)&m_Tail, (PVOID)managedPtr, tail) == tail) {
                originTail->next = (Node*)managedPtr;
                break;
            }
#else
            if (next == NULL) {
                if (InterlockedCompareExchangePointer((PVOID*)&originTail->next, (PVOID)managedPtr, next) == next) {
                    m_Tail = (Node*)managedPtr;
                    newNode->next = NULL;
                    break;
                }
            }
#endif
#if defined(ASSERT)
            if (tryCnt++ == 10000) { DebugBreak(); PrintLog(); }
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
            USHORT tryCnt = 0;
            while (true) {
                Node* head = m_Head;
                Node* originHead = (Node*)((UINT_PTR)head & mask);
                Node* next = originHead->next;

                // "m_head == head" ensures that both 'head' and 'next' were read from the same node
                // (Because a technique for identifying nodes using incremental bits from the MSB is already in place)
                if (m_Head == head) {
                    if (next == NULL || next == (Node*)0xFFFF'FFFF'FFFF'FFFF) {
                        InterlockedIncrement(&m_Size);
                        return false;
                    }
                    else {
                        Node* originHeadNext = (Node*)((UINT_PTR)next & mask);
                        t = originHeadNext->data;
                        if (InterlockedCompareExchangePointer((PVOID*)&m_Head, next, head) == head) {
                            LFMP.Free(originHead);

                            Node* newNext = (Node*)((UINT_PTR)next & mask);
                            newNext = newNext->next;
                            break;
                        }
                    }
                }
#if defined(ASSERT)
                if (tryCnt++ == 10000) { DebugBreak(); PrintLog(); }
#endif
            }
        }
        else {
            Node* head = m_Head;
            Node* originHead = (Node*)((UINT_PTR)head & mask);
            Node* next = originHead->next;

            if (m_Head != head) {
#if defined(ASSERT)
                DebugBreak();       // An abnormal flow, as the queue size was confirmed to be greater than 0
#endif
                return false;
            }
            else {
                if (next == NULL || next == (Node*)0xFFFF'FFFF'FFFF'FFFF) {
#if defined(ASSERT)
                    DebugBreak();  // An abnormal flow, as the queue size was confirmed to be greater than 0
#endif
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

#if defined(LOCK_FREE_DEQUEUE_ITERATOR)
    iterator Dequeue() {
        Node* dummy = (Node*)LFMP.Alloc();
#if defined(ASSERT)
        if (dummy == NULL) { DebugBreak(); }
#endif

#if defined(SWAP_CAS_LOCATION)
        DebugBreak();
        // to do: SWAP_CAS_LOCATION 모드 구현
#else
        dummy->next = (Node*)NULL;
#endif

        UINT_PTR incrementPart = InterlockedIncrement16(&m_Increment);	// 64 = 16
        incrementPart <<= (64 - 16);
        UINT_PTR managedDummyPtr = ((UINT_PTR)dummy ^ incrementPart);

        Node* tail;
        Node* originTail;
        Node* next;
        while (true) {
            tail = m_Tail;
            originTail = (Node*)((UINT_PTR)tail & mask);
            next = originTail->next;

            TRY_DEQUEUE_ITER_TAIL(m_Head, tail, next, m_Size);

#if defined(SWAP_CAS_LOCATION)
            DebugBreak();
            // to do: SWAP_CAS_LOCATION 모드 구현
#else
            if (next == NULL) {
                if (InterlockedCompareExchangePointer((PVOID*)&originTail->next, (PVOID)0xFFFF'FFFF'FFFF'FFFF, next) == next) {
                    TRY_DEQUEUE_ITER_TAIL_COMMIT(m_Head, tail, (void*)0xFFFF'FFFF'FFFF'FFFF, m_Size);
                    break;
                }
        }
#endif
        }

        /////////////////////////////////////////
        // 이 시점 이후부턴 추가적인 Enqueue 없음
        /////////////////////////////////////////
        LONG queueSize = InterlockedExchange(&m_Size, 0);
        if (queueSize <= 0) {           // 순간적으로 queueSize는 음수가 될 수 있다(Dequque 시도에 의해) 따라서 부등호 조건식이 필요
            if (queueSize < 0) {
                InterlockedAdd(&m_Size, queueSize);
            }

            LFMP.Free(dummy);
            originTail->next = NULL;
            return iterator(NULL, &this->LFMP);
        }

        /////////////////////////////////////////
        // 이 시점 이후부턴 추가적인 Dequeue 없음
        // 단, 진행중인 Dequeue는 존재할 수 있음
        /////////////////////////////////////////
        Node* head;
        Node* originHead;
        while (true) {
            head = m_Head;
            originHead = (Node*)((UINT_PTR)head & mask);
            if (InterlockedCompareExchangePointer((PVOID*)&m_Head, (PVOID)managedDummyPtr, head) == head) {
                break;
            }
        }
        Node* current = originHead->next;
#if defined(ASSERT)
        if (current == NULL) { DebugBreak(); PrintLog(); }
#endif

        LONG nodeCnt = 0;
        while ((UINT_PTR)current != 0xFFFF'FFFF'FFFF'FFFF) {
            nodeCnt++;
            Node* originPtr = (Node*)((UINT_PTR)current & mask);
            current = originPtr->next;
        }

        if (nodeCnt == 0) {
            m_Head = originTail;
            originTail->next = NULL;

            Sleep(0);
            Sleep(0);
            Sleep(0);
            LFMP.Free(dummy);   // Free 된 직후 다시 할당 받아 인큐될 시 잘못된 디큐잉 우려...?

            return iterator(NULL, &this->LFMP);
        }
        else {
            if (queueSize > nodeCnt) {
                InterlockedAdd(&m_Size, queueSize - nodeCnt);
            }
            m_Tail = (Node*)managedDummyPtr;
            
            // Enqueue 시도 스레드에서 새로운 더미가 아닌 기존 tail에 새로운 노드를 추가할 수 있음
            // 따라서 orinTail->next == 0xFFFF'FFFF'FFFF'FFFF 유지
            //originTail->next = NULL;

            return iterator(originHead->next, &this->LFMP);
        }
    }
#endif

};

#if defined(LOCK_FREE_QUEUE_LOG)
struct stLog {
    BYTE type;      // 0: enqueue, 1: dequeue, 2: dequeueIter
    void* head;
    void* tail;
    void* next;
    bool commited;
    LONG queueSize;
    BYTE detail;
};

USHORT LogIndex;
stLog LogVec[USHRT_MAX];

void PrintLog() {
    // 현재 날짜와 시간을 파일 제목으로 설정
    auto now = chrono::system_clock::now();
    //time_t now_time = chrono::system_clock::to_time_t(now);
    //tm* now_tm = localtime(&now_time);
    time_t rawtime;
    struct tm timeinfo;
    time(&rawtime);
    localtime_s(&timeinfo, &rawtime);

    ostringstream filename;
    filename << "Log_"
        << (timeinfo.tm_year + 1900) << "-"
        << (timeinfo.tm_mon + 1) << "-"
        << timeinfo.tm_mday << "_"
        << timeinfo.tm_hour << "-"
        << timeinfo.tm_min << "-"
        << timeinfo.tm_sec << ".txt";

    ofstream logfile(filename.str());
    if (!logfile.is_open()) {
        cerr << "Failed to open log file!" << endl;
        return;
    }

    stLog cmp = { 0 };
    for (USHORT i = 0; i < LogIndex; i++) {
        if (memcmp(&LogVec[0], &cmp, sizeof(stLog)) == 0) {
            break;
        }

        logfile << "====================================" << endl;
        if (LogVec[i].type == 0) {
            if (!LogVec[i].commited) {
                logfile << "BEFORE ENQUEUE COMMIT" << endl;
                logfile << "m_Head          : " << LogVec[i].head << endl;
                logfile << "tail            : " << LogVec[i].tail << endl;
                logfile << "tail->next      : " << LogVec[i].next << endl;
            }
            else {
                logfile << "AFTER ENQUEUE COMMIT" << endl;
                logfile << "m_Head          : " << LogVec[i].head << endl;
                logfile << "tail(new)       : " << LogVec[i].tail << endl;
                //logfile << "head->next(new) : " << LogVec[i].next << endl;
            }
        }
        else if (LogVec[i].type == 1) {
            if (!LogVec[i].commited) {
                logfile << "BEFORE DEQUEUE COMMIT" << endl;
                logfile << "head            : " << LogVec[i].head << endl;
                logfile << "head->next      : " << LogVec[i].next << endl;
                logfile << "m_Tail          : " << LogVec[i].tail << endl;
            }
            else {
                logfile << "AFTER DEQUEUE COMMIT" << endl;
                logfile << "head(new)       : " << LogVec[i].head << endl;
                logfile << "head->next(new) : " << LogVec[i].next << endl;
                logfile << "m_Tail          : " << LogVec[i].tail << endl;
            }
        }
        else if (LogVec[i].type == 2) {
            if (!LogVec[i].commited) {
                if (LogVec[i].detail == 0) {
                    logfile << "BEFORE ENQUEUE BLOCK [ITER]" << endl;
                    logfile << "m_Head          : " << LogVec[i].head << endl;
                    logfile << "tail            : " << LogVec[i].tail << endl;
                    logfile << "tail->next      : " << LogVec[i].next << endl;
                }
                else if (LogVec[i].detail == 1) {
                    logfile << "AFTER ENQUEUE BLOCK [ITER]" << endl;
                    logfile << "m_Head          : " << LogVec[i].head << endl;
                    logfile << "tail            : " << LogVec[i].tail << endl;
                }
                else if (LogVec[i].detail == 2) {
                    logfile << "BEFORE SWAP DUMMY [ITER]" << endl;
                    logfile << "head            : " << LogVec[i].head << endl;
                    logfile << "head->next      : " << LogVec[i].next << endl;
                    logfile << "m_Tail          : " << LogVec[i].tail << endl;
                }
                else if (LogVec[i].detail == 3) {
                    logfile << "AFTER SWAP DUMMY[ITER]" << endl;
                    logfile << "dummy(new)      : " << LogVec[i].head << endl;
                    logfile << "m_Tail          : " << LogVec[i].tail << endl;
                }
            }
            else {
                logfile << "AFTER DEQUEUE_ITER COMMIT" << endl;
                logfile << "HEAD     : " << LogVec[i].head << endl;
                logfile << "HEAD NEXT: " << LogVec[i].next << endl;
                logfile << "TAIL     : " << LogVec[i].tail << endl;
            }
        }

        logfile << "queueSize     : " << LogVec[i].queueSize << endl;
    }

    cout << "로그 파일 완료" << endl;
}

void Logging(BYTE type, void* head, void* tail, void* next, bool commited, LONG queueSize, BYTE detail = 0) {
    USHORT idx = InterlockedIncrement16((SHORT*)&LogIndex);
    idx -= 1;

    LogVec[idx].type = type;
    LogVec[idx].head = head;
    LogVec[idx].tail = tail;
    LogVec[idx].next = next;
    LogVec[idx].commited = commited;
    LogVec[idx].queueSize = queueSize;
    LogVec[idx].detail = detail;
}

void TRY_ENQUEUE(void* head, void* tail, void* next, LONG queueSize) {
    Logging(0, head, tail, next, false, queueSize);
}
void COMMIT_ENQUEUE(void* head, void* tail, void* next, LONG queueSize) {
    Logging(0, head, tail, next, true, queueSize);
}
void TRY_DEQUEUE(void* head, void* tail, void* next, LONG queueSize) {
    Logging(1, head, tail, next, false, queueSize);
}
void COMMIT_DEQUEUE(void* head, void* tail, void* next, LONG queueSize) {
    Logging(1, head, tail, next, true, queueSize);
}

void TRY_DEQUEUE_ITER_TAIL(void* head, void* tail, void* next, LONG queueSize) {
    Logging(2, head, tail, next, false, queueSize, 0);
}
void TRY_DEQUEUE_ITER_TAIL_COMMIT(void* head, void* tail, void* next, LONG queueSize) {
    Logging(2, head, tail, next, false, queueSize, 1);
}
void TRY_DEQUEUE_ITER_HEAD(void* head, void* tail, void* next, LONG queueSize) {
    Logging(2, head, tail, next, false, queueSize, 2);
}
void TRY_DEQUEUE_ITER_HEAD_COMMIT(void* head, void* tail, void* next, LONG queueSize) {
    Logging(2, head, tail, next, false, queueSize, 3);
}
void COMMIT_DEQUEUE_ITER(void* head, void* tail, void* next, LONG queueSize) {
    Logging(2, head, tail, next, true, queueSize);
}
#endif