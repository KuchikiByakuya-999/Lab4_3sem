#include "trainings.h"
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <ctime>
#include <algorithm>

using namespace std;

struct Training {
    tm datetime{};
    string coach;
};

tm make_tm(int year, int month, int day, int hour, int min) {
    tm t{};
    t.tm_year = year - 1900;
    t.tm_mon  = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min  = min;
    t.tm_sec  = 0;
    mktime(&t);
    return t;
}

int weekday(const tm& t) {
    tm copy = t;
    mktime(&copy);
    return copy.tm_wday;
}

void find_by_weekday_range(const vector<Training>& data,
                           size_t begin, size_t end,
                           int target_wday,
                           vector<Training>& out,
                           mutex& out_mtx)
{
    vector<Training> local;
    local.reserve(end - begin);
    for (size_t i = begin; i < end; ++i) {
        if (weekday(data[i].datetime) == target_wday)
            local.push_back(data[i]);
    }
    lock_guard<mutex> lock(out_mtx);
    out.insert(out.end(), local.begin(), local.end());
}

void run_trainings_demo() {
    cout << "\n=== Тренировки в зале ===\n";

    vector<Training> trainings;
    trainings.reserve(100000);

    for (int i = 0; i < 100000; ++i) {
        int day   = 1 + (i % 28);
        int month = 1 + (i % 12);
        int year  = 2025;
        int hour  = (i % 12) + 8;
        int min   = (i % 60);
        Training tr;
        tr.datetime = make_tm(year, month, day, hour, min);
        tr.coach    = "Trainer_" + to_string(i % 20);
        trainings.push_back(tr);
    }

    int target_day = 1; // понедельник

    auto start_seq = chrono::high_resolution_clock::now();
    vector<Training> result_seq;
    for (const auto& tr : trainings) {
        if (weekday(tr.datetime) == target_day)
            result_seq.push_back(tr);
    }
    auto end_seq = chrono::high_resolution_clock::now();
    chrono::duration<double, milli> seq_ms = end_seq - start_seq;

    int thread_count = thread::hardware_concurrency();
    if (thread_count <= 0) thread_count = 4;

    auto start_par = chrono::high_resolution_clock::now();
    vector<thread> threads;
    vector<Training> result_par;
    mutex res_mtx;

    size_t n = trainings.size();
    size_t block = (n + thread_count - 1) / thread_count;

    for (int i = 0; i < thread_count; ++i) {
        size_t begin = i * block;
        if (begin >= n) break;
        size_t end = min(begin + block, n);
        threads.emplace_back(find_by_weekday_range,
                             cref(trainings),
                             begin, end,
                             target_day,
                             ref(result_par),
                             ref(res_mtx));
    }
    for (auto& t : threads) t.join();
    auto end_par = chrono::high_resolution_clock::now();
    chrono::duration<double, milli> par_ms = end_par - start_par;

    cout << "Sequential time: " << seq_ms.count() << " ms\n";
    cout << "Parallel time:   " << par_ms.count() << " ms\n";
    cout << "Found seq: " << result_seq.size()
              << ", parallel: " << result_par.size() << "\n";
}
