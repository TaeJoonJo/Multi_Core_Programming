#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>


///////////////////////////////////////////////////////////////// shared_ptr을 최소화하자 /////////////////////////////////////////////////////////////////

using namespace std;
using namespace chrono;

const auto NUM_TEST = 40000;
const auto KEY_RANGE = 1000;

class SPNODE {
public:
	int key;
	shared_ptr<SPNODE> next;
	mutex m;
	bool marked;
	SPNODE() {
		next = nullptr;
		marked = false;
	}
	SPNODE(int key_value) {
		next = nullptr;
		key = key_value;
		marked = false;
	}
	void lock() { m.lock(); }
	void unlock() { m.unlock(); }
	~SPNODE()
	{

	}
};

class CLIST {
	shared_ptr<SPNODE> head, tail;

public:
	CLIST()
	{
		head = make_shared<SPNODE>();
		tail = make_shared<SPNODE>();
		head->key = 0x80000000;
		tail->key = 0x7FFFFFFF;
		head->next = tail;
	}
	~CLIST() {}
	void Init()
	{
		head->next = tail;
	}


	bool Add(int key)
	{
		shared_ptr<SPNODE> pred, curr;
		//head.lock();
		while (true) {
			pred = head;                  //head는 바뀌지 않기 때문에 읽어와도 괜찮다. 
			curr = pred->next;               //shared_ptr에서 copy하면 data race를 유발한다;
			//curr->lock();
			while (curr->key < key) {
				//pred->unlock();
				pred = curr;               //pred와 curr은 지역변수라서 다른 쓰레드에서 접근하지 않기 때문에 이 부분은 안전
				curr = curr->next;            //next는 전역이기 때문에 Data race 발생할 수 있다.
			}
			pred->lock();
			curr->lock();

			if (Validate(pred, curr)) {
				if (key == curr->key) {
					pred->unlock();
					curr->unlock();
					return false;
				}
				else {
					shared_ptr<SPNODE> node(new SPNODE(key));
					node->next = curr;
					pred->next = node;

					pred->unlock();
					curr->unlock();
					return true;
				}
			}
			else {
				pred->unlock();
				curr->unlock();

			}
		}
	}
	bool Remove(int key)
	{
		shared_ptr<SPNODE> pred, curr;
		//head.lock();
		while (true) {
			pred = head;
			curr = pred->next;
			//curr->lock();
			while (curr->key < key) {
				//pred->unlock();
				pred = curr;

				curr = curr->next;
			}
			pred->lock();
			curr->lock();

			if (Validate(pred, curr)) {
				if (key == curr->key) {
					curr->marked = true;
					pred->next = curr->next;

					curr->unlock();
					pred->unlock();

					//delete curr;
					return true;
				}
				else {
					curr->unlock();
					pred->unlock();
					return false;
				}
			}
			else {
				pred->unlock();
				curr->unlock();
			}
		}
	}
	bool Contains(int key)
	{
		shared_ptr<SPNODE>  curr = head;
		//head.lock();
		while (curr->key < key)
			curr = curr->next;
		return curr->key == key && !curr->marked;
	}

	void Display()
	{
		int c = 20;
		shared_ptr<SPNODE> p = head->next;
		while (p != tail)
		{
			cout << p->key << ", ";
			p = p->next;
			c--;
			if (c == 0) break;
		}
	}

	bool Validate(const shared_ptr<SPNODE>& pred, const shared_ptr<SPNODE>& curr) {
		return !pred->marked && !curr->marked && pred->next == curr;
	}
};

CLIST clist;

void ThreadFunc(int num_thread)
{
	int key;
	for (int i = 0; i < NUM_TEST / num_thread; i++) {
		switch (rand() % 3) {
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
			cout << "error\n";
			exit(-1);
		}
	}


}

int main()
{
	for (int num_thread = 1; num_thread <= 16; num_thread *= 2) {
		clist.Init();
		vector<thread> threads;

		auto s = high_resolution_clock::now();
		for (int i = 0; i < num_thread; ++i) {
			threads.emplace_back(ThreadFunc, num_thread);
		}
		for (auto& th : threads) th.join();
		auto e = high_resolution_clock::now();
		cout << "threads[" << num_thread << "]  ";
		cout << duration_cast<milliseconds>(e - s).count() << "ms\t";
		clist.Display();
		cout << endl;

	}
	system("pause");

	return 0;
}