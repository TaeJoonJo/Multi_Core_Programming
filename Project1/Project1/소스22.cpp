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

class SPTR {
public:
	NODE* volatile ptr;
	volatile int stamp;

	SPTR() {
		ptr = nullptr;
		stamp = 0;
	};
	~SPTR() {};
	SPTR(NODE* new_ptr, int new_stamp) {
		ptr = new_ptr;
		stamp = new_stamp;
	}
};

class LFQUEUE {
public:
	SPTR head;
	SPTR tail;

	mutex enqLock;
	mutex deqLock;
	bool CAS(NODE** now_, NODE* old_, NODE* new_) {
		//#ifdef _WIN32
		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int*>(now_), reinterpret_cast<int*>(&old_), *reinterpret_cast<int*>(&new_));
		//#else 
		//      return atomic_compare_exchange_strong(reinterpret_cast<atomic_llong*>(now_), reinterpret_cast<long long*>(&old_), *reinterpret_cast<long long*>(&new_));
		//#endif // !_WIN32
		//return atomic_compare_exchange_strong(reinterpret_cast<atomic_llong*>(now_), reinterpret_cast<long long*>(&old_), *reinterpret_cast<long long*>(&new_));
	}
	bool CAS(NODE* volatile* now_, NODE* old_, NODE* new_) {
		//#ifdef _WIN32
		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int volatile*>(now_), reinterpret_cast<int*>(&old_), *reinterpret_cast<int*>(&new_));
		//#else 
		//      return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_llong*>(now_), reinterpret_cast<long long*>(&old_), *reinterpret_cast<long long*>(&new_));
		//#endif // !_WIN32
		//return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_llong*>(now_), reinterpret_cast<long long*>(&old_), *reinterpret_cast<long long*>(&new_));
	}
	bool STAMP_CAS(SPTR* addr, NODE* old_node, int old_stamp, NODE* new_node) {
		SPTR old_ptr{ old_node, old_stamp };
		SPTR new_ptr{ new_node, old_stamp + 1 };
		return atomic_compare_exchange_strong(reinterpret_cast<atomic_llong*>(addr), reinterpret_cast<long long*>(&old_ptr), *reinterpret_cast<long long*>(&new_ptr));
	}

	LFQUEUE() {
		head.ptr = tail.ptr = new NODE(0);
	}
	~LFQUEUE() {}

	void Init()
	{
		NODE* ptr;
		while (head.ptr->next != nullptr) {
			ptr = head.ptr->next;
			head.ptr->next = head.ptr->next->next;
			delete ptr;
		}
		tail = head;
	}

	bool Enq(int key)
	{
		NODE* e = new NODE(key);
		while (true) {
			SPTR last = tail;
			//NODE* last = tail.ptr;
			NODE* next = last.ptr->next;
			if (last.ptr != tail.ptr) continue;
			if (next == nullptr) {
				if (CAS(&(last.ptr->next), nullptr, e)) {
					STAMP_CAS(&tail, last.ptr, last.stamp, e);
					return true;
				}
			}
			else
				STAMP_CAS(&tail, last.ptr, last.stamp, next);
		}
		return false;
	}
	int Deq()
	{
		while (true) {
			SPTR first = head;
			SPTR last = tail;
			NODE* next = first.ptr->next;
			NODE* lastnext = last.ptr->next;
			if (first.ptr != head.ptr) continue;
			if (first.ptr == last.ptr) {
				if (next == nullptr) {
					//cout << "EMPTY\n";
					this_thread::sleep_for(1ms);
					return -1;
				}
				STAMP_CAS(&tail, last.ptr, last.stamp, lastnext);
				continue;
			}
			int value = next->value;
			if (false == STAMP_CAS(&head, first.ptr, first.stamp, first.ptr->next)) continue;
			delete first.ptr;
			// delete를 막은 이유?
			// delete를 해도 원래 잘 돌아가야 한다.
			return value;                                    // (i < 10000 / num_thread) 만개이하때는 deq를 안해주기 떄문에 enq와 deq가 만날일이 적다.
		}                                                // 
		return 0;                                          //
	}                                                   //
										   //
	void Display()                                          //
	{                                                   //
		int c = 20;
		NODE* p = head.ptr->next;
		while (p != tail.ptr) {
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
		if ((rand() % 2) || i < 100 / num_thread)
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
	system("pause");
	return 0;
}