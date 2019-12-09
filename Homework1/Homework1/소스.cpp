#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <atomic>
#include <algorithm>
using namespace  std;
using namespace std::chrono;

constexpr int NUMBER = 50000000;

class Bakery {
private:
	volatile bool* flag{};
	volatile int* label{};
	int size{};
public:
	Bakery() {};
	~Bakery() {
		if(flag != nullptr) delete flag;
		if (label != nullptr) delete label;
	}
public:
	void Init(int threadnum) {
		if (flag != nullptr) delete flag;
		if (label != nullptr) delete label;
		size = threadnum;
		flag = new bool[threadnum] {};
		label = new int[threadnum] {};
	}
	void Lock(int myid) {
		flag[myid] = true;
		label[myid] = *max_element(label, label + (size)) + 1;
		flag[myid] = false;
		for (int i = 0; i < size; ++i) {
			//if (i == myid) continue;
			while (flag[i] == true) {};
			while ((label[i] != 0) && ((label[i],i) < (label[myid],myid))) {};
		}
	}
	void UnLock(int myid) {
		//flag[myid] = false;
		label[myid] = 0;
	}
};

volatile int sum;
vector<thread> mt;
Bakery mylock;
//mutex mylock;

void do_work(int num_threads, int myid)
{
	for (int i = 0; i < NUMBER / num_threads; ++i) {
		mylock.Lock(myid);
		//mylock.lock();
		sum += 2;
		//mylock.unlock();
		mylock.UnLock(myid);
	}
}

int main()
{
	system("pause");
	
	for (int num_threads = 1; num_threads <= 16; num_threads *= 2) {
		sum = 0;
		mylock.Init(num_threads);
		auto start = high_resolution_clock::now();
		for (auto i = 0; i < num_threads; ++i) {
			mt.emplace_back(do_work, num_threads, i);
		}

		for (auto& t : mt)
			t.join();
		mt.clear();

		auto end = high_resolution_clock::now();
		auto exec = end - start;
		int exec_time = duration_cast<milliseconds>(exec).count();

		cout << "Thread num [" << num_threads << "], Sum = " << sum << " Duration : " << exec_time << "\n";
	}
}