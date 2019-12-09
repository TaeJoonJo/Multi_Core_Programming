#include <iostream>
#include <thread>
using namespace  std;

int sum;

void do_work()
{
	for (auto i = 0; i < 25000000; ++i) {
		_asm add sum, 2;
		//_asm add sum,2 //sum += 2;
	}

	//int temp{};
	//for (auto i = 0; i < 25000000; ++i)
	//	temp += 2;
	//sum += temp;
}

int main()
{
	system("pause");

	while (true) {
		sum = 0;
		thread t1{ do_work };
		thread t2{ do_work };
		//thread t3{ do_work };
		//thread t4{ do_work };

		t1.join();
		t2.join();

		cout << "Sum = " << sum << "\n";
	}
	//t3.join();
	//t4.join();

	//cout << "Sum = " << sum << "\n";
}