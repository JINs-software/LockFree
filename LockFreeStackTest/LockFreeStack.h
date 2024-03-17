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

public:
	LockFreeStack();
	void Push(const T& data);
	bool Pop(T& data);

	size_t GetSize();
};

template<typename T>
inline LockFreeStack<T>::LockFreeStack()
	: m_Head(NULL)	{}

template<typename T>
inline void LockFreeStack<T>::Push(const T& data)
{
	//Node* newNode = new Node(data);
	//
	//do {
	//	newNode->next = m_Head;
	//} while (newNode->next != InterlockedCompareExchangePointer((PVOID*)&m_Head, (PVOID)newNode, (PVOID)newNode->next));
	// => 최적화 시 문제가 발생

	Node* newNode = new Node(data);
	Node* oldNode;

	do {
		oldNode = m_Head;
		newNode->next = oldNode;
	} while (oldNode != InterlockedCompareExchangePointer((PVOID*)&m_Head, (PVOID)newNode, (PVOID)oldNode));
}

template<typename T>
inline bool LockFreeStack<T>::Pop(T& data)
{
	Node* head = m_Head;
	while (head && head != InterlockedCompareExchangePointer((PVOID*)&m_Head, (PVOID)head->next, (PVOID)head)) {}
	
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
