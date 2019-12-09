#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <vector>


void ThreadFunc(int threadNum);
using namespace std;
using namespace std::chrono;
const auto NUM_TEST = 10000000;
const auto KEY_RANGE = 1000;

class NODE {
public:
	int key;
	NODE* next;

	NODE() { next = nullptr; }
	NODE(int key_value) {
		next = nullptr;
		key = key_value;
	}
	~NODE() {}
};

class BACKOFF {

	int limit;
	const int minDelay = 10;
	const int maxDelay = 100000;

public:
	BACKOFF()
	{
		limit = minDelay;
	}

	void do_backoff()
	{
		if (0 == limit)
			return;
		int delay = rand() % limit;
		if (0 == delay)
			return;
		limit = limit + limit;
	}
};
class LFBOSTACK {
	NODE* top;
	mutex glock;
public:
	LFBOSTACK()
	{
		top = nullptr;

	}
	~LFBOSTACK() {}

	void Init()
	{
		NODE* ptr;
		while (top != nullptr)
		{
			ptr = top;
			top = top->next;
			delete ptr;
		}
	}
	bool CAS(NODE* volatile* addr, NODE* old_node, NODE* new_node)
	{
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int*>(addr),
			reinterpret_cast<int*>(&old_node),
			reinterpret_cast<int>(new_node));
	}

	bool Push(int key)
	{
		BACKOFF bo;
		NODE* e = new NODE(key);
		while (true) {
			NODE* last = top;

			if (last != top) continue;
			e->next = last;

			if (CAS(&top, last, e)) {
				return true;
			}
			bo.do_backoff();
		}
		return false;
	}
	int Pop()
	{
		BACKOFF bo;
		while (true) {
			NODE* first = top;
			if (first == nullptr) {

				return 0;
			}
			NODE* next = first->next;
			if (first != top) continue;
			int value = first->key;
			if (false == CAS(&top, first, next)) continue;

			return value;
			bo.do_backoff();
		}
		return 0;
	}

	void Display()
	{
		int c = 20;
		NODE* p = top->next;
		while (p != nullptr)
		{
			cout << p->key << ", ";
			p = p->next;
			c--;
			if (c == 0) break;
		}
		cout << endl;
	}
};

LFBOSTACK stack;

void ThreadFunc(int threadNum)
{
	for (int i = 0; i < NUM_TEST / threadNum; i++) {
		if ((rand() % 2 == 0) || (false)) {
			stack.Push(i);
		}
		else {
			int key = stack.Pop();
		}
	}
}

int main()
{
	for (auto num_thread = 1; num_thread <= 16; num_thread *= 2) {
		stack.Init();
		vector <thread> threads;

		auto s = high_resolution_clock::now();
		for (int i = 0; i < num_thread; ++i) {
			threads.emplace_back(ThreadFunc, num_thread);
		}
		for (auto& th : threads) th.join();
		auto e = high_resolution_clock::now();
		cout << "threads[" << num_thread << "]  ";
		cout << duration_cast<milliseconds>(e - s).count() << "ms\t";

		stack.Display();
	}
	system("pause");
}