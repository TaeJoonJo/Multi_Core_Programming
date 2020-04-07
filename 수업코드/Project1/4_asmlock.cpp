#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <atomic>
using namespace  std;
using namespace std::chrono;

volatile int sum;
mutex mylock;
vector<thread> mt;

void do_work(int num_threads)
{
	//mylock.lock();
	for (auto i = 0; i < 50000000 / num_threads; ++i) {
		//mylock.lock();
		//sum += 2;
		//_asm add sum, 2; 
		_asm lock add sum, 2;
		//mylock.unlock();
	}
	//mylock.unlock();
}

int main()
{
	system("pause");

	for(int num_threads = 1; num_threads <= 16; num_threads *= 2) {       
		sum = 0;
		auto start = high_resolution_clock::now();
		for (auto i = 0; i < num_threads; ++i) {
			mt.emplace_back(do_work, num_threads);
		}

		//thread t1{ do_work };
		//thread t2{ do_work };
		//thread t3{ do_work };
		//thread t4{ do_work };

		//t1.join();
		//t2.join();
		for (auto& t : mt)
			t.join();
		mt.clear();

		auto end = high_resolution_clock::now();
		auto exec = end - start;
		int exec_time = duration_cast<milliseconds>(exec).count();

		cout << "Thread num [" << num_threads << "], Sum = " << sum << " Duration : " << exec_time << "\n";
	}
	//t3.join();
	//t4.join();

	//cout << "Sum = " << sum << "\n";
}