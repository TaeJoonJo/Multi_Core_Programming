#include <mutex>
#include <thread>
#include <iostream>
#include <chrono>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>

using namespace std;
using namespace chrono;

static const int NUM_TEST = 4000000;
static const int RANGE = 1000;
static const int MAX_LEVEL = 10;

class null_mutex
{
public:
	void lock() {}
	void unlock() {}
};

class NODE {
public:	
	int key;
	NODE *next;
	mutex nlock;
	bool removed;

	NODE() {
		next = nullptr;
		removed = false;
	}
	NODE(int x) {
		key = x;
		next = nullptr;
		removed = false;
	}
	~NODE() {
	}
	void Lock() 
	{ 
		nlock.lock(); 
	}
	void Unlock()
	{
		nlock.unlock();
	}
};

class SPNODE {
public:
	int key;
	shared_ptr<SPNODE> next;
	mutex nlock;
	bool removed;

	SPNODE() {
		next = nullptr;
		removed = false;
	}
	SPNODE(int x) {
		key = x;
		next = nullptr;
		removed = false;
	}
	~SPNODE() {
	}
	void Lock()
	{
		nlock.lock();
	}
	void Unlock()
	{
		nlock.unlock();
	}
};

class LFNODE {
public:
	int key;
	unsigned next;

	LFNODE() {
		next = 0;
	}
	LFNODE(int x) {
		key = x;
		next = 0;
	}
	~LFNODE() {
	}
	LFNODE *GetNext() {
		return reinterpret_cast<LFNODE *>(next & 0xFFFFFFFE);
	}

	void SetNext(LFNODE *ptr) {
		next = reinterpret_cast<unsigned>(ptr);
	}

	LFNODE *GetNextWithMark(bool *mark) {
		int temp = next;
		*mark = (temp % 2) == 1;
		return reinterpret_cast<LFNODE *>(temp & 0xFFFFFFFE);
	}

	bool CAS(int old_value, int new_value)
	{
		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_int *>(&next), 
			&old_value, new_value);
	}

	bool CAS(LFNODE *old_next, LFNODE *new_next, bool old_mark, bool new_mark) {
		unsigned old_value = reinterpret_cast<unsigned>(old_next);
		if (old_mark) old_value = old_value | 0x1;
		else old_value = old_value & 0xFFFFFFFE;
		unsigned new_value = reinterpret_cast<unsigned>(new_next);
		if (new_mark) new_value = new_value | 0x1;
		else new_value = new_value & 0xFFFFFFFE;
		return CAS(old_value, new_value);
	}

	bool TryMark(LFNODE *ptr)
	{
		unsigned old_value = reinterpret_cast<unsigned>(ptr) & 0xFFFFFFFE;
		unsigned new_value = old_value | 1;
		return CAS(old_value, new_value);
	}

	bool IsMarked() {
		return (0 != (next & 1));
	}
};

class CSET
{
	NODE head, tail;
	null_mutex  g_lock;
public:
	CSET()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next = &tail;
	}
	void Init()
	{
		while (head.next != &tail) {
			NODE *temp = head.next;
			head.next = temp->next;
			delete temp;
		}
	}
	void Dump()
	{
		NODE *ptr = head.next;
		cout << "Result Contains : ";
		for (int i = 0; i < 20; ++i) {
			cout << ptr->key << ", ";
			if (&tail == ptr) break;
			ptr = ptr->next;
		}
		cout << endl;
	}
	bool Add(int x)
	{
		NODE *pred, *curr;
		pred = &head;
		g_lock.lock();
		curr = pred->next;
		while (curr->key < x) {
			pred = curr;
			curr = curr->next;
		}

		if (curr->key == x) {
			g_lock.unlock();
			return false;
		}
		else {
			NODE *e = new NODE{ x };
			e->next = curr;
			pred->next = e;
			g_lock.unlock();
			return true;
		}
	}
	bool Remove(int x)
	{
		NODE *pred, *curr;
		pred = &head;
		g_lock.lock();
		curr = pred->next;
		while (curr->key < x) {
			pred = curr;
			curr = curr->next;
		}

		if (curr->key != x) {
			g_lock.unlock();
			return false;
		}
		else {
			pred->next = curr->next;
			delete curr;
			g_lock.unlock();
			return true;
		}
	}
	bool Contains(int x)
	{
		NODE *pred, *curr;
		pred = &head;
		g_lock.lock();
		curr = pred->next;
		while (curr->key < x) {
			pred = curr;
			curr = curr->next;
		}

		if (curr->key == x) {
			g_lock.unlock();
			return true;
		}
		else {
			g_lock.unlock();
			return false;
		}
	}
};

