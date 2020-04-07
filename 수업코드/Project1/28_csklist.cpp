#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <atomic>
using namespace  std;
using namespace std::chrono;

/////////////////////////////////////////// skiplist ///////////////////////////////////////////

const auto NUM_TEST = 4000000;
const auto KEY_RANGE = 1000;

//atomic <int> sum;
volatile int sum;
mutex mylock;
vector<thread> mt;

constexpr int MAXHEIGHT = 10;

class SKNODE {
public:
	int key;
	SKNODE* next[MAXHEIGHT];
	int height;
public:
	SKNODE() {
		key = 0;
		for (int i = 0; i < MAXHEIGHT; ++i) next[i] = nullptr;
		height = MAXHEIGHT;
	}
	SKNODE(int value) {
		key = value;
		for (int i = 0; i < MAXHEIGHT; ++i) next[i] = nullptr;
		height = MAXHEIGHT;
	}
	SKNODE(int value, int h) {
		key = value;
		for (int i = 0; i < MAXHEIGHT; ++i) next[i] = nullptr;
		height = h;
	}
	~SKNODE() {}
};

class SKLIST {
public:
	SKNODE head, tail;
	mutex glock;
	SKLIST() {
		head.key = 0x80000000;
		tail.key = 0x7FFFFFFF;
		head.height = tail.height = MAXHEIGHT;
		for(int i = 0; i < MAXHEIGHT; ++i) head.next[i] = &tail;
	}
	~SKLIST() {
		this->Init();
	}
public:
	void Init() {
		SKNODE* ptr;
		while (head.next[0] != &tail) {
			ptr = head.next[0];
			head.next[0] = head.next[0]->next[0];
			delete ptr;
		}
		for (auto& p : head.next) p = &tail;
	}
	void Find(int key, SKNODE* preds[], SKNODE* currs[]) {
		int cl = MAXHEIGHT - 1;
		while (true) {
			if (MAXHEIGHT - 1 == cl)
				preds[cl] = &head;
			else
				preds[cl] = preds[cl + 1];
			currs[cl] = preds[cl]->next[cl];
			while (currs[cl]->key < key) {
				preds[cl] = currs[cl];
				currs[cl] = currs[cl]->next[cl];
			}
			if (0 == cl)
				return;
			--cl;
		}
	}
	bool Add(int key) {
		SKNODE* preds[MAXHEIGHT], * currs[MAXHEIGHT];

		glock.lock();
		Find(key, preds, currs);

		if (key == currs[0]->key) {
			glock.unlock();
			return false;
		}
		else {
			int height = 1;
			while (rand() % 2 == 0) 
				if (++height == MAXHEIGHT) break;
			
			SKNODE* node = new SKNODE(key, height);
			for (int i = 0; i < height; ++i) {
				node->next[i] = currs[i];
				preds[i]->next[i] = node;
			}
			glock.unlock();
			return true;
		}
	}
	bool Remove(int key) {
		SKNODE* preds[MAXHEIGHT], * currs[MAXHEIGHT];

		glock.lock();
		Find(key, preds, currs);

		if (key == currs[0]->key) {
			for (int i = 0; i < currs[0]->height; ++i) 
				preds[i]->next[i] = currs[i]->next[i];
			delete currs[0];
			glock.unlock();
			return true;
		}
		else {
			glock.unlock();
			return false;
		}
	}
	bool Contains(int key) {
		SKNODE* preds[MAXHEIGHT], * currs[MAXHEIGHT];

		glock.lock();
		Find(key, preds, currs);

		if (key == currs[0]->key) {
			glock.unlock();
			return true;
		}
		else {
			glock.unlock();
			return false;
		}
	}
	void Display20() {
		SKNODE* curr{};
		int x{};
		curr = head.next[0];
		while (x++ != 20) {
			if (curr == &tail)
				break;
			cout << curr->key << " ";
			curr = curr->next[0];
		}
		cout << "\n";
	}
};

SKLIST clist;

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
	system("pause");
}