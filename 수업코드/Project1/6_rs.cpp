#include <iostream>
#include <thread>
using namespace  std;

volatile bool flag = false;
volatile int _data;

void recv()
{
	while (flag == false);
	cout << "I got " << _data << "\n";
}

void send()
{
	_data = 999;
	flag = true;
	cout << "flag up\n";
}

int main()
{
	thread receiver{ recv };
	thread sender{ send };

	receiver.join();
	sender.join();
}