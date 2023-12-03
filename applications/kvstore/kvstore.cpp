

#include "DHT.hpp"
#include <vector>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <string>
#include <atomic>
#include <sys/time.h>
#include <cstdio>
#include <cstdlib>

#include "MatchesDHT.hpp"

// Grappa includes
#include <Grappa.hpp>
#include <Cache.hpp>
#include <ParallelLoop.hpp>
#include <GlobalCompletionEvent.hpp>
#include <AsyncDelegate.hpp>
#include <Metrics.hpp>

DEFINE_double(get_ratio, 50, "Percentage of gets in workload");
DEFINE_int64(iter, 200000, "Number of iterations");
DEFINE_int64(num_lines, 200000, "Number of lines to read from file");
DEFINE_string(filename, "/mnt/ssd/haoran/types_proj/baseline/GAM/dht/data/zipf.csv", "File to read from");

constexpr uint32_t KEY_SIZE = 100;
constexpr uint32_t REPORT = 100000;
const int val_size = 32;

// performance counters
std::atomic<uint64_t> get_latency(0);
std::atomic<uint64_t> set_latency(0);
std::atomic<uint64_t> get_finished(0);
std::atomic<uint64_t> set_finished(0);
std::atomic<uint64_t> finished(1);

using namespace Grappa;

uint64_t identity_hash(int64_t k)
{
    return k;
}

struct Value
{
    char value[val_size];
    Value()
    {
        for (int i = 0; i < val_size - 1; i++)
        {
            value[i] = 'x';
        }
        value[val_size - 1] = '\0';
    }
};

// DHT
// typedef DHT<int64_t, char[val_size], identity_hash> DHT_type;
// DHT_type Table;

// MatchesDHT
typedef MatchesDHT<int64_t, Value, identity_hash> DHT_type;
DHT_type Table;

static uint64_t nstime(void)
{
    struct timespec time;
    uint64_t ust;

    clock_gettime(CLOCK_MONOTONIC, &time);

    ust = ((uint64_t)time.tv_sec) * 1000000000;
    ust += time.tv_nsec;
    return ust;
}

static uint64_t mstime(void)
{
    return nstime() / 1000000;
}

void populate(const std::string &filePath, int64_t num_lines)
{
    FILE *fp = fopen(filePath.c_str(), "r");
    Value v = Value();
    char key[KEY_SIZE];
    uint64_t start = mstime();
    uint64_t last_report = start, current;
    fprintf(stdout, "start to populate\n");
    int cnt = 0;
    while (cnt++ < FLAGS_iter && fgets(key, KEY_SIZE, fp))
    {
        if (cnt % 10000 == 0)
        {
            fprintf(stdout, "populate %d\n", cnt);
            fflush(stdout);
        }
        key[strlen(key) - 1] = 0;
        Table.insert_unique(std::stoull(std::string(key)), v);
        if (finished.fetch_add(1, std::memory_order_relaxed) % REPORT == 0)
        {
            current = mstime();
            printf("%.2f\n", (1000.0 * REPORT) / (current - last_report));
            last_report = current;
        }
    }
    double duration = mstime() - start;
    printf("%lu, %.2f\n",
           finished.load(),
           (finished - 1) * 1000 / duration);

    std::cout << "Populate time on core " << mycore() << " locale " << mylocale() << ": " << duration << std::endl;
}

int GetRandom(int low, int high, unsigned int *seed)
{
    return (rand_r(seed) % (high - low + 1)) + low;
}

void benchmark(const std::string &filePath, int64_t num_lines)
{
    FILE *fp = fopen(filePath.c_str(), "r");
    Value v = Value();
    finished.store(1);
    uint64_t start = mstime();
    char key[KEY_SIZE];
    GlobalAddress<Value> read_value;
    unsigned int seed = rand();
    uint64_t t1, t2;
    uint64_t last_report = start, current;
    int cnt = 0;
    while (cnt++ < FLAGS_iter && fgets(key, KEY_SIZE, fp))
    {
        key[strlen(key) - 1] = 0;
        if (GetRandom(0, 100, &seed) < FLAGS_get_ratio)
        {
            t1 = nstime();
            Table.lookup(std::stoull(std::string(key)), &read_value);

            // Value v2 = delegate::read(read_value);
            // char *local_read_value = v2.value;
            // if (!strncmp(local_read_value, v.value, val_size))
            // {
            //     std::cout << "value doesn't match" << std::endl;
            // }
            t2 = nstime();
            get_latency += (t2 - t1);
            get_finished++;
        }
        else
        {
            t1 = nstime();
            Table.insert_unique(std::stoull(std::string(key)), v);
            t2 = nstime();
            set_latency += (t2 - t1);
            set_finished++;
        }

        if (finished.fetch_add(1, std::memory_order_relaxed) % REPORT == 0)
        {
            current = mstime();
            printf("%.2f\n", (1000.0 * REPORT) / (current - last_report));
            last_report = current;
        }
    }

    double duration = mstime() - start;
    double gets = get_finished.load(), sets = set_finished.load();
    double glat = get_latency.load(), slat = set_latency.load();
    printf("%lu, %.2f", finished - 1, (finished - 1) * 1000 / duration);
    if (gets > 0)
        printf(", %.2f", glat / gets);
    else
        printf(", -");
    if (sets > 0)
        printf(", %.2f", slat / sets);
    else
        printf(", -");
    printf("\n");
    std::cout << "Benchmark time on core " << mycore() << " locale " << mylocale() << ": " << duration << std::endl;
}

int main(int argc, char *argv[])
{
    Grappa::init(&argc, &argv);
    Grappa::run([]
                {
        std::cout << "get_ratio: " << FLAGS_get_ratio << std::endl;
        size_t capacity = 200000;

        // initialize DHT
        DHT_type::init_global_DHT(&Table, capacity);

        // populate DHT
        on_all_cores([=]{
        // if(mycore() % 16 == 0){
        //     populate(FLAGS_filename, FLAGS_num_lines);
        // } 
        populate(FLAGS_filename, FLAGS_num_lines);
        });

        // set to R0?
        DHT_type::set_RO_global(&Table);

        // benchmark DHT
        on_all_cores([=]{
        // if(mycore() % 16 == 0){
        //     benchmark(FLAGS_filename, FLAGS_num_lines);
        // } 
        benchmark(FLAGS_filename, FLAGS_num_lines);
        }); });
    Grappa::finalize();
}
