#pragma once
#include <Windows.h>

template <typename T>
class LockFreeStack
{
	struct Node {
		T data;
		Node* next;
	};

private:
	Node* m_Head;

public:
	LockFreeStack();
	void Push(const T& data);
	bool Pop(T& data);
};

template<typename T>
inline LockFreeStack<T>::LockFreeStack()
	: m_Head(NULL)	{}

template<typename T>
inline void LockFreeStack<T>::Push(const T& data)
{
	Node* newNode = new Node;
	newNode->data = data;

	do {
		newNode->next = m_Head;
	} while (!InterlockedCompareExchange((unsigned long long*)&m_Head, (unsigned long long)newNode, (unsigned long long)newNode->next));
}

template<typename T>
inline bool LockFreeStack<T>::Pop(T& data)
{
	Node* head = m_Head;
	while (head && !InterlockedCompareExchange((unsigned long long*)&m_Head, (unsigned long long)head->next, (unsigned long long)head)) {}
	
	if (head != NULL) {
		data = head->data;
		//delete head;
		return true;
	}
	else {
		return false;
	}
}
