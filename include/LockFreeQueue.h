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

    Node*   m_Head;        // ���۳�带 ����Ʈ�Ѵ�.
    Node*   m_Tail;        // ��������带 ����Ʈ�Ѵ�.
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
        // => ���� �ʿ�
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

            // [next�� NULL ó���� ���ϸ�?]
            // next�� NULL �񱳰� ���� ù ��° CAS�� �״�� �����غ��� �����Ѵ�. ù ��° CAS�� ���� ������ ����. ������ �� operand�� ���� next�̴�.
            // 
            // 0x1000[����] -> 0xA000[���] -> NULL
            // m_head          m_tail
            //
            // (1) ������A�� ù ��° CAS�� �����ϱ� �� m_Tail ���� ���� ������ ����,  originalTail: 0xA000
            // (2) ������B�� ù ��° CAS�� �����ϰ�, �� ��° CAS ���� ��. ť�� ��Ȳ�� �Ʒ��� ������.
            //     0x1000[����] -> 0xA000[���] -> 0xB000[���] -> NULL
            //     m_head          m_tail (�� ��° CAS ���̹Ƿ� ���� tail�� ������ ����)
            // (3) ������A�� ������ ���� 0xA000 ����� ���� next�� 0xB000���� �а�, ù ��° CAS�� ������. ������ 0xA000 ����� next ���� ���ŵ�
            //     0x1000[����] -> 0xA000[���] -> 0xB000[���] -> NULL
            // -> 0xA000 ����� next ���� 0xB000�� 0xC000[���]�� ����������. �ᱹ 0xB000 ��尡 �����Ǿ���.
            //     0x1000[����] -> 0xA000[���] -> 0xC000[���] -> NULL
            //     m_head          m_tail 
            // (4) ���� ���� ��Ȳ���� ������A�� �ι�° CAS�� ������B�� �ι�° CAS�� �ռ��ٸ�, �ܼ��� ��� �ϳ��� �����Ǵ� �������� ��������(���� �� ��ü�� ����),
            //     ������B�� �ι�° CAS�� ���� ����ȴٸ� ���������� m_Tail�� ������ 0xB000�� ����Ű�� �Ǵ� ��Ȳ�� �ȴ�.
            // 
            // (���)
            // ���� ���Ժο� next == NULL ���ǹ��� �����ϰ� �ʹٸ�, ù ��° CAS �ڵ带 �����ؾ� �Ѵ�.
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
                m_LogArr[logIdx].ptr0 = (UINT_PTR)tail;         // ���� �ٶ󺸰� �ִ� Tail
                m_LogArr[logIdx].ptr1 = (UINT_PTR)next;         // ���� �ٶ󺸰� �ִ� Tail�� next
                m_LogArr[logIdx].ptr2 = (UINT_PTR)managedPtr;   // Enqueue ���� ��� �ּ�
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
                    m_LogArr[logIdx].ptr0 = (UINT_PTR)tail;         // �ٶ󺸰� �ִ� Tail
                    m_LogArr[logIdx].ptr1 = (UINT_PTR)next;         // �ٶ󺸰� �ִ� Tail�� next
                    m_LogArr[logIdx].ptr2 = (UINT_PTR)managedPtr;   // ���ο� Tail�� Next (Tail ������ �̹ݿ�)
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
                    m_LogArr[logIdx].isAllCommit = true;            // �ٶ󺸰� �ִ� Tail
                    m_LogArr[logIdx].ptr0 = (UINT_PTR)tail;         // �ٶ󺸰� �ִ� Tail�� next
                    m_LogArr[logIdx].ptr1 = (UINT_PTR)next;         // ���ο� Tail�� Next (Tail ������ �ݿ� ��)
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
            m_LogArr[logIdx].ptr0 = (UINT_PTR)tail;         // ���� �ٶ󺸰� �ִ� Tail
            m_LogArr[logIdx].ptr1 = (UINT_PTR)next;         // ���� �ٶ󺸰� �ִ� Tail�� next
            m_LogArr[logIdx].ptr2 = (UINT_PTR)managedPtr;   // Enqueue ���� ��� �ּ�
            ///////////////////////////////////////////////////////
            if (InterlockedCompareExchangePointer((PVOID*)&m_Tail, (PVOID)managedPtr, tail) == tail) {
                ///////////////////////// LOG ///////////////////////// 
                logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
                m_LogArr[logIdx].threadID = thID;
                m_LogArr[logIdx].isEnqueue = true;              // Enqueue
                m_LogArr[logIdx].isCommit = true;               // after first CAS Commit
                m_LogArr[logIdx].isAllCommit = false;           // before all CAS
                m_LogArr[logIdx].ptr0 = (UINT_PTR)tail;         // �ٶ󺸰� �ִ� Tail
                m_LogArr[logIdx].ptr1 = (UINT_PTR)next;         // �ٶ󺸰� �ִ� Tail�� next
                m_LogArr[logIdx].ptr2 = (UINT_PTR)managedPtr;   // ���ο� Tail�� Next (Tail ������ �̹ݿ�)
                ///////////////////////////////////////////////////////
                originTail->next = (Node*)managedPtr;
                ///////////////////////// LOG ///////////////////////// 
                logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
                m_LogArr[logIdx].threadID = thID;               // Enqueue
                m_LogArr[logIdx].isEnqueue = true;              // after first CAS Commit
                m_LogArr[logIdx].isCommit = true;               // after all CAS
                m_LogArr[logIdx].isAllCommit = true;            // �ٶ󺸰� �ִ� Tail
                m_LogArr[logIdx].ptr0 = (UINT_PTR)tail;         // �ٶ󺸰� �ִ� Tail�� next
                m_LogArr[logIdx].ptr1 = (UINT_PTR)next;         // ���ο� Tail�� Next (Tail ������ �ݿ� ��)
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

                // Dequeue �� ���� �߻�
                // (��Ȳ)
                // 0xA000[����] -> 0xB000 -> 0xC000 -> NULL
                // m_Head          m_Tail
                // 
                // (1) threadA Dequeue �õ� �� orginHead = 0xA000 ����, next ���� ������ �̷������ ����
                // (2) threadB Dequeue �õ� �� Ŀ�Ա��� �Ϸ� 
                // 0xB000[����] -> 0xC000 -> NULL
                // (3) threadB Enqueue �õ� �� Ŀ�Ա��� �Ϸ�
                // 0xB000[����] -> 0xC000 -> 0xA000 -> NULL
                // (4) threadA�� ���� Dequeue �õ�, ���⼭ ������ ����Ű�� 0xA000�� ��Ȱ��Ǿ� Enqueue�� ����, ���� next �������� NULL�� ���Ե�
                // (5) next�� NULL�̹Ƿ� �Ʒ� ���ǹ��� �ɷ� Dequeue�� ���з� �̾���
                //
                // (�м�)
                // ABA ������ ����ϴ�. ���� ABA �ذ��� ���� ��Ŀ��� Ȱ���ϴ� ��Ʈ ����ŷ ����� �Ʒ� ���ǹ������� Ȱ��� �� ���� ������.
                // �ܼ��� next�� üũ�ϴ� ��Ŀ��� ������ �ʿ���.
                // => head ������ ���� ������ next ������ ���� ������ �������� �������־�� �Ѵ�. ������ ����� �ּҸ� �а�, �� ����� next�� �д� ���� �������־�� �Ѵ�.
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
                //    m_LogArr[logIdx].ptr0 = (UINT_PTR)head;     // ���� �ٶ󺸰� �ִ� head
                //    m_LogArr[logIdx].ptr1 = (UINT_PTR)next;;    // ���� �ٶ󺸰� �ִ� head�� next
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
                //    m_LogArr[logIdx].ptr0 = (UINT_PTR)head;     // ���� �ٶ󺸰� �ִ� head
                //    m_LogArr[logIdx].ptr1 = (UINT_PTR)next;;    // ���� �ٶ󺸰� �ִ� head�� next
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

                // �Ʒ� ���ǹ����� m_head == head�� �����Ѵٴ� ���� �� head�� next�� ������ ��忡�� ���� ���� ����ȴ�.
                // �̹� MSB���� �Ϻο� ���� ��Ʈ�� ��带 �ĺ��ϴ� ����� �� �ֱ� ����.
                if (m_Head == head) {
                    if (next == NULL || next == (Node*)0xFFFF'FFFF'FFFF'FFFF) {
#if defined(LOGGING)
                        ///////////////////////// LOG ///////////////////////// 
                        unsigned short logIdx = InterlockedIncrement16((SHORT*)&m_LogIndex);
                        m_LogArr[logIdx].ptr3 = 0xFFFF'FFFF'FFFF'FFFF;
                        m_LogArr[logIdx].threadID = thID;
                        m_LogArr[logIdx].isEnqueue = false;         // Dequeue
                        m_LogArr[logIdx].isCommit = false;          // before dequee commit
                        m_LogArr[logIdx].ptr0 = (UINT_PTR)head;     // ���� �ٶ󺸰� �ִ� head
                        m_LogArr[logIdx].ptr1 = (UINT_PTR)next;;    // ���� �ٶ󺸰� �ִ� head�� next
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
                        m_LogArr[logIdx].ptr0 = (UINT_PTR)head;     // ���� �ٶ󺸰� �ִ� head
                        m_LogArr[logIdx].ptr1 = (UINT_PTR)next;;    // ���� �ٶ󺸰� �ִ� head�� next
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
                        // �ٸ� ������ ���� Dequeue�Ͽ� originHeadNext�� �ش��ϴ� ��带 Free�� �� ����
                        // �� ��� Free�� ��带 �����ϴ� ��.
                        // => t ������ �������� �ø���.

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
                DebugBreak();  // ť ������ > 0�� Ȯ���Ͽ��⿡ ����ó�� 
                return false;
            }
            else {
                if (next == NULL || next == (Node*)0xFFFF'FFFF'FFFF'FFFF) {
                    DebugBreak();  // ť ������ > 0�� Ȯ���Ͽ��⿡ ����ó�� 
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

        // m_Size ���� ���Ҵ� ���� Dequeue Ŀ�� ������ �ݿ��ϵ���...
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

    // ���� ��� ����
    std::string filePath = "./" + currentDateTime + ".txt";

    // ���� ��Ʈ�� ����
    std::ofstream outputFile(filePath);

    if (!outputFile) {
        std::cerr << "������ �� �� �����ϴ�." << std::endl;
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
                // Dequeue ����
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
    // ���� �ݱ�
    outputFile.close();

    std::cout << "������ �����Ǿ����ϴ�: " << filePath << std::endl;
}
#endif