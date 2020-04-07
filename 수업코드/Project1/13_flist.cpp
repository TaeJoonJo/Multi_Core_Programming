#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <chrono>
#include <atomic>

using namespace std;
using namespace std::chrono;

constexpr auto NUM_TEST = 4000000;
constexpr auto KEY_RANGE = 1000;

class NODE {
public:
	mutex node_mutex;
	int key;
	NODE* next;

	NODE() { next = NULL; }

	NODE(int key_value) {
		next = NULL;
		key = key_value;
	}
	~NODE() {}

	void lock() { node_mutex.lock(); }
	void unlock() { node_mutex.unlock(); }
};

class nullmutex {
public:
	void lock() {}
	void unlock() {}
};

// 세밀한 동기화 (fine-grained synchronization)
// node마다 lock을 가지고 있음

class FLIST {
	NODE head, tail;
public:
	FLIST() {
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next = &tail;
	}
	~FLIST() {}

	void Init()
	{
		NODE* ptr;
		while (head.next != &tail) {
			ptr = head.next;
			head.next = head.next->next;
			delete ptr;
		}
	}

	bool Add(int key)
	{
		NODE* pred, * curr;

		head.lock();
		pred = &head;

		curr = pred->next;
		curr->lock();

		while (curr->key < key) {
			pred->unlock();
			pred = curr;
			curr = curr->next;
			curr->lock();
		}

		if (key == curr->key) {
			pred->unlock();
			curr->unlock();
			return false;
		}
		else {
			NODE* node = new NODE(key);
			node->next = curr;
			pred->next = node;
			pred->unlock();
			curr->unlock();
			return true;
		}
	}

	bool Remove(int key)
	{
		NODE* pred, * curr;

		head.lock();
		pred = &head;

		curr = pred->next;
		curr->lock();

		while (curr->key < key) {
			pred->unlock();
			pred = curr;
			curr = curr->next;
			curr->lock();
		}

		if (key == curr->key) {
			pred->next = curr->next;
			curr->unlock();
			pred->unlock();
			delete curr;

			return true;
		}
		else {
			curr->unlock();
			pred->unlock();
			return false;
		}
	}

	bool Contains(int key)
	{
		NODE* pred, * curr;
		head.lock();
		pred = &head;
		curr = pred->next;
		curr->lock();

		while (curr->key < key) {
			pred->unlock();
			pred = curr;
			curr = curr->next;
			curr->lock();
		}

		if (key == curr->key) {
			curr->unlock();
			pred->unlock();
			return true;
		}
		else {
			curr->unlock();
			pred->unlock();
			return false;
		}
	}

	void display20()
	{
		int c = 20;
		NODE* p = head.next;
		while (p != &tail) {
			cout << p->key << "  ";
			p = p->next;
			--c;
			if (c <= 0)break;
		}
		cout << endl;
	}
};
FLIST clist;

void ThreadFunc(int num_thread)
{
	int key;
	for (int i = 0; i < NUM_TEST / num_thread; ++i) {
		switch (rand() % 3)
		{
		case 0:
			key = rand() % KEY_RANGE;
			clist.Add(key);
			break;
		case 1:
			key = rand() % KEY_RANGE;
			clist.Remove(key);
			break;
		case 2:
			key = rand() % KEY_RANGE;
			clist.Contains(key);
			break;
		default:
			cout << "Error\n";
			exit(-1);
			break;
		}
	}
}

int main(void)
{
	for (int num = 1; num <= 16; num *= 2) {
		clist.Init();

		auto t = high_resolution_clock::now();

		std::vector<std::thread>threads;

		for (int i = 0; i < num; ++i)
			threads.emplace_back(ThreadFunc, num);

		for (auto& th : threads) th.join();

		auto d = high_resolution_clock::now() - t;

		threads.clear();

		int exec_ms = duration_cast<milliseconds>(d).count();

		std::cout << exec_ms << " msecs, " << num << "num" << std::endl;
		clist.display20();

	}
	system("pause");
}