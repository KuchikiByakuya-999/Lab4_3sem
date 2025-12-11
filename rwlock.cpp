#include "rwlock.h"
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>

using namespace std;

static mutex rw_cout_mutex;

class ReaderWriterLockReadersFirst {
    mutex m;
    condition_variable can_read;
    condition_variable can_write;
    int active_readers = 0;
    int waiting_writers = 0;
    bool writer_active = false;
public:
    void lock_read() {
        unique_lock<mutex> lock(m);
        can_read.wait(lock, [&]{
            return !writer_active && waiting_writers == 0;
        });
        ++active_readers;
    }
    void unlock_read() {
        unique_lock<mutex> lock(m);
        --active_readers;
        if (active_readers == 0)
            can_write.notify_one();
    }
    void lock_write() {
        unique_lock<mutex> lock(m);
        ++waiting_writers;
        can_write.wait(lock, [&]{
            return !writer_active && active_readers == 0;
        });
        --waiting_writers;
        writer_active = true;
    }
    void unlock_write() {
        unique_lock<mutex> lock(m);
        writer_active = false;
        if (waiting_writers > 0)
            can_write.notify_one();
        else
            can_read.notify_all();
    }
};

class ReaderWriterLockWritersFirst {
    mutex m;
    condition_variable can_read;
    condition_variable can_write;
    int active_readers = 0;
    int waiting_writers = 0;
    bool writer_active = false;
public:
    void lock_read() {
        unique_lock<mutex> lock(m);
        can_read.wait(lock, [&]{
            return !writer_active && waiting_writers == 0;
        });
        ++active_readers;
    }
    void unlock_read() {
        unique_lock<mutex> lock(m);
        --active_readers;
        if (active_readers == 0)
            can_write.notify_one();
    }
    void lock_write() {
        unique_lock<mutex> lock(m);
        ++waiting_writers;
        can_write.wait(lock, [&]{
            return !writer_active && active_readers == 0;
        });
        --waiting_writers;
        writer_active = true;
    }
    void unlock_write() {
        unique_lock<mutex> lock(m);
        writer_active = false;
        if (waiting_writers > 0)
            can_write.notify_one();
        else
            can_read.notify_all();
    }
};

int shared_value_rw = 0;

template<typename RWLock>
void reader_task_auto(int id, int iterations, RWLock& rw) {
    for (int i = 0; i < iterations; ++i) {
        rw.lock_read();
        int v = shared_value_rw;
        rw.unlock_read();
        if (i % 1000 == 0) {
            lock_guard<mutex> lock(rw_cout_mutex);
            cout << "[R" << id << "] read value = " << v << "\n";
        }
    }
}

template<typename RWLock>
void writer_task_auto(int id, int iterations, RWLock& rw) {
    for (int i = 0; i < iterations; ++i) {
        rw.lock_write();
        ++shared_value_rw;
        int v = shared_value_rw;
        rw.unlock_write();
        if (i % 1000 == 0) {
            lock_guard<mutex> lock(rw_cout_mutex);
            cout << "  [W" << id << "] wrote value = " << v << "\n";
        }
    }
}

void run_readers_writers_demo() {
    cout << "\n=== Readers-Writers: выбор приоритета ===\n";

    const int readers    = 5;
    const int writers    = 2;
    const int iterations = 5000;

    {
        cout << "\n-- Приоритет читателей --\n";
        ReaderWriterLockReadersFirst rw;
        shared_value_rw = 0;

        auto start = chrono::high_resolution_clock::now();
        vector<thread> threads;
        for (int i = 0; i < readers; ++i)
            threads.emplace_back(reader_task_auto<ReaderWriterLockReadersFirst>,
                                 i, iterations, ref(rw));
        for (int i = 0; i < writers; ++i)
            threads.emplace_back(writer_task_auto<ReaderWriterLockReadersFirst>,
                                 i, iterations, ref(rw));
        for (auto& t : threads) t.join();
        auto end = chrono::high_resolution_clock::now();
        chrono::duration<double, milli> ms = end - start;
        cout << "Final value: " << shared_value_rw
                  << ", time = " << ms.count() << " ms\n";
    }

    {
        cout << "\n-- Приоритет писателей --\n";
        ReaderWriterLockWritersFirst rw;
        shared_value_rw = 0;

        auto start = chrono::high_resolution_clock::now();
        vector<thread> threads;
        for (int i = 0; i < readers; ++i)
            threads.emplace_back(reader_task_auto<ReaderWriterLockWritersFirst>,
                                 i, iterations, ref(rw));
        for (int i = 0; i < writers; ++i)
            threads.emplace_back(writer_task_auto<ReaderWriterLockWritersFirst>,
                                 i, iterations, ref(rw));
        for (auto& t : threads) t.join();
        auto end = chrono::high_resolution_clock::now();
        chrono::duration<double, milli> ms = end - start;
        cout << "Final value: " << shared_value_rw
                  << ", time = " << ms.count() << " ms\n";
    }
}
