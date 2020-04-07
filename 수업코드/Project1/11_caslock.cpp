#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <atomic>
using namespace  std;
using namespace std::chrono;

/////////////////////////////////////////// 실습 18 ///////////////////////////////////////////

//atomic <int> sum;
volatile int sum;
mutex mylock;
vector<thread> mt;

volatile int x;		// 0 -> Lock이 free다.
			// 1 -> 누군가 Lock을 얻어서 CS을 실행중이다.
bool CAS(int* addr, int exp, int update)
{
	return atomic_compare_exchange_strong(reinterpret_cast<atomic_int*>(addr), &exp, update);
}
bool CAS(volatile int* addr, int exp, int update)
{
	return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int*>(addr), &exp, update);
}

namespace myCAS {
	void Lock(int myid)
	{
		while (CAS(&x, 0, 1) == false) {}

		//while (0 != x);			// data race때문에 제대로 작동안함
		//x = 1;
	}
	void UnLock(int myid)
	{
		//while (CAS(&x, 1, 0) == false) {}
		//CAS(&x, 1, 0);

		x = 0;
	}
}

void do_work(int num_threads, int myid)
{
	for (auto i = 0; i < 50000000 / num_threads; ++i) {
		myCAS::Lock(myid);
		sum = sum + 2;
		myCAS::UnLock(myid);
	}
}

int main()
{
	system("pause");


	for (int num_threads = 1; num_threads <= 16; num_threads *= 2) {
		sum = 0;
		auto start = high_resolution_clock::now();
		for (auto i = 0; i < num_threads; ++i)
			mt.emplace_back(do_work, num_threads, i);

		for (auto& t : mt)
			t.join();
		mt.clear();

		auto end = high_resolution_clock::now();
		auto exec = end - start;
		int exec_time = duration_cast<milliseconds>(exec).count();

		cout << "Thread num [" << num_threads << "], Sum = " << sum << " Duration : " << exec_time << "ms\n";
	}
}