#include <iostream>
#include <thread>
#include <atomic>
using namespace  std;

const auto SIZE = 50000000;
volatile int x, y;
int trace_x[SIZE], trace_y[SIZE];

void ThreadFunc0() 
{
	for (int i = 0; i < SIZE; i++) 
	{
		x = i;
		atomic_thread_fence(std::memory_order_seq_cst);
		trace_y[i] = y; 
	} 
}

void ThreadFunc1()
{
	for (int i = 0; i < SIZE; i++)
	{
		y = i;
		//atomic_thread_fence(std::memory_order_seq_cst);
		trace_x[i] = x;
	}
}

int main()
{
	thread t1{ ThreadFunc0 };
	thread t2{ ThreadFunc1 };

	t1.join();
	t2.join();

	int count = 0;
	for (int i = 0; i < SIZE; ++i) {
		if (trace_x[i] != trace_x[i + 1]) continue;
		int x = trace_x[i];
		if (trace_y[x] != trace_y[x + 1]) continue;
		if (trace_y[x] == i) ++count;
	}

	/*for (int i = 0; i < SIZE; ++i) 
		if (trace_x[i] == trace_x[i + 1]) 
			if (trace_y[trace_x[i]] == trace_y[trace_x[i] + 1]) 
			{
				if (trace_y[trace_x[i]] != i) 
					continue; 
				count++; 
			} 
	*/
	cout << "Total Memory Inconsistency: " << count << endl;
}