class FSET
{
	NODE head, tail;
public:
	FSET()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next = &tail;
	}
	void Init()
	{
		while (head.next != &tail) {
			NODE *temp = head.next;
			head.next = temp->next;
			delete temp;
		}
	}
	void Dump()
	{
		NODE *ptr = head.next;
		cout << "Result Contains : ";
		for (int i = 0; i < 20; ++i) {
			cout << ptr->key << ", ";
			if (&tail == ptr) break;
			ptr = ptr->next;
		}
		cout << endl;
	}
	bool Add(int x)
	{
		NODE *pred, *curr;
		head.Lock();
		pred = &head;
		curr = pred->next;
		curr->Lock();
		while (curr->key < x) {
			pred->Unlock();
			pred = curr;
			curr = curr->next;
			curr->Lock();
		}

		if (curr->key == x) {
			curr->Unlock();
			pred->Unlock();
			return false;
		}
		else {
			NODE *e = new NODE{ x };
			e->next = curr;
			pred->next = e;
			curr->Unlock();
			pred->Unlock();
			return true;
		}
	}
	bool Remove(int x)
	{
		NODE *pred, *curr;
		head.Lock();
		pred = &head;
		curr = pred->next;
		curr->Lock();
		while (curr->key < x) {
			pred->Unlock();
			pred = curr;
			curr = curr->next;
			curr->Lock();
		}

		if (curr->key != x) {
			curr->Unlock();
			pred->Unlock();
			return false;
		}
		else {
			pred->next = curr->next;
			curr->Unlock();
			pred->Unlock();
			delete curr;
			return true;
		}
	}
	bool Contains(int x)
	{
		NODE *pred, *curr;
		head.Lock();
		pred = &head;
		curr = pred->next;
		curr->Lock();
		while (curr->key < x) {
			pred->Unlock();
			pred = curr;
			curr = curr->next;
			curr->Lock();
		}

		if (curr->key == x) {
			curr->Unlock();
			pred->Unlock();
			return true;
		}
		else {
			curr->Unlock();
			pred->Unlock();
			return false;
		}
	}
};

class OSET
{
	NODE head, tail;
public:
	OSET()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next = &tail;
	}
	void Init()
	{
		while (head.next != &tail) {
			NODE *temp = head.next;
			head.next = temp->next;
			delete temp;
		}
	}
	void Dump()
	{
		NODE *ptr = head.next;
		cout << "Result Contains : ";
		for (int i = 0; i < 20; ++i) {
			cout << ptr->key << ", ";
			if (&tail == ptr) break;
			ptr = ptr->next;
		}
		cout << endl;
	}
	bool Validate(NODE *pred, NODE *curr)
	{
		NODE *ptr = &head;
		while (ptr->key <= pred->key) {
			if (ptr == pred) return pred->next == curr;
			ptr = ptr->next;
		}
		return false;
	}
	bool Add(int x)
	{
		NODE *pred, *curr;
		while (true) {
			pred = &head;
			curr = pred->next;
			while (curr->key < x) {
				pred = curr; curr = curr->next;
			}
			pred->Lock(); curr->Lock();

			if (false == Validate(pred, curr)) {
				curr->Unlock();
				pred->Unlock();
				continue;
			}

			if (curr->key == x) {
				curr->Unlock();
				pred->Unlock();
				return false;
			}
			else {
				NODE *e = new NODE{ x };
				e->next = curr;
				pred->next = e;
				curr->Unlock();
				pred->Unlock();
				return true;
			}
		}
	}
	bool Remove(int x)
	{
		NODE *pred, *curr;
		while (true) {
			pred = &head;
			curr = pred->next;
			while (curr->key < x) {
				pred = curr; curr = curr->next;
			}
			pred->Lock(); curr->Lock();

			if (false == Validate(pred, curr)) {
				curr->Unlock();
				pred->Unlock();
				continue;
			}
			if (curr->key != x) {
				curr->Unlock();
				pred->Unlock();
				return false;
			}
			else {
				pred->next = curr->next;
				curr->Unlock();
				pred->Unlock();
				return true;
			}
		}
	}
	bool Contains(int x)
	{
		NODE *pred, *curr;
		while (true) {
			pred = &head;
			curr = pred->next;
			while (curr->key < x) {
				pred = curr; curr = curr->next;
			}

			pred->Lock(); curr->Lock();
			if (false == Validate(pred, curr)) {
				curr->Unlock(); pred->Unlock();
				continue;
			}

			if (curr->key == x) {
				curr->Unlock();
				pred->Unlock();
				return true;
			}
			else {
				curr->Unlock();
				pred->Unlock();
				return false;
			}
		}
	}
};

