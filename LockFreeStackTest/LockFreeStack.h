#pragma once
#include <Windows.h>
#include <iostream>

template <typename T>
class LockFreeStack
{
	struct Node {
		T data;
		Node* next;

		Node(T _data) : data(_data) {}
	};

private:
	Node* m_Head;
	short m_Increment;

public:
	LockFreeStack();
	void Push(const T& data);
	bool Pop(T& data);
	size_t GetSize();

private:

};

template<typename T>
inline LockFreeStack<T>::LockFreeStack()
	: m_Head(NULL), m_Increment(0)	{}

template<typename T>
inline void LockFreeStack<T>::Push(const T& data)
{
	//Node* newNode = new Node(data);
	//
	//do {
	//	newNode->next = m_Head;
	//} while (newNode->next != InterlockedCompareExchangePointer((PVOID*)&m_Head, (PVOID)newNode, (PVOID)newNode->next));
	// => ����ȭ �� ������ �߻�

	unsigned long long incrementPart = InterlockedIncrement16(&m_Increment);	// 64 = 16
	incrementPart = incrementPart << (64 - 16);
	Node* newNode = new Node(data);
	PUINT_PTR = new 
	newNode = newNode + incrementPart;
	Node* oldNode;

	do {
		oldNode = m_Head;
		newNode->next = oldNode;
	} while (oldNode != InterlockedCompareExchangePointer((PVOID*)&m_Head, (PVOID)newNode, (PVOID)oldNode));
}

template<typename T>
inline bool LockFreeStack<T>::Pop(T& data)
{
	//Node* head;
	//do {
	//	head = m_Head;
	//											// head->next�� �ּҴ� InterlockedCompareExchangePointer �Լ� ȣ�� ������ ���ڷ� ����ȴ�.
	//											// ���� InterlockedCompareExchangePointer���� �����ϴ� ��� ��ü�� ���ڼ��� ����������,
	//											// head->next�� ���� ���� �� �ִ�. ABA ������ �߻��� ��� head->next�� ������ ����� �� �ִ�. 
	//} while (head && head != InterlockedCompareExchangePointer((PVOID*)&m_Head, (PVOID)head->next, (PVOID)head));

	// ��� �� �� �ڵ忡���� �������� �Ʒ� �ڵ忡���� �������� �����ϴٰ� ������
	Node* head;
	Node* nextNode;
	do {
		head = m_Head;
		nextNode = head->next;
	} while (head && head != InterlockedCompareExchangePointer((PVOID*)&m_Head, (PVOID)nextNode, (PVOID)head));
	
	if (head != NULL) {
		data = head->data;
		delete head;
		return true;
	}
	else {
		return false;
	}
}

template<typename T>
inline size_t LockFreeStack<T>::GetSize()
{
	size_t cnt = 0;
	Node* ptr = m_Head;
	while (ptr != NULL) {
		//std::cout << ptr->data << std::endl;
		cnt++;
		ptr = ptr->next;
	}

	return cnt;
}
