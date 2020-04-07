//#include <iostream>
//#include <thread>
//using namespace std;
//
//int a;
//
//void func()
//{
//	for (int i = 0; i < 10; ++i) a = a + i;
//	cout << a << "\n";
//}
//int main()
//{
//	thread child1(func);
//	thread child2(func);
//	func();
//
//	child1.join();
//	child2.join();
//}

#include <iostream>

int sum;

int main()
{
	for (auto i = 0; i < 50000000; ++i) sum += 2;
	std::cout << "Sum = " << sum << "\n";
}