class NodeManager
{
	NODE *first, *second;
	mutex f_m, s_m;
public:
	NodeManager()
	{
		first = second = nullptr;
	}
	~NodeManager()
	{
		while (nullptr != first) {
			NODE *p = first;
			first = first->next;
			delete p;
		}
		while (nullptr != second) {
			NODE *p = second;
			second = second->next;
			delete p;
		}
	}
	NODE *GetNode(int x)
	{
		f_m.lock();
		if (nullptr == first) {
			f_m.unlock();
			return new NODE{ x };
		}
		NODE *p = first;
		first = first->next;
		f_m.unlock();
		p->key = x;
		p->next = nullptr;
		p->removed = false;
		return p;
	}
	void FreeNode(NODE *e)
	{
		s_m.lock();
		e->next = second;
		second = e;
		s_m.unlock();
	}
	void Recycle()
	{
		NODE *p = first;
		first = second;
		second = p;
	}
};

NodeManager node_pool;

class ZSET
{
	NODE head, tail;
public:
	ZSET()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next = &tail;
	}
	void Init()
	{
		while (head.next != &tail) {
			NODE *temp = head.next;
			head.next = temp->next;
			delete temp;
		}
	}
	void Dump()
	{
		NODE *ptr = head.next;
		cout << "Result Contains : ";
		for (int i = 0; i < 20; ++i) {
			cout << ptr->key << ", ";
			if (&tail == ptr) break;
			ptr = ptr->next;
		}
		cout << endl;
	}
	bool Validate(NODE *pred, NODE *curr)
	{
		return (false == pred->removed) && (false == curr->removed) && (pred->next == curr);
	}
	bool Add(int x)
	{
		NODE *pred, *curr;
		while (true) {
			pred = &head;
			curr = pred->next;
			while (curr->key < x) {
				pred = curr; curr = curr->next;
			}
			pred->Lock(); curr->Lock();

			if (false == Validate(pred, curr)) {
				curr->Unlock();
				pred->Unlock();
				continue;
			}

			if (curr->key == x) {
				curr->Unlock();
				pred->Unlock();
				return false;
			}
			else {
				NODE *e = node_pool.GetNode(x);
				e->next = curr;
				pred->next = e;
				curr->Unlock();
				pred->Unlock();
				return true;
			}
		}
	}
	bool Remove(int x)
	{
		NODE *pred, *curr;
		while (true) {
			pred = &head;
			curr = pred->next;
			while (curr->key < x) {
				pred = curr; curr = curr->next;
			}
			pred->Lock(); curr->Lock();

			if (false == Validate(pred, curr)) {
				curr->Unlock();
				pred->Unlock();
				continue;
			}
			if (curr->key != x) {
				curr->Unlock();
				pred->Unlock();
				return false;
			}
			else {
				curr->removed = true;
				pred->next = curr->next;
				curr->Unlock();
				pred->Unlock();
				node_pool.FreeNode(curr);
				return true;
			}
		}
	}
	bool Contains(int x)
	{
		NODE *curr;
		curr = &head;
		while (curr->key < x) {
			curr = curr->next;
		}

		return (false == curr->removed) && (x == curr->key);
	}
};

class SPZSET
{
	shared_ptr <SPNODE> head, tail;
public:
	SPZSET()
	{
		head = make_shared <SPNODE>( 0x80000000 );
		tail = make_shared <SPNODE>(0x7FFFFFFF);
		head->next = tail;
	}
	void Init()
	{
		head->next = tail;
	}
	void Dump()
	{
		shared_ptr<SPNODE> ptr = head->next;
		cout << "Result Contains : ";
		for (int i = 0; i < 20; ++i) {
			cout << ptr->key << ", ";
			if (nullptr == ptr) break;
			ptr = ptr->next;
		}
		cout << endl;
	}
	bool Validate(shared_ptr<SPNODE> pred, shared_ptr<SPNODE> curr)
	{
		return (false == pred->removed) && (false == curr->removed) && (pred->next == curr);
	}
	bool Add(int x)
	{
		shared_ptr <SPNODE> pred, curr;
		while (true) {
			pred = head;
			curr = pred->next;
			while (curr->key < x) {
				pred = curr; curr = curr->next;
			}
			pred->Lock(); curr->Lock();

			if (false == Validate(pred, curr)) {
				curr->Unlock();
				pred->Unlock();
				continue;
			}

			if (curr->key == x) {
				curr->Unlock();
				pred->Unlock();
				return false;
			}
			else {
				shared_ptr<SPNODE> e = make_shared<SPNODE>(x);
				e->next = curr;
				pred->next = e;
				curr->Unlock();
				pred->Unlock();
				return true;
			}
		}
	}
	bool Remove(int x)
	{
		shared_ptr<SPNODE> pred, curr;
		while (true) {
			pred = head;
			curr = pred->next;
			while (curr->key < x) {
				pred = curr; curr = curr->next;
			}
			pred->Lock(); curr->Lock();

			if (false == Validate(pred, curr)) {
				curr->Unlock();
				pred->Unlock();
				continue;
			}
			if (curr->key != x) {
				curr->Unlock();
				pred->Unlock();
				return false;
			}
			else {
				curr->removed = true;
				pred->next = curr->next;
				curr->Unlock();
				pred->Unlock();
				return true;
			}
		}
	}
	bool Contains(int x)
	{
		shared_ptr<SPNODE> curr;
		curr = head;
		while (curr->key < x) {
			curr = curr->next;
		}
		return (false == curr->removed) && (x == curr->key);
	}
};

