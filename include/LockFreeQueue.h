#pragma once
#include <Windows.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include "LockFreeMemPool.h"
#pragma comment(lib, "LockFreeMemoryPool")

#define LOG_ARR_SIZE 10'000'000

#define SIMPLE_ENQUEUE
//#define SWAP_CAS_LOCATION

//#define LOGGING

template<typename T>
class LockFreeQueue {
private:
    struct Node {
        T data;
        Node* next;
    };
    Node test_CompareNode;

    LockFreeMemPool LFMP;

    Node*   m_Head;        // 시작노드를 포인트한다.
    Node*   m_Tail;        // 마지막노드를 포인트한다.
    //size_t m_Size;
    LONG    m_Size;

    short m_Increment;
    const unsigned long long mask = 0x0000'FFFF'FFFF'FFFF;

#if defined(LOGGING)
    ///////////////////////// LOG ///////////////////////// 
    struct stLog {
        DWORD threadID = 0;
        bool isEnqueue = 0;
        bool isCommit = 0;
        bool isAllCommit = 0;

        UINT_PTR ptr0 = 0;
        UINT_PTR ptr1 = 0;
        UINT_PTR ptr2 = 0;
        UINT_PTR ptr3 = 0;
    };
    stLog m_LogArr[USHRT_MAX + 1];
    unsigned short m_LogIndex;
    std::mutex g_LogPrintMtx;
    ///////////////////////////////////////////////////////
#endif

public:
    LockFreeQueue() 
        : LFMP(sizeof(T), 0), m_Increment(0), m_Size(0)
    {
        m_Head = (Node*)LFMP.Alloc();
        m_Head->next = NULL;
        m_Tail = m_Head;

        ///////////////////////// TEST ////////////////////////
        memset((void*)&test_CompareNode, 0xFDFD'FDFD, sizeof(Node));

#if defined(LOGGING)
        ///////////////////////// LOG /////////////////////////
        memset( m_LogArr, 0, sizeof(stLog) * USHRT_MAX + 1);
        m_LogIndex = 0;
#endif
    }
#if defined(LOGGING)
    void ClearLog() {
        ///////////////////////// LOG /////////////////////////
        memset(m_LogArr, 0, sizeof(stLog) * USHRT_MAX + 1);
        m_LogIndex = 0;
    }
#endif

    LONG GetSize() {
        return m_Size;
    }

    void Enqueue(T t) {
#if defined(LOGGING)
        ///////////////////////// LOG ///////////////////////// 
        DWORD thID = GetThreadId(GetCurrentThread());
        ///////////////////////////////////////////////////////
#endif

        Node* newNode = (Node*)LFMP.Alloc();
        if (newNode == NULL) {
            DebugBreak();
        }
        //if (memcmp((void*)newNode, (void*)&test_CompareNode, sizeof(Node)) != 0) {
        //    DebugBreak();
        //}
        // => 검증 필요
        if (memcmp((void*)newNode, (void*)&test_CompareNode, sizeof(Node::data)) != 0) {
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

            // [next의 NULL 처리를 안하면?]
            // next의 NULL 비교가 없이 첫 번째 CAS를 그대로 수행해본다 가정한다. 첫 번째 CAS의 변동 사항은 없다. 여전히 비교 operand는 지역 next이다.
            // 
            // 0x1000[더미] -> 0xA000[노드] -> NULL
            // m_head          m_tail
            //
            // (1) 스레드A가 첫 번째 CAS를 수행하기 전 m_Tail 값만 지역 변수로 읽음,  originalTail: 0xA000
            // (2) 스레드B가 첫 번째 CAS를 수행하고, 두 번째 CAS 수행 전. 큐의 상황은 아래와 같아짐.
            //     0x1000[더미] -> 0xA000[노드] -> 0xB000[노드] -> NULL
            //     m_head          m_tail (두 번째 CAS 전이므로 아직 tail의 변동은 없음)
            // (3) 스레드A가 기존에 읽은 0xA000 노드의 지역 next를 0xB000으로 읽고, 첫 번째 CAS를 수행함. 문제는 0xA000 노드의 next 값이 갱신됨
            //     0x1000[더미] -> 0xA000[노드] -> 0xB000[노드] -> NULL
            // -> 0xA000 노드의 next 였던 0xB000이 0xC000[노드]로 덮여써진다. 결국 0xB000 노드가 누락되었다.
            //     0x1000[더미] -> 0xA000[노드] -> 0xC000[노드] -> NULL
            //     m_head          m_tail 
            // (4) 위와 같은 상황에서 스레드A의 두번째 CAS가 스레드B의 두번째 CAS를 앞선다면, 단순히 노드 하나가 누락되는 문제에서 끝나지만(물론 이 자체가 결함),
            //     스레드B의 두번째 CAS가 먼저 수행된다면 최종적으로 m_Tail은 누락된 0xB000을 가리키게 되는 상황이 된다.
            // 
            // (결론)
            // 만약 도입부에 next == NULL 조건문을 생략하고 싶다면, 첫 번째 CAS 코드를 수정해야 한다.
            // if (InterlockedCompareExchangePointer((PVOID*)&originTail->next, (PVOID)managedPtr, next) == next)
            //  => if (InterlockedCompareExchangePointer((PVOID*)&originTail->next, (PVOID)managedPtr, NULL) == NULL)

#if defined(SIMPLE_ENQUEUE)
            if (next == NULL) {

#if defined(LOGGING)
                ///////////////////////// LOG ///////////////////////// 
                unsigned short logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
                m_LogArr[logIdx].threadID = thID;
                m_LogArr[logIdx].isEnqueue = true;              // Enqueue
                m_LogArr[logIdx].isCommit = false;              // before first CAS Commit
                m_LogArr[logIdx].isAllCommit = false;           // before all CAS 
                m_LogArr[logIdx].ptr0 = (UINT_PTR)tail;         // 현재 바라보고 있는 Tail
                m_LogArr[logIdx].ptr1 = (UINT_PTR)next;         // 현재 바라보고 있는 Tail의 next
                m_LogArr[logIdx].ptr2 = (UINT_PTR)managedPtr;   // Enqueue 예정 노드 주소
                ///////////////////////////////////////////////////////
#endif

                if (InterlockedCompareExchangePointer((PVOID*)&originTail->next, (PVOID)managedPtr, next) == next) {
                    //newNode->next = NULL;

#if defined(LOGGING)
                    ///////////////////////// LOG ///////////////////////// 
                    logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
                    m_LogArr[logIdx].threadID = thID;
                    m_LogArr[logIdx].isEnqueue = true;              // Enqueue
                    m_LogArr[logIdx].isCommit = true;               // after first CAS Commit
                    m_LogArr[logIdx].isAllCommit = false;           // before all CAS
                    m_LogArr[logIdx].ptr0 = (UINT_PTR)tail;         // 바라보고 있던 Tail
                    m_LogArr[logIdx].ptr1 = (UINT_PTR)next;         // 바라보고 있던 Tail의 next
                    m_LogArr[logIdx].ptr2 = (UINT_PTR)managedPtr;   // 새로운 Tail의 Next (Tail 갱신은 미반영)
                    ///////////////////////////////////////////////////////
#endif

                    //InterlockedCompareExchangePointer((PVOID*)&m_Tail, (PVOID)managedPtr, tail);
                    m_Tail = (Node*)managedPtr;
                    newNode->next = NULL;

#if defined(LOGGING)
                    ///////////////////////// LOG ///////////////////////// 
                    logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
                    m_LogArr[logIdx].threadID = thID;               // Enqueue
                    m_LogArr[logIdx].isEnqueue = true;              // after first CAS Commit
                    m_LogArr[logIdx].isCommit = true;               // after all CAS
                    m_LogArr[logIdx].isAllCommit = true;            // 바라보고 있던 Tail
                    m_LogArr[logIdx].ptr0 = (UINT_PTR)tail;         // 바라보고 있던 Tail의 next
                    m_LogArr[logIdx].ptr1 = (UINT_PTR)next;         // 새로운 Tail의 Next (Tail 갱신은 반영 후)
                    m_LogArr[logIdx].ptr2 = (UINT_PTR)managedPtr;
                    ///////////////////////////////////////////////////////
#endif
                    break;
                }
            }
#elif defined(SWAP_CAS_LOCATION)
            ///////////////////////// LOG ///////////////////////// 
            unsigned short logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
            m_LogArr[logIdx].threadID = thID;
            m_LogArr[logIdx].isEnqueue = true;              // Enqueue
            m_LogArr[logIdx].isCommit = false;              // before first CAS Commit
            m_LogArr[logIdx].isAllCommit = false;           // before all CAS 
            m_LogArr[logIdx].ptr0 = (UINT_PTR)tail;         // 현재 바라보고 있는 Tail
            m_LogArr[logIdx].ptr1 = (UINT_PTR)next;         // 현재 바라보고 있는 Tail의 next
            m_LogArr[logIdx].ptr2 = (UINT_PTR)managedPtr;   // Enqueue 예정 노드 주소
            ///////////////////////////////////////////////////////
            if (InterlockedCompareExchangePointer((PVOID*)&m_Tail, (PVOID)managedPtr, tail) == tail) {
                ///////////////////////// LOG ///////////////////////// 
                logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
                m_LogArr[logIdx].threadID = thID;
                m_LogArr[logIdx].isEnqueue = true;              // Enqueue
                m_LogArr[logIdx].isCommit = true;               // after first CAS Commit
                m_LogArr[logIdx].isAllCommit = false;           // before all CAS
                m_LogArr[logIdx].ptr0 = (UINT_PTR)tail;         // 바라보고 있던 Tail
                m_LogArr[logIdx].ptr1 = (UINT_PTR)next;         // 바라보고 있던 Tail의 next
                m_LogArr[logIdx].ptr2 = (UINT_PTR)managedPtr;   // 새로운 Tail의 Next (Tail 갱신은 미반영)
                ///////////////////////////////////////////////////////
                originTail->next = (Node*)managedPtr;
                ///////////////////////// LOG ///////////////////////// 
                logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
                m_LogArr[logIdx].threadID = thID;               // Enqueue
                m_LogArr[logIdx].isEnqueue = true;              // after first CAS Commit
                m_LogArr[logIdx].isCommit = true;               // after all CAS
                m_LogArr[logIdx].isAllCommit = true;            // 바라보고 있던 Tail
                m_LogArr[logIdx].ptr0 = (UINT_PTR)tail;         // 바라보고 있던 Tail의 next
                m_LogArr[logIdx].ptr1 = (UINT_PTR)next;         // 새로운 Tail의 Next (Tail 갱신은 반영 후)
                m_LogArr[logIdx].ptr2 = (UINT_PTR)managedPtr;
                ///////////////////////////////////////////////////////
                break;
            }
#endif
        }

        //InterlockedExchangeAdd(&m_Size, 1);
        InterlockedIncrement(&m_Size);
    }

    bool Dequeue(T& t, bool singleReader = false) {
        //if (m_Size == 0) {
        //    return false;
        //}
        //InterlockedExchangeAdd(&m_Size, -1);

        //size_t qsize = m_Size;
        //if (qsize == 0) {
        //    return false;
        //}
        //if (InterlockedCompareExchange(&m_Size, qsize - 1, qsize) != qsize) {
        //    return false;
        //}

        //size_t qsize;
        //do {
        //    qsize = m_Size;
        //    if (qsize == 0) {
        //        return false;
        //    }
        //} while (InterlockedCompareExchange(&m_Size, qsize - 1, qsize) != qsize);

        if (InterlockedDecrement(&m_Size) < 0) {
            InterlockedIncrement(&m_Size);
            return false;
        }
#if defined(LOGGING)
        ///////////////////////// LOG ///////////////////////// 
        DWORD thID = GetThreadId(GetCurrentThread());
        ///////////////////////////////////////////////////////
#endif

        if (!singleReader) {
            while (true) {
                Node* head = m_Head;
                Node* originHead = (Node*)((UINT_PTR)head & mask);
                Node* next = originHead->next;

                // Dequeue 시 문제 발생
                // (상황)
                // 0xA000[더미] -> 0xB000 -> 0xC000 -> NULL
                // m_Head          m_Tail
                // 
                // (1) threadA Dequeue 시도 가 orginHead = 0xA000 저장, next 변수 대입은 이루어지지 않음
                // (2) threadB Dequeue 시도 및 커밋까지 완료 
                // 0xB000[더미] -> 0xC000 -> NULL
                // (3) threadB Enqueue 시도 및 커밋까지 완료
                // 0xB000[더미] -> 0xC000 -> 0xA000 -> NULL
                // (4) threadA가 마저 Dequeue 시도, 여기서 기존에 가리키던 0xA000이 재활용되어 Enqueue된 상태, 따라서 next 변수에는 NULL이 대입됨
                // (5) next가 NULL이므로 아래 조건문에 걸려 Dequeue의 실패로 이어짐
                //
                // (분석)
                // ABA 문제와 비슷하다. 기존 ABA 해결의 접근 방식에서 활용하는 비트 마스킹 기법을 아래 조건문에서는 활용될 수 없는 구조임.
                // 단순히 next를 체크하는 방식에서 변경이 필요함.
                // => head 변수로 읽은 시점과 next 변수로 읽은 시점이 동일함을 보장해주어야 한다. 동일한 노드의 주소를 읽고, 그 노드의 next를 읽는 것을 보장해주어야 한다.
                // 
                //if(next == NULL) {
                //    ///////////////////////// LOG ///////////////////////// 
                //    size_t logIdx = InterlockedIncrement(&m_LogIndex);
                //    if (logIdx >= LOG_ARR_SIZE) {
                //        DebugBreak();
                //    }
                //    m_LogArr[logIdx].ptr3 = 0xFFFF'FFFF'FFFF'FFFF;
                //    m_LogArr[logIdx].threadID = thID;
                //    m_LogArr[logIdx].isEnqueue = false;         // Dequeue
                //    m_LogArr[logIdx].isCommit = false;          // before dequee commit
                //    m_LogArr[logIdx].ptr0 = (UINT_PTR)head;     // 현재 바라보고 있는 head
                //    m_LogArr[logIdx].ptr1 = (UINT_PTR)next;;    // 현재 바라보고 있는 head의 next
                //    ///////////////////////////////////////////////////////
                //    return false;
                //}
                //else {
                //    ///////////////////////// LOG ///////////////////////// 
                //    size_t logIdx = InterlockedIncrement(&m_LogIndex);
                //    if (logIdx >= LOG_ARR_SIZE) {
                //        DebugBreak();
                //    }
                //    m_LogArr[logIdx].threadID = thID;
                //    m_LogArr[logIdx].isEnqueue = false;         // Dequeue
                //    m_LogArr[logIdx].isCommit = false;          // before dequee commit
                //    m_LogArr[logIdx].ptr0 = (UINT_PTR)head;     // 현재 바라보고 있는 head
                //    m_LogArr[logIdx].ptr1 = (UINT_PTR)next;;    // 현재 바라보고 있는 head의 next
                //    ///////////////////////////////////////////////////////
                //    if (InterlockedCompareExchangePointer((PVOID*)&m_Head, next, head) == head) {
                //        ///////////////////////// LOG ///////////////////////// 
                //        size_t logIdx = InterlockedIncrement(&m_LogIndex);
                //        if (logIdx >= LOG_ARR_SIZE) {
                //            DebugBreak();
                //        }
                //        m_LogArr[logIdx].threadID = thID;
                //        m_LogArr[logIdx].isEnqueue = false;
                //        m_LogArr[logIdx].isCommit = true;       // after dequeue commit
                //        m_LogArr[logIdx].ptr0 = (UINT_PTR)head;
                //        m_LogArr[logIdx].ptr1 = (UINT_PTR)next;
                //        ///////////////////////////////////////////////////////
                //
                //        Node* originHeadNext = (Node*)((UINT_PTR)next & mask);
                //        t = originHeadNext->data;
                //        LFMP.Free(originHead);
                //        break;
                //    }
                //}

                // 아래 조건문에서 m_head == head가 충족한다는 것은 위 head와 next는 동일한 노드에서 읽은 것이 보장된다.
                // 이미 MSB부터 일부에 증분 비트로 노드를 식별하는 기법이 들어가 있기 때문.
                if (m_Head == head) {
                    if (next == NULL || next == (Node*)0xFFFF'FFFF'FFFF'FFFF) {
#if defined(LOGGING)
                        ///////////////////////// LOG ///////////////////////// 
                        unsigned short logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
                        m_LogArr[logIdx].ptr3 = 0xFFFF'FFFF'FFFF'FFFF;
                        m_LogArr[logIdx].threadID = thID;
                        m_LogArr[logIdx].isEnqueue = false;         // Dequeue
                        m_LogArr[logIdx].isCommit = false;          // before dequee commit
                        m_LogArr[logIdx].ptr0 = (UINT_PTR)head;     // 현재 바라보고 있는 head
                        m_LogArr[logIdx].ptr1 = (UINT_PTR)next;;    // 현재 바라보고 있는 head의 next
                        ///////////////////////////////////////////////////////
#endif
                        return false;
                    }
                    else {
#if defined(LOGGING)
                        ///////////////////////// LOG ///////////////////////// 
                        unsigned short logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
                        m_LogArr[logIdx].threadID = thID;
                        m_LogArr[logIdx].isEnqueue = false;         // Dequeue
                        m_LogArr[logIdx].isCommit = false;          // before dequee commit
                        m_LogArr[logIdx].ptr0 = (UINT_PTR)head;     // 현재 바라보고 있는 head
                        m_LogArr[logIdx].ptr1 = (UINT_PTR)next;;    // 현재 바라보고 있는 head의 next
                        ///////////////////////////////////////////////////////
#endif

                        Node* originHeadNext = (Node*)((UINT_PTR)next & mask);
                        t = originHeadNext->data;
                        if (InterlockedCompareExchangePointer((PVOID*)&m_Head, next, head) == head) {
#if defined(LOGGING)
                            ///////////////////////// LOG ///////////////////////// 
                            unsigned short logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
                            m_LogArr[logIdx].threadID = thID;
                            m_LogArr[logIdx].isEnqueue = false;
                            m_LogArr[logIdx].isCommit = true;       // after dequeue commit
                            m_LogArr[logIdx].ptr0 = (UINT_PTR)head;
                            m_LogArr[logIdx].ptr1 = (UINT_PTR)next;
                            ///////////////////////////////////////////////////////
#endif
                        // Node* originHeadNext = (Node*)((UINT_PTR)next & mask);
                        // t = originHeadNext->data;
                        // 다른 스레드 이후 Dequeue하여 originHeadNext에 해당하는 노드를 Free할 수 있음
                        // 이 경우 Free된 노드를 참조하는 셈.
                        // => t 대입을 위쪽으로 올린다.

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

        // m_Size 변수 감소는 실제 Dequeue 커밋 이전에 반영하도록...
        //InterlockedExchangeAdd(&m_Size, -1);
        return true;
    }

#if defined(LOGGING)
    ///////////////////////// LOG ///////////////////////// 
    void PrintLog();
#endif
};

#if defined(LOGGING)
template<typename T>
void LockFreeQueue<T>::PrintLog()
{
    std::lock_guard<std::mutex> lockGurad(g_LogPrintMtx);

    time_t now = time(0);
    struct tm timeinfo;
    char buffer[80];
    localtime_s(&timeinfo, &now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", &timeinfo);
    std::string currentDateTime = std::string(buffer);

    // 파일 경로 생성
    std::string filePath = "./" + currentDateTime + ".txt";

    // 파일 스트림 열기
    std::ofstream outputFile(filePath);

    if (!outputFile) {
        std::cerr << "파일을 열 수 없습니다." << std::endl;
        return;
    }
    
    outputFile << currentDateTime << std::endl;
    //std::lock_guard<std::mutex> lockGuard(g_LogPrintMtx);
    for (size_t idx = 0; idx <= USHRT_MAX; idx++) {
        stLog& log = m_LogArr[idx];
        if (idx != 0 && log.threadID == 0) {
            break;
        }

        outputFile << "------------------------------" << std::endl;
        outputFile << idx << std::endl;
        if (log.isEnqueue) {
            if (!log.isCommit) {
                outputFile << "[BEFORE ENQ COMMIT] ";
                outputFile << "Thread: " << std::hex << log.threadID << std::endl;
                outputFile << "now    tail:     0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr0 << std::endl;
                outputFile << "now    tailNext: 0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr1 << std::endl;
                outputFile << "enqing tailNext: 0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr2 << std::endl;
            }
            else if(log.isCommit && !log.isAllCommit) {
                outputFile  << "[AFTER ENQ FIRST CAS] ";
                outputFile << "Thread: " << std::hex << log.threadID << std::endl;
                outputFile << "now tail:        0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr0 << std::endl;
                outputFile << "now tailNext:    0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr1 << std::endl;
                outputFile << "will be tail:    0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr2 << std::endl;
            }
            else if (log.isCommit && log.isAllCommit) {
                outputFile << "[AFTER ENQ SECOND CAS] ";
                outputFile << "Thread: " << std::hex << log.threadID << std::endl;
                outputFile << "old tail:        0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr0 << std::endl;
                outputFile << "new tail:        0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr2 << std::endl;
            }
        }
        else {
            if (log.ptr3 == 0xFFFF'FFFF'FFFF'FFFF) {
                // Dequeue 실패
                outputFile << "[FAILED DEQ] ";
                outputFile << "Thread: " << std::hex << log.threadID << std::endl;
                outputFile << "now dummy:       0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr0 << std::endl;
                outputFile << "now dummy next:  0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr1 << std::endl;
            }
            else {
                if (!log.isCommit) {
                    outputFile << "[BEFORE DEQ COMMIT] ";
                    outputFile << "Thread: " << std::hex << log.threadID << std::endl;
                    outputFile << "now dummy:       0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr0 << std::endl;
                    outputFile << "now dummy next:  0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr1 << std::endl;
                }
                else {
                    outputFile << "[AFTER DEQ COMMIT] ";
                    outputFile << "Thread: " << std::hex << log.threadID << std::endl;
                    outputFile << "deleted:         0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr0 << std::endl;
                    outputFile << "new dummy:       0x" << std::setw(16) << std::setfill('0') << std::hex << log.ptr1 << std::endl;
                }
            }
        }
    }
    // 파일 닫기
    outputFile.close();

    std::cout << "파일이 생성되었습니다: " << filePath << std::endl;
}
#endif