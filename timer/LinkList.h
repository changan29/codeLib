#ifndef LINKLIST_H_
#define LINKLIST_H_
#include <cstdlib>
#include <stdint.h>

template <class T>
class LinkList
{
public:


	class Node
	{
	public:
		T t;
		Node* next;
	};

	LinkList():count(0),head(NULL),tail(NULL)
	{}


	Node* Serve();
	uint32_t Size();
	void PushBack(Node* node);

private:

	uint32_t count;
	Node* head;
	Node* tail;
};

template<class T>
inline void LinkList<T>::PushBack(Node* node)
{
	if (head == NULL)
	{
		head = node;
		tail = node;
	}
	else
	{
		tail->next = node;
		tail = node;
	}
	node->next = NULL;
	count++;

}

template<class T>
inline typename LinkList<T>::Node* LinkList<T>::Serve()
{
	Node* p = NULL;
	if (count > 0)
	{
		p = head;
		count--;
		if (count == 0)
		{
			tail = NULL;
		}
		head = head->next;
	}
	return p;

}

template<class T>
inline uint32_t LinkList<T>::Size()
{
	return count;
}


#endif /* LINKLIST_H_ */