class LFSET
{
	LFNODE head, tail;
public:
	LFSET()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.SetNext(&tail);
	}
	void Init()
	{
		while (head.GetNext() != &tail) {
			LFNODE *temp = head.GetNext();
			head.next = temp->next;
			delete temp;
		}
	}

	void Dump()
	{
		LFNODE *ptr = head.GetNext();
		cout << "Result Contains : ";
		for (int i = 0; i < 20; ++i) {
			cout << ptr->key << ", ";
			if (&tail == ptr) break;
			ptr = ptr->GetNext();
		}
		cout << endl;
	}
	
	void Find(int x, LFNODE **pred, LFNODE **curr)
	{
	retry:
		LFNODE *pr = &head;
		LFNODE *cu = pr->GetNext();
		while (true) {
			bool removed;
			LFNODE *su = cu->GetNextWithMark(&removed);
			while (true == removed) {
				if (false == pr->CAS(cu, su, false, false))
					goto retry;
				cu = su;
				su = cu->GetNextWithMark(&removed);
			}
			if (cu->key >= x) {
				*pred = pr; *curr = cu;
				return;
			}
			pr = cu;
			cu = cu->GetNext();
		}
	}
	bool Add(int x)
	{
		LFNODE *pred, *curr;
		while (true) {
			Find(x, &pred, &curr);

			if (curr->key == x) {
				return false;
			}
			else {
				LFNODE *e = new LFNODE(x);
				e->SetNext(curr);
				if (false == pred->CAS(curr, e, false, false))
					continue;
				return true;
			}
		}
	}
	bool Remove(int x)
	{
		LFNODE *pred, *curr;
		while (true) {
			Find(x, &pred, &curr);

			if (curr->key != x) {
				return false;
			}
			else {
				LFNODE *succ = curr->GetNext();
				if (false == curr->TryMark(succ)) continue;
				pred->CAS(curr, succ, false, false);
				return true;
			}
		}
	}
	bool Contains(int x)
	{
		LFNODE *curr = &head;
		while (curr->key < x) {
			curr = curr->GetNext();
		}

		return (false == curr->IsMarked()) && (x == curr->key);
	}
};

class SKNODE
{
public:
	int value;
	SKNODE *next[MAX_LEVEL];
	int top_level;
	SKNODE()
	{
		value = 0;
		for (auto i = 0; i < MAX_LEVEL; ++i)
			next[i] = nullptr;
		top_level = 0;
	}
	SKNODE(int x)
	{
		value = x;
		for (auto i = 0; i < MAX_LEVEL; ++i)
			next[i] = nullptr;
		top_level = 0;
	}
	SKNODE(int x, int top)
	{
		value = x;
		for (auto i = 0; i < MAX_LEVEL; ++i)
			next[i] = nullptr;
		top_level = top;
	}
};

