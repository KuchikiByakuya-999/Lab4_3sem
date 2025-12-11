#include "primitives.h"
#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <chrono>
#include <mutex>
#include <atomic>
#include <barrier>
#include <condition_variable>
#include <string>
#include <algorithm>

using namespace std;

constexpr int THREAD_COUNT = 8;
constexpr int ITERATIONS   = 50000;

string shared_data;
mutex cout_mutex;

char random_char() {
    static thread_local mt19937 gen(random_device{}());
    uniform_int_distribution<int> dist(32, 126);
    return static_cast<char>(dist(gen));
}

template<typename F>
double run_test(const string& name, F func, int threads) {
    shared_data.clear();
    auto start = chrono::high_resolution_clock::now();

    vector<thread> v;
    v.reserve(threads);
    for (int i = 0; i < threads; ++i)
        v.emplace_back(func, i);
    for (auto &t : v) t.join();

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double, milli> ms = end - start;

    lock_guard<mutex> lock(cout_mutex);
    cout << name << ": time = " << ms.count()
         << " ms, size = " << shared_data.size() << "\n";
    return ms.count();
}

// Mutex
mutex mtx;
void worker_mutex(int) {
    for (int i = 0; i < ITERATIONS; ++i) {
        char c = random_char();
        lock_guard<mutex> lock(mtx);
        shared_data.push_back(c);
    }
}

// Semaphore
class Semaphore {
    mutex m;
    condition_variable cv;
    int count;
public:
    explicit Semaphore(int init) : count(init) {}
    void acquire() {
        unique_lock<mutex> lock(m);
        cv.wait(lock, [&]{ return count > 0; });
        --count;
    }
    void release() {
        unique_lock<mutex> lock(m);
        ++count;
        cv.notify_one();
    }
};

Semaphore sem(1);
void worker_semaphore(int) {
    for (int i = 0; i < ITERATIONS; ++i) {
        char c = random_char();
        sem.acquire();
        shared_data.push_back(c);
        sem.release();
    }
}

// Barrier
barrier sync_point(THREAD_COUNT);

void worker_barrier(int id) {
    for (int i = 0; i < ITERATIONS; ++i) {
        char c = random_char();
        if (id == 0) {
            shared_data.push_back(c);
        }
        sync_point.arrive_and_wait();
    }
}

// SpinLock
class SpinLock {
    atomic_flag flag = ATOMIC_FLAG_INIT;
public:
    void lock() {
        while (flag.test_and_set(memory_order_acquire)) {}
    }
    void unlock() {
        flag.clear(memory_order_release);
    }
};

SpinLock spin;
void worker_spinlock(int) {
    for (int i = 0; i < ITERATIONS; ++i) {
        char c = random_char();
        spin.lock();
        shared_data.push_back(c);
        spin.unlock();
    }
}

// SpinWait
atomic<bool> ready_flag{false};
void worker_spinwait_producer(int) {
    for (int i = 0; i < ITERATIONS; ++i) {
        while (ready_flag.load(memory_order_acquire)) {}
        char c = random_char();
        shared_data.push_back(c);
        ready_flag.store(true, memory_order_release);
    }
}
void worker_spinwait_consumer(int) {
    for (int i = 0; i < ITERATIONS; ++i) {
        while (!ready_flag.load(memory_order_acquire)) {}
        ready_flag.store(false, memory_order_release);
    }
}

// Monitor
class Monitor {
    mutex m;
    condition_variable cv;
    bool can_write = true;
public:
    void enter() {
        unique_lock<mutex> lock(m);
        cv.wait(lock, [&]{ return can_write; });
        can_write = false;
    }
    void leave() {
        unique_lock<mutex> lock(m);
        can_write = true;
        cv.notify_one();
    }
};

Monitor mon;
void worker_monitor(int) {
    for (int i = 0; i < ITERATIONS; ++i) {
        char c = random_char();
        mon.enter();
        shared_data.push_back(c);
        mon.leave();
    }
}

void run_primitives_demo() {
    cout << "=== Примитивы синхронизации ===\n";
    run_test("Mutex",      worker_mutex,      THREAD_COUNT);
    run_test("Semaphore",  worker_semaphore,  THREAD_COUNT);
    run_test("SpinLock",   worker_spinlock,   THREAD_COUNT);

    {
        shared_data.clear();
        auto start = chrono::high_resolution_clock::now();
        thread p(worker_spinwait_producer, 0);
        thread c(worker_spinwait_consumer, 0);
        p.join(); c.join();
        auto end = chrono::high_resolution_clock::now();
        chrono::duration<double, milli> ms = end - start;
        cout << "SpinWait: time = " << ms.count()
             << " ms, size = " << shared_data.size() << "\n";
    }

    run_test("Barrier",    worker_barrier,    THREAD_COUNT);
    run_test("Monitor",    worker_monitor,    THREAD_COUNT);
}
