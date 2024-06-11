#pragma once
#include <Windows.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <fstream>
#include <sstream>
#include "LockFreeMemPool.h"
#pragma comment(lib, "LockFreeMemoryPool")

using namespace std;

#define SIMPLE_ENQUEUE
//#define SWAP_CAS_LOCATION

struct stLog {
    BYTE type;      // 0: enqueue, 1: dequeue, 2: dequeueIter
    void* head;
    void* tail;
    void* next;
    bool commited;
    BYTE detail;
};

USHORT LogIndex;
stLog LogVec[USHRT_MAX];

void PrintLog() {
    // 현재 날짜와 시간을 파일 제목으로 설정
    auto now = chrono::system_clock::now();
    time_t now_time = chrono::system_clock::to_time_t(now);
    tm* now_tm = localtime(&now_time);

    ostringstream filename;
    filename << "Log_" << (now_tm->tm_year + 1900) << "-"
        << (now_tm->tm_mon + 1) << "-"
        << now_tm->tm_mday << "_"
        << now_tm->tm_hour << "-"
        << now_tm->tm_min << "-"
        << now_tm->tm_sec << ".txt";

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
            }
            else {
                logfile << "AFTER ENQUEUE COMMIT" << endl;
            }

            logfile << "HEAD     : " << LogVec[i].head << endl;
            logfile << "TAIL     : " << LogVec[i].tail << endl;
            logfile << "TAIL NEXT: " << LogVec[i].next << endl;
        }
        else if (LogVec[i].type == 1) {
            if (!LogVec[i].commited) {
                logfile << "BEFORE DEQUEUE COMMIT" << endl;
            }
            else {
                logfile << "AFTER DEQUEUE COMMIT" << endl;
            }

            logfile << "HEAD     : " << LogVec[i].head << endl;
            logfile << "HEAD NEXT: " << LogVec[i].next << endl;
            logfile << "TAIL     : " << LogVec[i].tail << endl;
        }
        else if (LogVec[i].type == 2) {
            if (!LogVec[i].commited) {
                if (LogVec[i].detail == 0) {
                    logfile << "BEFORE ENQUEUE COMMIT [ITER]" << endl;
                    logfile << "HEAD     : " << LogVec[i].head << endl;
                    logfile << "TAIL     : " << LogVec[i].tail << endl;
                    logfile << "TAIL NEXT: " << LogVec[i].next << endl;
                }
                else if (LogVec[i].detail == 1) {
                    logfile << "AFTER ENQUEUE COMMIT [ITER]" << endl;
                    logfile << "HEAD     : " << LogVec[i].head << endl;
                    logfile << "TAIL     : " << LogVec[i].tail << endl;
                    logfile << "TAIL NEXT: " << LogVec[i].next << endl;
                }
                else if (LogVec[i].detail == 2) {
                    logfile << "BEFORE DEQUEUE COMMIT [ITER]" << endl;
                    logfile << "HEAD     : " << LogVec[i].head << endl;
                    logfile << "HEAD NEXT: " << LogVec[i].next << endl;
                    logfile << "TAIL     : " << LogVec[i].tail << endl;
                }
                else if (LogVec[i].detail == 3) {
                    logfile << "AFTER DEQUEUE COMMIT [ITER]" << endl;
                    logfile << "HEAD     : " << LogVec[i].head << endl;
                    logfile << "HEAD NEXT: " << LogVec[i].next << endl;
                    logfile << "TAIL     : " << LogVec[i].tail << endl;
                }
            }
            else {
                logfile << "AFTER DEQUEUE_ITER COMMIT" << endl;
                logfile << "HEAD     : " << LogVec[i].head << endl;
                logfile << "HEAD NEXT: " << LogVec[i].next << endl;
                logfile << "TAIL     : " << LogVec[i].tail << endl;
            }
        }
    }

    cout << "로그 파일 완료" << endl;
}

void Logging(BYTE type, void* head, void* tail, void* next, bool commited, BYTE detail = 0) {
    USHORT idx = InterlockedIncrement16((SHORT*)&LogIndex);
    idx -= 1;
   
    LogVec[idx].type = type;
    LogVec[idx].head = head;
    LogVec[idx].tail = tail;
    LogVec[idx].next = next;
    LogVec[idx].commited = commited;
    LogVec[idx].detail = detail;
}

void TRY_ENQUEUE(void* head, void* tail, void* next) {
    Logging(0, head, tail, next, false);
}
void COMMIT_ENQUEUE(void* head, void* tail, void* next) {
    Logging(0, head, tail,next,  true);
}
void TRY_DEQUEUE(void* head, void* tail, void* next) {
    Logging(1, head, tail, next, false);
}
void COMMIT_DEQUEUE(void* head, void* tail, void* next) {
    Logging(1, head, tail,next,  true);
}