class CSKSET
{
	SKNODE head, tail;
	mutex  g_lock;
public:
	CSKSET()
	{
		head.value = 0x80000000;
		head.top_level = MAX_LEVEL - 1;
		tail.value = 0x7FFFFFFF;
		tail.top_level = MAX_LEVEL - 1;
		for (int i = 0; i < MAX_LEVEL; ++i)
			head.next[i] = &tail;
	}
	void Init()
	{
		while (head.next[0] != &tail) {
			SKNODE *temp = head.next[0];
			head.next[0] = temp->next[0];
			delete temp;
		}
		for (int i = 1; i < MAX_LEVEL; ++i)
			head.next[i] = &tail;
	}
	void Dump()
	{
		SKNODE *ptr = head.next[0];
		cout << "Result Contains : ";
		for (int i = 0; i < 20; ++i) {
			cout << ptr->value << ", ";
			if (&tail == ptr) break;
			ptr = ptr->next[0];
		}
		cout << endl;
	}
	void Find(int x, SKNODE* preds[], SKNODE* currs[])
	{
		preds[MAX_LEVEL - 1] = &head;
		for (int level = MAX_LEVEL - 1; level >= 0; --level) {
			currs[level] = preds[level]->next[level];
			while (currs[level]->value < x) {
				preds[level] = currs[level];
				currs[level] = currs[level]->next[level];
			}
			if (0 != level) preds[level - 1] = preds[level];
		}
	}
	bool Add(int x)
	{
		SKNODE *pred[MAX_LEVEL], *curr[MAX_LEVEL];
		g_lock.lock();

		Find(x, pred, curr);

		if (curr[0]->value == x) {
			g_lock.unlock();
			return false;
		}
		else {
			int top = 0;
			while ((rand() % 2) == 1)
			{
				top++;
				if (top >= MAX_LEVEL - 1) break;
			}
			SKNODE *e = new SKNODE{ x, top };
			for (int i = 0; i <= top; ++i) {
				e->next[i] = curr[i];
				pred[i]->next[i] = e;
			}
			g_lock.unlock();
			return true;
		}
	}
	bool Remove(int x)
	{
		SKNODE *pred[MAX_LEVEL], *curr[MAX_LEVEL];
		g_lock.lock();

		Find(x, pred, curr);

		if (curr[0]->value != x) {
			g_lock.unlock();
			return false;
		}
		else {
			for (int i = 0; i <= curr[0]->top_level; ++i) {
				pred[i]->next[i] = curr[0]->next[i];
			}
			g_lock.unlock();
			return true;
		}
	}
	bool Contains(int x)
	{
		SKNODE *pred[MAX_LEVEL], *curr[MAX_LEVEL];
		g_lock.lock();

		Find(x, pred, curr);

		if (curr[0]->value == x) {
			g_lock.unlock();
			return true;
		}
		else {
			g_lock.unlock();
			return false;
		}
	}
};

class ZSKNODE{
public:

	int key;
	ZSKNODE * volatile next[MAX_LEVEL];
	int toplevel;
	volatile bool marked;
	volatile bool fullylinked;
	recursive_mutex nodelock;

	ZSKNODE() {
		for (int i = 0; i < MAX_LEVEL; ++i) next[i] = nullptr;
		toplevel = MAX_LEVEL;
		marked = false;
		fullylinked = false;
	}

	ZSKNODE(int key_value) {
		key = key_value;
		for (int i = 0; i < MAX_LEVEL; ++i) next[i] = nullptr;
		toplevel = MAX_LEVEL;
		marked = fullylinked = false;
	}

	ZSKNODE(int key_value, int top) {
		key = key_value;
		for (int i = 0; i < MAX_LEVEL; ++i) next[i] = nullptr;
		toplevel = top;
		marked = fullylinked = false;
	}
	~ZSKNODE() {
	}
};

class ZSKSET {
	ZSKNODE head, tail;
public:
	ZSKSET()
	{
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		for (int i = 0; i < MAX_LEVEL; ++i)
			head.next[i] = &tail;
	}
	~ZSKSET() {
		Init();
	}

	void Init()
	{
		ZSKNODE *ptr;
		while (head.next[0] != &tail) {
			ptr = head.next[0];
			head.next[0] = head.next[0]->next[0];
			delete ptr;
		}
		for (int i = 1; i < MAX_LEVEL; ++i)
			head.next[i] = &tail;
	}

	int Find(int key, ZSKNODE *preds[], ZSKNODE *currs[])
	{
		int firstfound = -1;

		for (int level = MAX_LEVEL - 1; level >= 0; --level)
		{
			if (level == (MAX_LEVEL - 1)) preds[level] = &head;
			else preds[level] = preds[level + 1];
			currs[level] = preds[level]->next[level];
			while (currs[level]->key < key)
			{
				preds[level] = currs[level];
				currs[level] = currs[level]->next[level];
			}
			if ((firstfound == -1) && (currs[level]->key == key)) firstfound = level;
		}
		return firstfound;
	}

	bool Add(int key)
	{
		ZSKNODE *preds[MAX_LEVEL];
		ZSKNODE *currs[MAX_LEVEL];

		int toplevel = 0;
		while ((rand() % 2) == 1)
		{
			toplevel++;
			if (toplevel >= MAX_LEVEL - 1) break;
		}

		while (true) {
			int lFound = Find(key, preds, currs);
			if (-1 != lFound) {
				ZSKNODE *nodeFound = currs[lFound];
				if (false == nodeFound->marked) {
					while (false == nodeFound->fullylinked);
					return false;
				}
				continue;
			}
			int highestLocked = -1;

			ZSKNODE *pred, *curr;
			bool valid = true;
			for (int level = 0; valid && (level <= toplevel); level++) {
				pred = preds[level];
				curr = currs[level];
				pred->nodelock.lock();
				highestLocked = level;
				valid = ((false == pred->marked) && (false == curr->marked) && (pred->next[level] == curr));
			}
			if (false == valid) {
				for (int i = 0; i <= highestLocked; ++i) preds[i]->nodelock.unlock();
				continue;
			}
			ZSKNODE *node = new ZSKNODE(key, toplevel);
			for (int level = 0; level <= toplevel; level++)
				node->next[level] = currs[level];
			for (int level = 0; level <= toplevel; level++)
				preds[level]->next[level] = node;
			node->fullylinked = true;
			for (int i = 0; i <= highestLocked; ++i) preds[i]->nodelock.unlock();
			return true;
		}
	}

