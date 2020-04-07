#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

using namespace std;
using namespace chrono;

const auto NUM_TEST = 1000000;
const auto KEY_RANGE = 1000;
///////////////////////////// 실습 23 ///////////////////////////////////////
class LFNODE;

class MPTR {
	int value;
public:
	void Set(LFNODE* node, bool removed) {
		value = reinterpret_cast<int>(node);
		if (removed) 
			value = value | 0x01;
		else 
			value = value & 0xFFFFFFFE;
	}
	LFNODE* getptr() {
		return reinterpret_cast<LFNODE*>(value & 0xFFFFFFFE);
	}
	LFNODE* getptr(bool* removed) {
		int temp = value;
		if (0 == (temp & 0x1)) *removed = false;
		else *removed = true;
		return reinterpret_cast<LFNODE*>(temp & 0xFFFFFFFE);
	}
	bool CAS(LFNODE* old_node, LFNODE* new_node, bool old_removed, bool new_removed) {
		int old_value, new_value;

		old_value = reinterpret_cast<int>(old_node);
		if (true == old_removed) old_value = old_value | 0x01;
		else old_value = old_value & 0xFFFFFFFE;

		new_value = reinterpret_cast<int>(new_node);
		if (true == new_removed) new_value = new_value | 0x01;
		else new_value = new_value & 0xFFFFFFFE;

		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int*>(&value), &old_value, new_value);
	}
	bool TryMarking(LFNODE* old_node, bool new_removed) {
		int old_value, new_value;

		old_value = reinterpret_cast<int>(old_node);
		old_value = old_value & 0xFFFFFFFE;

		new_value = old_value;
		if (true == new_removed) new_value = new_value | 0x01;

		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int*>(&value), &old_value, new_value);
	}
};
class LFNODE {
public:
	int key;
	//LFNODE* next;
	MPTR next;
	
	LFNODE() {
		next.Set(nullptr, false);
	}
	LFNODE(int key_value) {
		next.Set(nullptr, false);
		key = key_value;
	}
	~LFNODE() {}

};

typedef LFNODE* MarkableReference;

// 비멈춤 동기화 (non-blocking synchronization)
// lock을 사용하지 않는다.

class LFLIST {
	//shared_ptr<SPNODE> head, tail;
	LFNODE head, tail;
	LFNODE* freelist;
	mutex fl_mutex;
public:
	LFLIST()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next.Set(&tail, false);
		//head.next = tail;
	}
	~LFLIST() {}
	void Init()
	{
		LFNODE* ptr;
		while (head.next.getptr() != &tail) {
			ptr = head.next.getptr();
			head.next = head.next.getptr()->next;
			delete ptr;
		}
	}

	void recycle_freelist() {

	}

	void find(int key, LFNODE *(&pred), LFNODE *(&curr)) {
		retry:
		pred = &head;
		curr = pred->next.getptr();
		while (true) {
			bool removed{ false };
			LFNODE* succ = curr->next.getptr(&removed);
			while (true == removed) {
				if (false == pred->next.CAS(curr, succ, false, false))
					goto retry;
				curr = succ;
				succ = curr->next.getptr(&removed);
			}
			if (curr->key >= key)
				return;
			pred = curr;
			curr = curr->next.getptr();
		}
	}

	bool Add(int key)
	{
		LFNODE *pred, *curr;
		while (true) {
			find(key, pred, curr);
			if (curr->key == key) {
				return false;
			}
			else {
				LFNODE* node = new LFNODE(key);
				node->next.Set(curr, false);
				if (pred->next.CAS(curr, node, false, false))
					return true;
				delete node;
			}
		}
	}
	bool Remove(int key)
	{
		LFNODE *pred, *curr;
		bool snip;
		while (true) {
			find(key, pred, curr);
			if (curr->key != key)
				return false;
			else {
				LFNODE* succ = curr->next.getptr();
				snip = curr->next.TryMarking(succ, true);
				if (false == snip)
					continue;
				pred->next.CAS(curr, succ, false, false);
				return true;
			}
		}
	}
	bool Contains(int key)
	{
		LFNODE *curr = &head;
		bool marked = false;
		while (curr->key < key) {
			curr = curr->next.getptr();
			LFNODE* succ = curr->next.getptr(&marked);
		}
		return curr->key == key && !marked;
	}

	void Display()
	{
		int c = 20;
		LFNODE* p = head.next.getptr();
		while (p != &tail)
		{
			cout << p->key << ", ";
			p = p->next.getptr();
			c--;
			if (c == 0) break;
		}
	}
};

LFLIST lflist;

void ThreadFunc(int num_thread)
{
	int key;
	for (int i = 0; i < NUM_TEST / num_thread; i++) {
		switch (rand() % 3) {
		case 0:
			key = rand() % KEY_RANGE;
			lflist.Add(key);
			break;
		case 1:
			key = rand() % KEY_RANGE;
			lflist.Remove(key);
			break;
		case 2:
			key = rand() % KEY_RANGE;
			lflist.Contains(key);
			break;
		default:
			cout << "error\n";
			exit(-1);
		}
	}


}

int main()
{
	for (int num_thread = 1; num_thread <= 16; num_thread *= 2) {
		lflist.Init();
		vector<thread> threads;

		auto s = high_resolution_clock::now();
		for (int i = 0; i < num_thread; ++i) {
			threads.emplace_back(ThreadFunc, num_thread);
		}
		for (auto& th : threads) th.join();
		auto e = high_resolution_clock::now();
		cout << "threads[" << num_thread << "]  ";
		cout << duration_cast<milliseconds>(e - s).count() << "ms\t";
		lflist.Display();
		cout << endl;

	}
	system("pause");

	return 0;
}