#include <iostream>
#include <omp.h>
using namespace std;

/* OpenMP */

int main()
{
	int a{};

	#pragma omp parallel
	{
		//a += 2;
		cout << "hi" << endl;
	}

	cout << a << endl;

	system("pause");
}