	bool Remove(int key)
	{
		ZSKNODE *preds[MAX_LEVEL];
		ZSKNODE *currs[MAX_LEVEL];

		ZSKNODE *victim = nullptr;
		bool isMarked = false;
		int topLevel = -1;

		while (true) {
			int lFound = Find(key, preds, currs);
			if (-1 != lFound) victim = currs[lFound];
			if (isMarked || ((-1 != lFound)
				&& (true == victim->fullylinked)
				&& (victim->toplevel == lFound)
				&& (false == victim->marked))) {
				if (false == isMarked) {
					topLevel = victim->toplevel;
					victim->nodelock.lock();
					if (true == victim->marked) {
						victim->nodelock.unlock();
						return false;
					}
					victim->marked = true;
					isMarked = true;
				}
				int highestLocked = -1;

				ZSKNODE *pred;
				bool valid = true;
				for (int level = 0; valid && (level <= topLevel); ++level) {
					pred = preds[level];
					pred->nodelock.lock();
					highestLocked = level;
					valid = (false == pred->marked) && (pred->next[level] == victim);
				}
				if (false == valid) {
					for (int i = 0; i <= highestLocked; ++i) preds[i]->nodelock.unlock();
					continue;
				}
				for (int level = topLevel; level >= 0; --level)
				{
					preds[level]->next[level] = victim->next[level];
				}
				victim->nodelock.unlock();
				for (int i = 0; i <= highestLocked; ++i) preds[i]->nodelock.unlock();
				return true;
			}
			else return false;
		}
	}

	bool Contains(int key)
	{
		ZSKNODE *preds[MAX_LEVEL];
		ZSKNODE *currs[MAX_LEVEL];

		int lFound = Find(key, preds, currs);

		if (-1 == lFound) return false;
		return (false == currs[lFound]->marked) && (true == currs[lFound]->fullylinked);
	}

	void Dump()

	{
		ZSKNODE *ptr = head.next[0];
		for (int i = 0; i < 40; ++i)
		{
			if (&tail == ptr) break;
			cout << ptr->key << "[" << ptr->toplevel << "]-";
			ptr = ptr->next[0];
		}
		cout << endl;
	}
};


class LFSKNode;

bool Marked(LFSKNode *curr)
{
	int add = reinterpret_cast<int> (curr);
	return ((add & 0x1) == 0x1);
}

LFSKNode* GetReference(LFSKNode *curr)
{
	int addr = reinterpret_cast<int> (curr);
	return reinterpret_cast<LFSKNode *>(addr & 0xFFFFFFFE);
}

LFSKNode* Get(LFSKNode *curr, bool *marked)
{
	int addr = reinterpret_cast<int> (curr);
	*marked = ((addr & 0x01) != 0);
	return reinterpret_cast<LFSKNode *>(addr & 0xFFFFFFFE);
}

LFSKNode* AtomicMarkableReference(LFSKNode *node, bool mark)
{
	int addr = reinterpret_cast<int>(node);
	if (mark)
		addr = addr | 0x1;
	else
		addr = addr & 0xFFFFFFFE;
	return reinterpret_cast<LFSKNode *>(addr);
}

LFSKNode* Set(LFSKNode *node, bool mark)
{
	int addr = reinterpret_cast<int>(node);
	if (mark)
		addr = addr | 0x1;
	else
		addr = addr & 0xFFFFFFFE;
	return reinterpret_cast<LFSKNode *>(addr);
}

class LFSKNode
{
public:
	int key;
	LFSKNode* next[MAX_LEVEL];
	int topLevel;

	// 보초노드에 관한 생성자
	LFSKNode() {
		for (int i = 0; i<MAX_LEVEL; i++) {
			next[i] = AtomicMarkableReference(NULL, false);
		}
		topLevel = MAX_LEVEL;
	}
	LFSKNode(int myKey) {
		key = myKey;
		for (int i = 0; i<MAX_LEVEL; i++) {
			next[i] = AtomicMarkableReference(NULL, false);
		}
		topLevel = MAX_LEVEL;
	}

