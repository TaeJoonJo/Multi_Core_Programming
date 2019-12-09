#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <vector>

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

constexpr unsigned int ST_EMPTY			= 0;
constexpr unsigned int ST_WAITING		= 0x40000000;
constexpr unsigned int ST_BUSY			= 0x80000000;
constexpr int MAX_LOOP = 100;

class EL_ARRAY;

class EXCHANGER {
	volatile int slot;
public:
	EL_ARRAY* el_array;
	EXCHANGER() { 
		slot = 0; 
		el_array = nullptr;
	}
	bool CAS(unsigned int old_st, int value, unsigned int new_st) {
		int old_v = (slot & 0x3FFFFFFF) | old_st;
		int new_v = (value & 0x3FFFFFFF) | new_st;
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int*>(&slot), &old_v, new_v);
	}
	void Busy();
	int exchange(int value, bool* time_out) {
		*time_out = false;
		int ret = 0;
		for(int j = 0; j < MAX_LOOP; ++j) {	// 몇번안에 성공 못하면 타임 아웃 시켜줘야 함
			switch (slot & 0xC0000000) {	// 앞에 두비트만 짤라냄
			case ST_EMPTY:
				if (true == CAS(ST_EMPTY, value, ST_WAITING)) { //slot = value;
					for (int i = 0; i < MAX_LOOP; ++i) {
						if ((slot & 0xC0000000) != ST_WAITING) {
							ret = slot & 0x3FFFFFFF;
							slot = ST_EMPTY;
							return ret;
						}
					}
					if (true == CAS(ST_WAITING, 0, ST_EMPTY)) {  //slot = 0;
						*time_out = true;
						return 0;
					}
					else {	// 실패하면 교환이 된것
						ret = slot & 0x3FFFFFFF;
						slot = ST_EMPTY;
						return ret;
					}
				}
				break;
			case ST_WAITING:
				ret = slot & 0x3FFFFFFF;
				if (CAS(ST_WAITING, value, ST_BUSY)) {
					return ret;
				}
				break;
			case ST_BUSY:
				Busy();
				break;
			default:
				cout << "Invalid Slot!\n";
				while (true);
			}
		}
		
		*time_out = true;
		return 0;
	}
};

constexpr int CAPACITY = 8;
constexpr int MAX_TIME_OUT = 10;

class EL_ARRAY {
	EXCHANGER exchanger[CAPACITY];
	int range;
public:
	int num_time_out;
	EL_ARRAY() {
		for (int i = 0; i < CAPACITY; ++i)
			exchanger[i].el_array = this;
		range = 1;
		num_time_out = 0;
	}
	int visit(int value, bool* time_out) {
		int idx = rand() % range;
		int ret = exchanger[idx].exchange(value, time_out);
		if (true == *time_out) {
			int nto = num_time_out;
			CAS(&num_time_out, nto, nto + 1);
			if (num_time_out == MAX_TIME_OUT) {
				CAS(&num_time_out, nto + 1, 0);
				int r = range;
				if (range > 1) {
					CAS(&range, r, r - 1);
					//cout << "range : " << r << "\n";
				}
			}
		}
		return ret;
	}
	void Busy() {
		int nto = num_time_out;
		CAS(&num_time_out, nto, nto - 1);
		if (num_time_out == MAX_TIME_OUT * -1) {
			CAS(&num_time_out, nto - 1, 0);
			int r = range;
			if (r < 7) {
				CAS(&range, r, r + 1);
				//cout << "range : " << r << "\n";
			}
		}
	}
	bool CAS(int* value, int old_v, int new_v) {
		return atomic_compare_exchange_strong(reinterpret_cast<atomic_int*>(value), &old_v, new_v);
	}
};

void EXCHANGER::Busy() {
	el_array->Busy();
}

class LFELSTACK {
	NODE* volatile top;
	EL_ARRAY el_array;
	mutex glock;
public:
	LFELSTACK()
	{
		top = nullptr;
	}
	~LFELSTACK() {}

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
		NODE* e = new NODE(key);
		while (true) {
			NODE* last = top;

			if (last != top) continue;
			e->next = last;

			if (CAS(&top, last, e)) {
				return true;
			}
			bool time_out;
			int ret = el_array.visit(key, &time_out);
			if (false == time_out) {
				if (0 == ret) return true;
			}
		}
		return false;
	}
	int Pop()
	{
		while (true) {
			NODE* first = top;
			if (first == nullptr) {

				return 0;
			}
			NODE* next = first->next;
			if (first != top) continue;
			int value = first->key;
			if (true == CAS(&top, first, next)) return value;
			bool time_out;
			int ret = el_array.visit(0, &time_out);
			if (false == time_out) {
				if (0 == ret) return ret;
			}
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

LFELSTACK stack;

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
	for (auto num_thread = 1; num_thread <= 8; num_thread *= 2) {
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