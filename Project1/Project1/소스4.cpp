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

void do_work(int num_threads)
{
	volatile int local_sum = 0;
	for (auto i = 0; i < 50000000 / num_threads; ++i) {
		//mylock.lock();
		local_sum += 2;
		/// atomic + atomic != atomic
		//sum = sum + 2;
		//sum += 2;
		//_asm add sum, 2; 
		//_asm lock add sum, 2;
		//mylock.unlock();
	}

	mylock.lock();
	sum += local_sum;
	mylock.unlock();
}

int main()
{
	system("pause");

	for (int num_threads = 1; num_threads <= 16; num_threads *= 2) {
		sum = 0;
		auto start = high_resolution_clock::now();
		for (auto i = 0; i < num_threads; ++i) {
			mt.emplace_back(do_work, num_threads);
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