	// 일반노드에 관한 생성자
	LFSKNode(int x, int height) {
		key = x;
		for (int i = 0; i<MAX_LEVEL; i++) {
			next[i] = AtomicMarkableReference(NULL, false);
		}
		topLevel = height;
	}

	void InitNode() {
		key = 0;
		for (int i = 0; i<MAX_LEVEL; i++) {
			next[i] = AtomicMarkableReference(NULL, false);
		}
		topLevel = MAX_LEVEL;
	}

	void InitNode(int x, int top) {
		key = x;
		for (int i = 0; i<MAX_LEVEL; i++) {
			next[i] = AtomicMarkableReference(NULL, false);
		}
		topLevel = top;
	}

	bool CompareAndSet(int level, LFSKNode *old_node, LFSKNode *next_node, bool old_mark, bool next_mark) {
		int old_addr = reinterpret_cast<int>(old_node);
		if (old_mark) old_addr = old_addr | 0x1;
		else old_addr = old_addr & 0xFFFFFFFE;
		int next_addr = reinterpret_cast<int>(next_node);
		if (next_mark) next_addr = next_addr | 0x1;
		else next_addr = next_addr & 0xFFFFFFFE;
		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int *>(&next[level]), &old_addr, next_addr);
		//int prev_addr = InterlockedCompareExchange(reinterpret_cast<long *>(&next[level]), next_addr, old_addr);
		//return (prev_addr == old_addr);
	}
};

class LFSKSET
{
public:

	LFSKNode * head;
	LFSKNode* tail;

	LFSKSET() {
		head = new LFSKNode(0x80000000);
		tail = new LFSKNode(0x7FFFFFFF);
		for (int i = 0; i<MAX_LEVEL; i++) {
			head->next[i] = AtomicMarkableReference(tail, false);
		}
	}

	void Init()
	{
		LFSKNode *curr = head->next[0];
		while (curr != tail) {
			LFSKNode *temp = curr;
			curr = GetReference(curr->next[0]);
			delete temp;
		}
		for (int i = 0; i<MAX_LEVEL; i++) {
			head->next[i] = AtomicMarkableReference(tail, false);
		}
	}

	bool Find(int x, LFSKNode* preds[], LFSKNode* succs[])
	{
		int bottomLevel = 0;
		bool marked = false;
		bool snip;
		LFSKNode* pred = NULL;
		LFSKNode* curr = NULL;
		LFSKNode* succ = NULL;
	retry:
		while (true) {
			pred = head;
			for (int level = MAX_LEVEL - 1; level >= bottomLevel; level--) {
				curr = GetReference(pred->next[level]);
				while (true) {
					succ = curr->next[level];
					while (Marked(succ)) { //표시되었다면 제거
						snip = pred->CompareAndSet(level, curr, succ, false, false);
						if (!snip) goto retry;
						//	if (level == bottomLevel) freelist.free(curr);
						curr = GetReference(pred->next[level]);
						succ = curr->next[level];
					}

					// 표시 되지 않은 경우
					// 키값이 현재 노드의 키값보다 작다면 pred전진
					if (curr->key < x) {
						pred = curr;
						curr = GetReference(succ);
						// 키값이 그렇지 않은 경우
						// curr키는 대상키보다 같거나 큰것이므로 pred의 키값이 
						// 대상 노드의 바로 앞 노드가 된다.		
					}
					else {
						break;
					}
				}
				preds[level] = pred;
				succs[level] = curr;
			}
			return (curr->key == x);
		}
	}

	bool Add(int x)
	{
		int topLevel = 0;
		while ((rand() % 2) == 1)
		{
			topLevel++;
			if (topLevel >= MAX_LEVEL - 1) break;
		}

		int bottomLevel = 0;
		LFSKNode *preds[MAX_LEVEL];
		LFSKNode *succs[MAX_LEVEL];
		while (true) {
			bool found = Find(x, preds, succs);
			// 대상 키를 갖는 표시되지 않은 노드를 찾으면 키가 이미 집합에 있으므로 false 반환
			if (found) {
				return false;
			}
			else {
				LFSKNode* newNode = new LFSKNode;
				newNode->InitNode(x, topLevel);

				for (int level = bottomLevel; level <= topLevel; level++) {
					LFSKNode* succ = succs[level];
					// 현재 새노드의 next는 표시되지 않은 상태, find()가 반환반 노드를 참조
					newNode->next[level] = Set(succ, false);
				}

				//find에서 반환한 pred와 succ의 가장 최하층을 먼저 연결
				LFSKNode* pred = preds[bottomLevel];
				LFSKNode* succ = succs[bottomLevel];

				newNode->next[bottomLevel] = Set(succ, false);

				//pred->next가 현재 succ를 가리키고 있는지 않았는지 확인하고 newNode와 참조설정
				if (!pred->CompareAndSet(bottomLevel, succ, newNode, false, false))
					// 실패일경우는 next값이 변경되었으므로 다시 호출을 시작
					continue;

				for (int level = bottomLevel + 1; level <= topLevel; level++) {
					while (true) {
						pred = GetReference(preds[level]);
						succ = GetReference(succs[level]);
						// 최하층 보다 높은 층들을 차례대로 연결
						// 연결을 성공할경우 다음단계로 넘어간다
						if (pred->CompareAndSet(level, succ, newNode, false, false))
							break;
						//Find호출을 통해 변경된 preds, succs를 새로 얻는다.
						Find(x, preds, succs);
					}
				}

				//모든 층에서 연결되었으면 true반환
				return true;
			}
		}
	}

