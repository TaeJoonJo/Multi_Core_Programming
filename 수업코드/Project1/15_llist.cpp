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
	volatile bool marked;
	NODE() { next = NULL; marked = false; }

	NODE(int key_value) {
		next = NULL;
		key = key_value;
		marked = false;
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

// 게으른 동기화 (lazy synchronization)
// 낙천적 동기화는 유효성을 검사할때 list를 한번 더 순회할 필요가 있다.
// 이를 줄이자. mark를 추가한다.

class CLIST {
	NODE head, tail;
	NODE* freelist;
	NODE freetail;
	mutex mymutex;
public:
	CLIST() {
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next = &tail;
		freetail.key = 0x7FFFFFFF;
		freelist = &freetail;
	}
	~CLIST() {}

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
		while (true) {
			NODE* pred, * curr;
			pred = &head;
			curr = pred->next;
			while (curr->key < key) {
				pred = curr;
				curr = curr->next;
			}
			pred->lock();
			curr->lock();
			if (validate(pred, curr)) {
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
			else {
				pred->unlock();
				curr->unlock();
				continue;
			}
		}
	}

	bool Remove(int key)
	{
		while (true) {
			NODE* pred, * curr;
			pred = &head;
			curr = pred->next;
			while (curr->key < key) {
				pred = curr;
				curr = curr->next;
			}
			pred->lock();
			curr->lock();
			if (validate(pred, curr)) {
				if (key == curr->key) {
					curr->marked = true;
					pred->next = curr->next;
					//mymutex.lock();
					//curr->next = freelist;
					//freelist = curr;
					//mymutex.unlock();
					curr->unlock();
					pred->unlock();
					//delete curr;
					return true;
				}
				else {
					curr->unlock();
					pred->unlock();
					return false;
				}
			}
			else {
				curr->unlock();
				pred->unlock();
				continue;
			}
		}
	}

	bool Contains(int key)
	{
		NODE* curr;
		curr = &head;
		while (curr->key < key) {
			curr = curr->next;
		}

		return curr->key == key && !curr->marked;
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

	void recycle_freelist()
	{
		NODE* p = freelist;
		while (p != &freetail) {
			NODE* n = p->next;
			delete p;
			p = n;
		}
		freelist = &freetail;
	}
private:
	bool validate(NODE* pre, NODE* cur)
	{
		return !pre->marked && !cur->marked && pre->next == cur;
	}

};
CLIST clist;

void ThreadFunc(int num_thread)
{
	int key;
	for (int i = 0; i < NUM_TEST / num_thread; ++i) {
		switch (rand() % 3)
		{
		case 0:
			key = rand() % KEY_RANGE;
			//while(clist.Add(key) == false);
			clist.Add(key);
			break;
		case 1:
			key = rand() % KEY_RANGE;
			//while(clist.Remove(key) == false);
			clist.Remove(key);
			break;
		case 2:
			key = rand() % KEY_RANGE;
			//while(clist.Contains(key) == false);
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
		clist.recycle_freelist();

	}
	system("pause");
}