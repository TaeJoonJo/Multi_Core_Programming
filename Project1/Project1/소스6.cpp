#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <atomic>
using namespace  std;
using namespace std::chrono;

//atomic <int> sum;
int sum;
mutex mylock;
vector<thread> mt;

volatile int victim = 0;
volatile bool flag[2]{ false, false };

namespace piterson {
	void Lock(int myid)
	{
		int other = 1 - myid;
		flag[myid] = true;
		atomic_thread_fence(std::memory_order_seq_cst);
		//_asm mfence
		victim = myid;
		while (true == flag[other] && victim == myid){}
	}
	void UnLock(int myid)
	{
		flag[myid] = false;
	}
}

void do_work(int num_threads, int myid)
{
	for (auto i = 0; i < 50000000 / num_threads; ++i) {
		//mylock.lock();
		piterson::Lock(myid);
		sum += 2;
		piterson::UnLock(myid);
		//mylock.unlock();
	}
}

int main()
{
	system("pause");

	for (int num_threads = 1; num_threads <= 2; num_threads *= 2) {
		sum = 0;
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