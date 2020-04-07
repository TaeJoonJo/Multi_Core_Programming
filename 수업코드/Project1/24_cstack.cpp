#include <iostream>
#include <mutex>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>


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

class CSTACK {
public:
	NODE* top;

	mutex gLock;

	CSTACK() {
		top = nullptr;
	}
	~CSTACK() {
		Init();
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
		gLock.lock();
		e->next = top;
		top = e;
		gLock.unlock();
		return true;
	}
	int Pop()
	{
		int result;
		NODE* delete_ptr;

		gLock.lock();
		if (top == nullptr) {
			gLock.unlock();
			return 0;
		}
		result = top->value;
		delete_ptr = top;
		top = top->next;
		gLock.unlock();
		delete delete_ptr;
		return result;
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

CSTACK stack;

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