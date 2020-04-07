#include <iostream>
#include <mutex>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>

using namespace std;

const auto NUM_TEST = 4000000;
const auto KEY_RANGE = 1000;

class NODE {
public:
	int key;
	mutex n_l;
	NODE* next;
	bool marked;
	NODE() {
		next = NULL;
		marked = false;
	}
	NODE(int key_value) {
		next = NULL;
		marked = false;
		key = key_value;
	}
	~NODE() {}
	void lock() { n_l.lock(); }
	void unlock() { n_l.unlock(); }
};

class LFNODE;
class MPTR
{
	int value;
public:
	void set(LFNODE* node, bool removed)
	{
		value = reinterpret_cast<int>(node);
		if (true == value)
		{
			value = value | 0x01;
		}
		else
		{
			value = value & 0xFFFFFFFE;
		}
	}
	LFNODE* getptr()
	{
		return reinterpret_cast<LFNODE*>(value & 0xFFFFFFFE);
	}
	LFNODE* getptr(bool* removed)
	{
		int temp = value;
		if (0 == (temp & 0x1))* removed = false;
		else *removed = true;
		return reinterpret_cast<LFNODE*>(temp & 0xFFFFFFFE);
	}
	bool CAS(LFNODE* old_node, LFNODE* new_node,
		bool old_removed, bool new_removed)
	{
		int old_value, new_value;
		old_value = reinterpret_cast<int>(old_node);
		if (true == old_removed) old_value = old_value | 0x01;
		else old_value = old_value & 0xFFFFFFFE;
		new_value = reinterpret_cast<int>(new_node);
		if (true == new_removed) new_value = new_value | 0x01;
		else new_value = new_value & 0xFFFFFFFE;

		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_int*>(&value), &old_value, new_value);
	}
	bool TryMarking(LFNODE* old_node, bool new_removed) {
		int old_value, new_value;
		old_value = reinterpret_cast<int>(old_node);
		old_value = old_value & 0xFFFFFFFE;
		new_value = old_value;
		if (true == new_removed) new_value = new_value | 0x01;

		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_int*>(&value), &old_value, new_value);
	}
};
class LFNODE
{

public:
	int key;
	MPTR next;
	LFNODE() {
		next.set(nullptr, false);
	}
	LFNODE(int key_value) {
		next.set(nullptr, false);
		key = key_value;
	}
	~LFNODE() {}
	bool CAS(int old_v, int new_v) {
		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_int*>(&next), &old_v, new_v);
	}
	bool CAS(LFNODE* old_node, LFNODE* new_node,
		bool oldMark, bool newMark)
	{
		int oldvalue = reinterpret_cast<int>(old_node);
		if (oldMark) oldvalue = oldvalue | 0x01;
		else oldvalue = oldvalue & 0xFFFFFFFE;

		int newvalue = reinterpret_cast<int>(new_node);
		if (newMark) newvalue = newvalue | 0x01;
		else newvalue = newvalue & 0xFFFFFFE;

		return CAS(oldvalue, newvalue);
	}
	bool AttemptMark(LFNODE* old_node, bool newMark) {
		int oldvalue = reinterpret_cast<int>(old_node);
		int newvalue = oldvalue;
		if (newMark) newvalue = newvalue | 0x01;
		else newvalue = newvalue & 0xFFFFFFFE;

		return CAS(oldvalue, newvalue);
	}
};
/*
MPTR AtomicMarkableReference(LFNODE* lf_node, bool b) {

   int value;
   value = reinterpret_cast<int>(lf_node);
   if (true == b)
   {
	  return reinterpret_cast<MPTR>(value | 0x01);
   }
   else
   {
	  return reinterpret_cast<MPTR>(value & 0xFFFFFFFE);
   }
}*/
class LFLIST {
	LFNODE head, tail;
	LFNODE* freelist;
	LFNODE freetail;
	mutex glock;
public:
	LFLIST()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next.set(&tail, false);
		freetail.key = 0x7FFFFFFF;
		freelist = &freetail;
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

	void recycle_freelist() {}

	void find(int key, LFNODE* (&pred), LFNODE* (&curr))
	{
		//bool removed = false;
	retry:
		pred = &head;
		curr = pred->next.getptr();

		while (true)
		{
			bool removed = false;
			LFNODE* succ = curr->next.getptr(&removed);
			while (true == removed)
			{
				//my ver
				//if (false == pred->CAS(curr, succ, false, false))
				//   goto retry;

				if (false == pred->next.CAS(curr, succ, false, false))
					goto retry;
				curr = succ;
				succ = curr->next.getptr(&removed);
			}
			if (curr->key >= key) return;
			pred = curr;
			curr = curr->next.getptr();
			//my ver
			//curr = succ;
		}
	}

	bool Add(int key)
	{
		LFNODE* pred, * curr;
		while (true)
		{
			find(key, pred, curr);
			if (key == curr->key) {
				return false;
			}
			else {
				LFNODE* node = new LFNODE(key);
				node->next.set(curr, false);
				//if (pred->CAS(curr, node, false, false)) {
				//   return true;
				//}

				if (pred->next.CAS(curr, node, false, false)) {
					return true;
				}
				delete node;
			}
		}
	}

	bool Remove(int key)
	{
		LFNODE* pred, * curr;
		bool snip;
		while (true)
		{
			find(key, pred, curr);
			if (curr->key != key) {
				return false;
			}
			else {
				LFNODE* succ = curr->next.getptr();
				/*snip = curr->next.getptr()->AttemptMark(succ, true);*/
				snip = curr->AttemptMark(succ, true);

				if (!snip) continue;

				pred->next.CAS(curr, succ, false, false);
				return true;
			}

		}
	}

	bool Contains(int key)
	{
		LFNODE* curr;
		bool* marked = new bool(false);
		curr = &head;
		while (curr->key < key) {
			curr = curr->next.getptr();
			LFNODE* succ = curr->next.getptr(marked);
		}
		return curr->key == key && !(*marked);
	}

	bool validate(NODE* pred, NODE* curr)
	{
		return !pred->marked && !curr->marked && pred->next == curr;
	}

	void display20()
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
		case 0: key = rand() % KEY_RANGE;
			lflist.Add(key);
			break;
		case 1: key = rand() % KEY_RANGE;
			lflist.Remove(key);
			break;
		case 2: key = rand() % KEY_RANGE;
			lflist.Contains(key);
			break;
		default: cout << "Error\n";
			exit(-1);
		}
	}
}

int main()
{
	for (int num_thread = 1; num_thread <= 16; num_thread *= 2)
	{
		// ÃÊ±âÈ­
		vector<thread> threads;
		lflist.Init();

		auto start_t = chrono::high_resolution_clock::now();

		for (int i = 0; i < num_thread; ++i) threads.emplace_back(ThreadFunc, num_thread);
		for (auto& th : threads) th.join();

		auto end_t = chrono::high_resolution_clock::now();
		auto exec_t = end_t - start_t;

		threads.clear();
		int ex_ms = chrono::duration_cast<chrono::milliseconds>(exec_t).count();

		lflist.display20();
		cout << "Threads [ " << num_thread << " ]";
		cout << ", Exec Time = " << ex_ms << endl;

	}

	system("pause");
}