#include <iostream>
#include <mutex>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>

// Lock Free Queue 구현

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

class LFQUEUE {
public:
	NODE* volatile head;
	NODE* volatile tail;

	mutex enqLock;
	mutex deqLock;
	bool CAS(NODE** now_, NODE* old_, NODE* new_) {
//#ifdef _WIN32 || _WIN64
//#ifdef _WIN32
//		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int*>(now_), reinterpret_cast<int*>(&old_), reinterpret_cast<int>(new_));
//#elif _WIN64
//		return atomic_compare_exchange_strong(reinterpret_cast<atomic_llong*>(now_), reinterpret_cast<long long*>(&old_), *reinterpret_cast<long long*>(&new_));
//#endif // !_WIN32
//#endif
		return atomic_compare_exchange_strong(reinterpret_cast<atomic_llong*>(now_), reinterpret_cast<long long*>(&old_), *reinterpret_cast<long long*>(&new_));
	}
	bool CAS(NODE* volatile* now_, NODE* old_, NODE* new_) {
//#ifdef _WIN32 || _WIN64
//#ifdef _WIN32
//		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int volatile*>(now_), reinterpret_cast<int*>(&old_), reinterpret_cast<int>(new_));
//#elif _WIN64
//		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_llong*>(now_), reinterpret_cast<long long*>(&old_), *reinterpret_cast<long long*>(&new_));
//#endif // !_WIN32
//#endif
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_llong*>(now_), reinterpret_cast<long long*>(&old_), *reinterpret_cast<long long*>(&new_));
	}
	LFQUEUE() {
		head = new NODE();
		tail = head;
	}
	~LFQUEUE() {}

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
		NODE* e = new NODE(key);
		while (true) {
			NODE* last = tail;
			NODE* next = last->next;
			if (last != tail) continue;
			if (next == nullptr) {
				if (CAS(&(last->next), nullptr, e)) {
					CAS(&tail, last, e);
					return true;
				}
			}
			else
				CAS(&tail, last, next);
		}
		return false;
	}
	int Deq()
	{
		while (true) {
			NODE* first = head;
			NODE* last = tail;
			NODE* next = first->next;
			if (first != head) continue;
			if (first == last) {
				if (next == nullptr) {
					cout << "EMPTY\n";
					this_thread::sleep_for(1ms);
					return -1;
				}
				CAS(&tail, last, next);
				continue;
			}
			int value = next->value;
			if (false == CAS(&head, first, next)) continue;
			//delete first;
																		// delete를 막은 이유?
																		// delete를 해도 원래 잘 돌아가야 한다.
			return value;												// (i < 10000 / num_thread) 만개이하때는 deq를 안해주기 떄문에 enq와 deq가 만날일이 적다.
		}																// 
		return 0;														//
	}																	//
																		//
	void Display()														//
	{																	//
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

LFQUEUE queue;

void ThreadFunc(int num_thread)
{
	int key;
	for (int i = 0; i < NUM_TEST / num_thread; i++) {
		if ((rand() % 2) || i < 10000 / num_thread)
			queue.Enq(i);
		else key = queue.Deq();
	}
}

int main()
{
	for (int num_thread = 1; num_thread <= 16; num_thread *= 2)
	{
		queue.Init();
		vector<thread> threads;

		auto s = high_resolution_clock::now();
		for (int i = 0; i < num_thread; ++i) {
			threads.emplace_back(ThreadFunc, num_thread);
		}
		for (auto& th : threads) th.join();
		auto e = high_resolution_clock::now();
		cout << "threads[" << num_thread << "]  ";
		cout << duration_cast<milliseconds>(e - s).count() << "ms\t";

		queue.Display();
		cout << endl;

	}

	return 0;
}