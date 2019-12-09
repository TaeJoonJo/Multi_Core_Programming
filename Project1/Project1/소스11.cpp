#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <atomic>
using namespace  std;
using namespace std::chrono;

/////////////////////////////////////////// ½Ç½À 19 ///////////////////////////////////////////

const auto NUM_TEST = 4000000;
const auto KEY_RANGE = 1000;

//atomic <int> sum;
volatile int sum;
mutex mylock;
vector<thread> mt;

class NODE {
public:
	int key;
	NODE* next;
public:
	NODE() { next = nullptr; }
	NODE(int value) { next = nullptr; key = value; }
	~NODE() {}
};

class CLIST {
public:
	NODE head, tail;
	mutex glock;
	CLIST() {
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.next = &tail;
	}
	~CLIST() {}
public:
	void Init() {
		NODE* ptr;
		while (head.next != &tail) {
			ptr = head.next;
			head.next = head.next->next;
			delete ptr;
		}
	}
	bool Add(int key) {
		NODE* pred{}, * curr{};

		pred = &head;
		glock.lock();
		curr = pred->next;
		while (curr->key < key) {
			pred = curr;
			curr = curr->next;
		}
		if (key == curr->key) {
			glock.unlock();
			return false;
		}
		else {
			NODE* node = new NODE(key);
			node->next = curr;
			pred->next = node;
			glock.unlock();
			return true;
		}
	}
	bool Remove(int key){
		NODE* pred{}, * curr{};

		pred = &head;
		glock.lock();
		curr = pred->next;
		while (curr->key < key) {
			pred = curr;
			curr = curr->next;
		}
		if (key == curr->key) {
			pred->next = curr->next;
			delete curr;
			glock.unlock();
			return true;
		}
		else {
			glock.unlock();
			return false;
		}
	}
	bool Contains(int key) {
		NODE* pred{}, * curr{};

		pred = &head;
		glock.lock();
		curr = pred->next;
		while (curr->key < key) {
			pred = curr;
			curr = curr->next;
		}
		if (key == curr->key) {
			glock.unlock();
			return true;
		}
		else {
			glock.unlock();
			return false;
		}
	}
	void Display20() {
		NODE* curr{};
		int x{};
		curr = head.next;
		while (x++ != 20) {
			if (curr == &tail)
				break;
			cout << curr->key << " ";
			curr = curr->next;
		}
		cout << "\n";
	}
};

CLIST clist;

void do_work(int num_threads, int myid)
{
	int key{};

	for (int i = 0; i < NUM_TEST / num_threads; i++) {
		switch (rand() % 3)
		{
		case 0:
			key = rand() % KEY_RANGE;
			clist.Add(key);
			break;
		case 1:
			key = rand() % KEY_RANGE;
			clist.Remove(key);
			break;
		case 2:
			key = rand() % KEY_RANGE;
			clist.Contains(key);
			break;
		default:
			cout << "Error\n";
			exit(-1);
		}
	}
}

int main()
{
	system("pause");
	clist.Init();

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
		clist.Display20();
		cout << "Thread num [" << num_threads << "]" << " Duration : " << exec_time << "ms\n";
		clist.Init();
	}
}