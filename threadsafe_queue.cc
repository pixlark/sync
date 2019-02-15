#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct TS_Node {
	int data;
	TS_Node * prev;
};

struct TS_Queue {
	TS_Node * head = NULL;
	TS_Node * tail = NULL;
	void add(int item)
	{
		if (!head || !tail) {
			assert(!head && !tail);
			TS_Node * node = (TS_Node*) malloc(sizeof(TS_Node));
			node->data = item;
			node->prev = NULL;
			head = node;
			tail = node;
		} else {
			TS_Node * node = (TS_Node*) malloc(sizeof(TS_Node));
			node->prev = NULL;
			tail->prev = node;
			tail = node;
		}
	}
	int take()
	{
		assert(!empty());
		int ret = head->data;
		head = head->prev;
		if (!head) {
			tail = NULL;
		}
		return ret;
	}
	bool empty()
	{
		if (!head || !tail) {
			assert(!head && !tail);
			return true;
		}
		return false;
	}
};

int main()
{
	TS_Queue queue;
	queue.add(3);
	queue.add(2);
	queue.add(1);
	TS_Node * node = queue.head;
	while (node) {
		printf("%p -> %p\n  %d\n", node, node->prev, node->data);
		node = node->prev;
	}
	return 0;
}
