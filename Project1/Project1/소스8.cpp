#include <iostream>
#include <thread>
#include <atomic>
using namespace  std;

volatile bool done = false;
volatile int* bound; 
int error;


void ThreadFunc1() 
{
	for (int j = 0; j <= 25000000; ++j)
		*bound = -(1 + *bound);
	done = true; 
}
void ThreadFunc2() 
{
	while (!done) 
	{
		int v = *bound;
		if ((v != 0) && (v != -1)) {
			//cout << hex << error << " ";
			error++;
		}
	}
}

int main()
{
	int a[64]{};
	int temp = reinterpret_cast<int>(&a[31]);
	temp = (temp / 64) * 64;
	temp = temp - 3;
	bound = reinterpret_cast<int*>(temp);
	*bound = 0;

	thread t1{ ThreadFunc1 };
	thread t2{ ThreadFunc2 };
	
	t1.join();
	t2.join();

	cout << "Number of Error : " << error << endl;
}