void TRY_DEQUEUE_ITER_TAIL(void* head, void* tail, void* next) {
    Logging(2, head, tail, next, false, 0);
}
void TRY_DEQUEUE_ITER_TAIL_COMMIT(void* head, void* tail, void* next) {
    Logging(2, head, tail, next, false, 1);
}
void TRY_DEQUEUE_ITER_HEAD(void* head, void* tail, void* next) {
    Logging(2, head, tail, next, false, 2);
}
void TRY_DEQUEUE_ITER_HEAD_COMMIT(void* head, void* tail, void* next) {
    Logging(2, head, tail, next, false, 3);
}
void COMMIT_DEQUEUE_ITER(void* head, void* tail, void* next) {
    Logging(2, head, tail,next,  true);
}


template<typename T>
class LockFreeQueue {
private:
    struct Node {
        T data;
        Node* next;
    };

private:
    LockFreeMemPool LFMP;

    Node*   m_Head;        // 시작노드를 포인트한다.
    Node*   m_Tail;        // 마지막노드를 포인트한다.
    LONG    m_Size;

    short m_Increment;
    static const unsigned long long mask = 0x0000'FFFF'FFFF'FFFF;

public:
    struct iterator {
        Node* current;

        iterator(Node* head) : current(head) {}
        bool operator!=(const iterator& other) const {
            return current != other.current;
        }
        bool pop(T& t) {
            current = (Node*)((UINT_PTR)current & LockFreeQueue::mask);
            if (current == NULL) {
                return false;
            }

            t = current->data;
            current = current->next;
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
#if defined(SWAP_CAS_LOCATION)
        newNode->next = NULL;
#else
        newNode->next = (Node*)0xFFFF'FFFF'FFFF'FFFF;
#endif

        UINT_PTR incrementPart = InterlockedIncrement16(&m_Increment);	// 64 = 16
        incrementPart <<= (64 - 16);
        UINT_PTR managedPtr = ((UINT_PTR)newNode ^ incrementPart);

        USHORT tryCnt = 0;
        while (true) {
            Node* tail = m_Tail;
            Node* originTail = (Node*)((UINT_PTR)tail & mask);
            Node* next = originTail->next;

            TRY_ENQUEUE(m_Head, tail, next);

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
            if (tryCnt++ == 100) {
                DebugBreak();
                PrintLog();
            }
        }

        COMMIT_ENQUEUE(m_Head, (PVOID)managedPtr, 0);

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

                TRY_DEQUEUE(head, m_Tail, next);

                // 아래 조건문에서 m_head == head가 충족한다는 것은 위 head와 next는 동일한 노드에서 읽은 것이 보장된다.
                // 이미 MSB부터 일부에 증분 비트로 노드를 식별하는 기법이 들어가 있기 때문.
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

                            COMMIT_DEQUEUE(next, m_Tail, newNext);

                            break;
                        }
                    }
                }

                if (tryCnt++ == 100) {
                    DebugBreak();
                    PrintLog();
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
        if (dummy == NULL) {
            DebugBreak();
        }

#if defined(SIMPLE_ENQUEUE)
        dummy->next = (Node*)NULL;
#elif defined(SWAP_CAS_LOCATION)
        DebugBreak();
        // to do: SWAP_CAS_LOCATION 모드 구현
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

            TRY_DEQUEUE_ITER_TAIL(m_Head, tail, next);

#if defined(SIMPLE_ENQUEUE)
            if (next == NULL) {
                if (InterlockedCompareExchangePointer((PVOID*)&originTail->next, (PVOID)0xFFFF'FFFF'FFFF'FFFF, next) == next) {
                    TRY_DEQUEUE_ITER_TAIL_COMMIT(m_Head, tail, (void*)0xFFFF'FFFF'FFFF'FFFF);
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
        if (queueSize == 0) {
            LFMP.Free(dummy);
            originTail->next = NULL;
            return iterator(NULL);
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

            TRY_DEQUEUE_ITER_HEAD(head, m_Tail, originHead->next);

            if (InterlockedCompareExchangePointer((PVOID*)&m_Head, (PVOID)managedDummyPtr, head) == head) {
                break;
            }
        }

        TRY_DEQUEUE_ITER_HEAD_COMMIT((PVOID)managedDummyPtr, m_Tail, (void*)0);

        Node* current = originHead->next;
        if (current == NULL) {
            DebugBreak();
            PrintLog();
        }

        UINT nodeCnt = 0;
        while ((UINT_PTR)current != 0xFFFF'FFFF'FFFF'FFFF) {
            nodeCnt++;
            Node* originPtr = (Node*)((UINT_PTR)current & mask);
            current = originPtr->next;
        }

        if (nodeCnt == 0) {
            LFMP.Free(dummy);
            originTail->next = NULL;

            return iterator(NULL);
        }
        else {
            COMMIT_DEQUEUE_ITER((PVOID)managedDummyPtr, (PVOID)managedDummyPtr, (void*)NULL);

            if (queueSize > nodeCnt) {
                InterlockedAdd(&m_Size, queueSize - nodeCnt);
            }
            m_Tail = (Node*)managedDummyPtr;
            originTail->next = NULL;

            return iterator(originHead->next);
        }
    }
};
