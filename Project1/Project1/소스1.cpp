#include <iostream>
#include <thread>

using namespace std;

void f();
struct F {
	void operator() ();
};

int main()
{
	thread t1{ f };
	thread t2{ F() };
	thread t3{ [](int a) {a = 3; } };

	t1.join();
}