	bool Remove(int x)
	{
		int bottomLevel = 0;
		LFSKNode *preds[MAX_LEVEL];
		LFSKNode *succs[MAX_LEVEL];
		LFSKNode* succ;

		while (true) {
			bool found = Find(x, preds, succs);
			if (!found) {
				//최하층에 제거하려는 노드가 없거나, 짝이 맞는 키를 갖는 노드가 표시 되어 있다면 false반환
				return false;
			}
			else {
				LFSKNode* nodeToRemove = succs[bottomLevel];
				//최하층을 제외한 모든 노드의 next와 mark를 읽고 AttemptMark를 이용하여 연결에 표시
				for (int level = nodeToRemove->topLevel; level >= bottomLevel + 1; level--) {
					succ = nodeToRemove->next[level];
					// 만약 연결이 표시되어있으면 메서드는 다음층으로 이동
					// 그렇지 않은 경우 다른 스레드가 병행을 햇다는 뜻이므로 현재 층의 연결을 다시 읽고
					// 연결에 다시 표시하려고 시도한다.
					while (!Marked(succ)) {
						nodeToRemove->CompareAndSet(level, succ, succ, false, true);
						succ = nodeToRemove->next[level];
					}
				}
				//이부분에 왔다는 것은 최하층을 제외한 모든 층에 표시했다는 의미

				bool marked = false;
				succ = nodeToRemove->next[bottomLevel];
				while (true) {
					//최하층의 next참조에 표시하고 성공했으면 Remove()완료
					bool iMarkedIt = nodeToRemove->CompareAndSet(bottomLevel, succ, succ, false, true);
					succ = succs[bottomLevel]->next[bottomLevel];

					if (iMarkedIt) {
						Find(x, preds, succs);
						return true;
					}
					else if (Marked(succ)) return false;
				}
			}
		}
	}

	bool Contains(int x)
	{
		int bottomLevel = 0;
		bool marked = false;
		LFSKNode *pred = head;
		LFSKNode *curr = NULL;
		LFSKNode *succ = NULL;

		for (int level = MAX_LEVEL - 1; level >= bottomLevel; level--) {
			curr = GetReference(pred->next[level]);
			while (true) {
				succ = curr->next[level];
				while (Marked(succ)) {
					curr = GetReference(curr->next[level]);
					succ = curr->next[level];
				}
				if (curr->key < x) {
					pred = curr;
					curr = GetReference(succ);
				}
				else {
					break;
				}
			}
		}
		return (curr->key == x);
	}
	void Dump()
	{
		LFSKNode *curr = head;
		printf("First 20 entries are : ");
		for (int i = 0; i<20; ++i) {
			curr = curr->next[0];
			if (NULL == curr) break;
			printf("%d(%d), ", curr->key, curr->topLevel);
		}
		printf("\n");
	}
};

CSET my_set;


void benchmark(int num_thread)
{
	for (int i = 0; i < NUM_TEST / num_thread; ++i)	{
	//	if (0 == i % 100000) cout << ".";
		switch (rand() % 3) {
		case 0: my_set.Add(rand() % RANGE); break;
		case 1: my_set.Remove(rand() % RANGE); break;
		case 2: my_set.Contains(rand() % RANGE); break;
		default: cout << "ERROR!!!\n"; exit(-1);
		}
	}
}

int main()
{
	vector <thread> worker;
	for (int num_thread = 1; num_thread <= 16; num_thread *= 2) {
		my_set.Init();
		worker.clear();

		auto start_t = high_resolution_clock::now();
		for (int i = 0; i < num_thread; ++i) 
			worker.push_back(thread{ benchmark, num_thread });
		for (auto &th : worker) th.join();
		auto du = high_resolution_clock::now() - start_t;
		node_pool.Recycle();
		my_set.Dump();

		cout << num_thread << " Threads,  Time = ";
		cout << duration_cast<milliseconds>(du).count() << " ms\n";
	}
	system("pause");
}
