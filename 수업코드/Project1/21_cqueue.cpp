#include <iostream>
#include <mutex>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>

// Lock Free Queue 구현 성긴 동기화

using namespace std;
using namespace chrono;
const auto NUM_TEST = 10000000;
const auto KEY_RANGE = 1000;

class NODE {
public:
	int value;
	NODE* next;
	NODE() {
		next = nullptr;
	}
	NODE(int input_value) {
		next = nullptr;
		value = input_value;
	}
	~NODE() {}
};

class CQUEUE {
public:
	NODE* head;
	NODE* tail;

	mutex enqLock;
	mutex deqLock;

	CQUEUE() {
		head = new NODE();
		tail = head;
	}
	~CQUEUE() {}

	void Init()
	{
		NODE* ptr;
		while (head->next != nullptr) {
			ptr = head->next;
			head->next = head->next->next;
			delete ptr;
		}
		tail = head;
	}

	bool Enq(int key)
	{
		enqLock.lock();
		NODE* e = new NODE(key);
		tail->next = e;
		tail = e;
		enqLock.unlock();
		return true;
	}
	int Deq()
	{
		int result;
		NODE* delete_ptr;

		deqLock.lock();
		if (head->next == nullptr) {
			deqLock.unlock();
			return -1;
		}
		result = head->next->value;
		delete_ptr = head;
		head = head->next;
		deqLock.unlock();
		delete delete_ptr;
		return result;
	}

	void Display()
	{
		int c = 20;
		NODE* p = head->next;
		while (p != tail) {
			cout << p->value << ", ";
			p = p->next;
			c--;
			if (c == 0) break;
		}
	}
};

CQUEUE cqueue;

void ThreadFunc(int num_thread)
{
	int key;
	for (int i = 0; i < NUM_TEST / num_thread; i++) {
		if ((rand() % 2) || i < 10000 / num_thread)
			cqueue.Enq(i);
		else key = cqueue.Deq();
	}
}

int main()
{
	for (int num_thread = 1; num_thread <= 16; num_thread *= 2)
	{
		cqueue.Init();
		vector<thread> threads;

		auto s = high_resolution_clock::now();
		for (int i = 0; i < num_thread; ++i) {
			threads.emplace_back(ThreadFunc, num_thread);
		}
		for (auto& th : threads) th.join();
		auto e = high_resolution_clock::now();
		cout << "threads[" << num_thread << "]  ";
		cout << duration_cast<milliseconds>(e - s).count() << "ms\t";

		cqueue.Display();
		cout << endl;

	}

	return 0;
}