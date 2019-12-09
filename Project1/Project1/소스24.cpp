#include <iostream>
#include <mutex>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>

// Lock Free Queue ±¸Çö

using namespace std;
using namespace chrono;
const auto NUM_TEST = 10000000;
const auto KEY_RANGE = 1000;

class NODE {
public:
	long value;
	NODE* next;
	NODE() {
		next = nullptr;
	}
	NODE(int input_value) {
		next = nullptr;
		value = input_value;
	}
	NODE* Get(int*) {}
	~NODE() {}
};

class LFSTACK {
public:
	NODE* volatile top;

	//mutex gLock;

	LFSTACK() {
		top = nullptr;
	}
	~LFSTACK() {
		Init();
	}

	bool CAS(NODE** now_, NODE* old_, NODE* new_) 
	{
		//return atomic_compare_exchange_strong(reinterpret_cast<atomic_llong*>(now_), reinterpret_cast<long long*>(&old_), *reinterpret_cast<long long*>(&new_));
		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int*>(now_), reinterpret_cast<int*>(&old_), reinterpret_cast<int>(new_));
	}
	bool CAS(NODE* volatile* now_, NODE* old_, NODE* new_) 
	{
		//return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_llong*>(now_), reinterpret_cast<long long*>(&old_), *reinterpret_cast<long long*>(&new_));
		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int volatile*>(now_), reinterpret_cast<int*>(&old_), reinterpret_cast<int>(new_));
	}

	void Init()
	{
		NODE* ptr;
		while (top != nullptr) {
			ptr = top;
			top = top->next;
			delete ptr;
		}
		top = nullptr;
	}

	bool Push(int key)
	{
		NODE* e = new NODE(key);
		while (true) {
			NODE* last = top;
			if (last != top) continue;
			e->next = last;
			if(CAS(&top, last, e)){
				return true;
			}
		}
		return false;
	}
	int Pop()
	{
		while (true) {
			NODE* first = top;
			if (first == nullptr) {
				//cout << "EMPTY\n";
				return 0;
			}
			NODE* next = first->next;
			if (first != top) continue;
			int value = top->value;
			if (false == CAS(&top, first, next)) continue;
			//delete first;

			return value;												
		}																
		return 0;														
	}

	void Display()
	{
		int c = 20;
		NODE* p = top;
		while (p != nullptr) {
			cout << p->value << ", ";
			p = p->next;
			c--;
			if (c == 0) break;
		}
	}
};

LFSTACK stack;

void ThreadFunc(int num_thread)
{
	int key;
	for (int i = 0; i < NUM_TEST / num_thread; i++) {
		if ((rand() % 2) || i < 1000 / num_thread)
			stack.Push(i);
		else key = stack.Pop();
	}
}

int main()
{
	for (int num_thread = 1; num_thread <= 16; num_thread *= 2)
	{
		stack.Init();
		vector<thread> threads;

		auto s = high_resolution_clock::now();
		for (int i = 0; i < num_thread; ++i) {
			threads.emplace_back(ThreadFunc, num_thread);
		}
		for (auto& th : threads) th.join();
		auto e = high_resolution_clock::now();
		cout << "threads[" << num_thread << "]  ";
		cout << duration_cast<milliseconds>(e - s).count() << "ms\t";

		stack.Display();
		cout << endl;

	}
	system("pause");
	return 0;
}