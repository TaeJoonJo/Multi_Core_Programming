#include <omp.h>
#include <iostream>
using namespace std;

constexpr unsigned int GOAL = 50000000;

int sum;

void Func(int& a) {
//#pragma omp parallel
	a += 2;
}

int main()
{
	int nthreads{};
	int a{}, b{};

	#pragma omp parallel shared(a, b)
	{
#pragma omp sections nowait
		{
#pragma omp section
			++a;
#pragma omp section
			++b;
		}
	}
	cout << a << " " << b << endl;
}