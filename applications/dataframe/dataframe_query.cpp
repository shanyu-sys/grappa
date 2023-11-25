#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <memory>
#include <cstring>
#include <tbb/concurrent_hash_map.h>
#include <functional>
#include <limits.h>
#include <cfloat>
#include <algorithm>
#include <thread>

#include <Grappa.hpp>
#include <Delegate.hpp>
using namespace Grappa;

const size_t CHUNK_SIZE = 1024;

std::string strip(const std::string& str) {
    auto start = std::find_if_not(str.begin(), str.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });

    auto end = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    }).base();

    if (start < end)
        return std::string(start, end);
    else
        return "";
}

class Chunk {
private:
    size_t len; // Number of elements in the array
    size_t elementSize; // Size of each element
    uint8_t buffer[CHUNK_SIZE]; // Buffer to store data

public:

    Chunk(size_t elementSize) : len(0), elementSize(elementSize) {
        if (elementSize > CHUNK_SIZE) {
            throw std::invalid_argument("Element size exceeds chunk size");
        }
    }

    size_t length() const {
        return len;
    }

    bool isFull() const {
        return len == CHUNK_SIZE / elementSize;
    }

    // append data to the end of the chunk
    void append(const void *data) {
        if (isFull()) {
            throw std::runtime_error("Chunk is full");
        }
        std::memcpy(buffer + len * elementSize, data, elementSize);
        len++;
    }

    // get data at index
    uint8_t* get(size_t index) {
        if (index >= len) {
            throw std::out_of_range("Index out of range");
        }
        return buffer + index * elementSize;
    }
};

class ChunkedArray {
private:
    std::vector<std::shared_ptr<Chunk>> chunks;
    std::vector<GlobalAddress<Chunk>> global_chunks;
    size_t elementSize;

public:
    ChunkedArray(size_t elementSize) : elementSize(elementSize) {
    }

    size_t length() const {
        size_t len = 0;
        for (auto chunk : chunks) {
            len += chunk->length();
        }
        return len;
    }

    size_t num_chunks() const {
        return chunks.size();
    }

    void append(const void *data) {
        if (chunks.empty() || chunks.back()->isFull()) {
            chunks.push_back(std::make_shared<Chunk>(elementSize));
        }

        chunks.back()->append(data);
    }

    uint8_t * get(size_t index) {
        size_t chunkIndex = index / (CHUNK_SIZE / elementSize);
        size_t chunkOffset = index % (CHUNK_SIZE / elementSize);
        return chunks[chunkIndex]->get(chunkOffset);
    }
};


class Series {
private:
    ChunkedArray data;

public:
    std::string type;
    std::string name;

    Series(ChunkedArray data, std::string type, std::string name) : data(data), type(type), name(name) {}

    size_t length() const {
        return data.length();
    }

    size_t num_chunks() const {
        return data.num_chunks();
    }

    void append(const void *value) {
        data.append(value);
    }

    uint8_t * get(size_t index) {
        return data.get(index);
    }
};

// implementation of convert_string_to_anytype, suppport "UINT32", "INT32", "FLOAT64"
// return as array of uint8_t
uint8_t* convert_string_to_anytype(std::string datatype, std::string str) {
    if (datatype == "UINT32") {
        uint32_t value = std::stoul(str);
        uint8_t* result = new uint8_t[sizeof(uint32_t)];
        std::memcpy(result, &value, sizeof(uint32_t));
        return result;
    } else if (datatype == "INT32") {
        int32_t value = std::stoi(str);
        uint8_t* result = new uint8_t[sizeof(int32_t)];
        std::memcpy(result, &value, sizeof(int32_t));
        return result;
    } else if (datatype == "FLOAT64") {
        double value = std::stod(str);
        uint8_t* result = new uint8_t[sizeof(double)];
        std::memcpy(result, &value, sizeof(double));
        return result;
    } else {
        throw std::invalid_argument("Unsupported data type");
    }
}

int get_element_size(std::string datatype) {
    if (datatype == "UINT32") {
        return sizeof(uint32_t);
    } else if (datatype == "INT32") {
        return sizeof(int32_t);
    } else if (datatype == "FLOAT64") {
        return sizeof(double);
    } else {
        throw std::invalid_argument("Unsupported data type");
    }
}


Series read_series(const std::string& file_name, std::string datatype, size_t series_id, size_t line_count) {
    // std::string full_path = "/mnt/ssd/haoran/types_proj/dataset/dataframe/my_" + file_name;
    std::string full_path = "/mnt/ssd/shan/grappa/applications/dataframe/my_" + file_name;


    // read first line of the file and get the headers

    std::ifstream file(full_path);

    std::string header_line;
    std::getline(file, header_line);

    std::vector<std::string> headers;
    std::stringstream header_stream(header_line);
    std::string header;
    while (std::getline(header_stream, header, ',')) {
        headers.push_back(strip(header));
    }
    if (headers.size() <= series_id) {
        throw std::out_of_range("Series id is out of bounds");
    }
    std::string name = headers[series_id];
    int element_size = get_element_size(datatype);
    // allocate memory for data on the heap
    ChunkedArray data(element_size);

    size_t line_id = 0;
    for (int i = 0; i < line_count; i++) {
        std::string line;
        if (!std::getline(file, line)) {
            break;
        }
        std::stringstream line_stream(line);
        std::vector<std::string> row;
        std::string cell;
        while (std::getline(line_stream, cell, ',')) {
            row.push_back(cell);
        }
        std::string str = row[series_id];
        uint8_t* value = convert_string_to_anytype(datatype, str);
        data.append(value);
        line_id++;
    }
    return Series(data, datatype, name);
}

// implement a groupby function
// input: groupby column series, chunk range, 
struct MyHashCompare {
    static size_t hash(const std::vector<uint8_t>& x) {
        size_t h = 0;
        for (uint8_t byte : x) {
            h = (h * 17) ^ byte;
        }
        return h;
    }

    static bool equal(const std::vector<uint8_t>& x, const std::vector<uint8_t>& y) {
        return x == y;
    }
};

typedef tbb::concurrent_hash_map<std::vector<uint8_t>, std::vector<size_t>, MyHashCompare> ConcurHashmap;
void groupy(std::vector<Series> group_by_column,
            ConcurHashmap& hash_map,
            int line_start, int line_end) {
    for (int i = line_start; i < line_end; i++) {
        // get the key from the groupby column
        std::vector<uint8_t> key;
        for(Series series : group_by_column) {
            // get 4 bytes key from the groupby column
            uint8_t *key_start_byte = series.get(i);
            for (int j = 0; j < 4; j++) {
                key.push_back(*key_start_byte);
                key_start_byte++;
            }
        }
        // insert the key into the hashmap
        ConcurHashmap::accessor a;
        hash_map.insert(a, key);
        a->second.push_back(i);
    }
}

void extract_index(ConcurHashmap& hash_map,
                   std::vector<size_t>& key_index,
                   std::vector<size_t>& value_index,
                   std::vector<size_t>& group_index) {
    int v_idx = 0;
    for (auto it = hash_map.begin(); it != hash_map.end(); ++it) {
        key_index.push_back(it->second[0]);
        value_index.push_back(v_idx);
        for (size_t value : it->second) {
            v_idx++;
            group_index.push_back(value);
        }
    }
    value_index.push_back(v_idx);
}

void select_keys(Series& original_column, std::vector<size_t>& key_index, Series& result_key_column) {
    for (size_t index : key_index) {
        uint8_t* value = original_column.get(index);
        result_key_column.append(value);
    }
}

void agg_sum(Series& original_column, std::vector<size_t>& value_index, std::vector<size_t> group_index, Series& result_value_column) {
    for (size_t i=0; i < value_index.size() - 1; i++) {
        size_t start = value_index[i];
        size_t end = value_index[i+1];
        if (original_column.type == "INT32") {
            int32_t sum = 0;
            for (size_t j = start; j < end; j++) {
                uint8_t* value = original_column.get(group_index[j]);
                sum += *((int32_t*) value);
            }
            result_value_column.append((uint8_t*)(&sum));
        } else if (original_column.type == "FLOAT64") {
            double sum = 0;
            for (size_t j = start; j < end; j++) {
                uint8_t* value = original_column.get(group_index[j]);
                sum += *((double*) value);
            }
            result_value_column.append((uint8_t*)(&sum));
        } else {
            throw std::invalid_argument("Unsupported data type");
        }
    }
}

void agg_min(Series& original_column, std::vector<size_t>& value_index, std::vector<size_t> group_index, Series& result_value_column) {
    for (size_t i=0; i < value_index.size() - 1; i++) {
        size_t start = value_index[i];
        size_t end = value_index[i+1];
        if (original_column.type == "INT32") {
            int32_t min = INT32_MAX;
            for (size_t j = start; j < end; j++) {
                uint8_t* value = original_column.get(group_index[j]);
                int32_t current = *((int32_t*) value);
                if (current < min) {
                    min = current;
                }
            }
            result_value_column.append((uint8_t*)(&min));
        } else if (original_column.type == "FLOAT64") {
            double min = DBL_MAX;
            for (size_t j = start; j < end; j++) {
                uint8_t* value = original_column.get(group_index[j]);
                double current = *((double*) value);
                if (current < min) {
                    min = current;
                }
            }
            result_value_column.append((uint8_t*)(&min));
        } else {
            throw std::invalid_argument("Unsupported data type");
        }
    }
}

// implements group by and aggregation
// input: groupby column, aggregation columns, aggregation functions
// output: result key column, result aggregation column

void groupby_agg(std::vector<Series> group_by_column,
                 std::vector<std::tuple<Series, std::string>> agg_columns,
                 std::vector<Series>& result_key_column,
                    std::vector<Series>& result_agg_column) {
    // group by
    ConcurHashmap hash_map;

    // create min(16, number chunk) threads to do groupby
    // groupy(group_by_column, hash_map, 0, group_by_column[0].length());
    std::vector<std::thread> threads;
    int num_threads = std::min(16, (int) group_by_column[0].num_chunks());
    int chunk_size = CHUNK_SIZE / get_element_size(group_by_column[0].type);
    for (int i = 0; i < num_threads; i++) {
        int line_start = i * chunk_size;
        int line_end = std::min((i + 1) * chunk_size, (int) group_by_column[0].length());
        threads.push_back(std::thread(groupy, group_by_column, std::ref(hash_map), line_start, line_end));
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    std::vector<size_t> key_index;
    std::vector<size_t> value_index;
    std::vector<size_t> group_index;
    extract_index(hash_map, key_index, value_index, group_index);

    // select keys
    for (Series series : group_by_column) {
        Series result = Series(ChunkedArray(get_element_size(series.type)), series.type, series.name);
        select_keys(series, key_index, result);
        result_key_column.push_back(result);
    }

    // aggregation
    for (std::tuple<Series, std::string> agg_column : agg_columns) {
        Series original_column = std::get<0>(agg_column);
        std::string agg_func = std::get<1>(agg_column);
        std::string name = agg_func + "_" + original_column.name;
        Series result = Series(ChunkedArray(get_element_size(original_column.type)), original_column.type, name);
        if (agg_func == "sum") {
            agg_sum(original_column, value_index, group_index, result);
        } else if (agg_func == "min") {
            agg_min(original_column, value_index, group_index, result);
        } else {
            throw std::invalid_argument("Unsupported aggregation function");
        }
        result_agg_column.push_back(result);
    }
}

// implement hash inner join on two columns
// input: two columns
// output: result left indexes and result right indexes

void hash_inner_join(Series& left_column,
                     Series& right_column,
                     std::vector<Series>& left_rest_columns,
                     std::vector<Series>& right_rest_columns,
                     std::vector<Series>& result_columns) {
    // build hashmap for right column
    ConcurHashmap hash_map;
    std::vector<size_t> result_left;
    std::vector<size_t> result_right;
    groupy({right_column}, hash_map, 0, right_column.length());

    // join
    for (size_t i = 0; i < left_column.length(); i++) {
        uint8_t* key = left_column.get(i);
        ConcurHashmap::accessor a;
        if (hash_map.find(a, std::vector<uint8_t>(key, key + 4))) {
            for (size_t index : a->second) {
                result_left.push_back(i);
                result_right.push_back(index);
            }
        }
    }

    // select left join column
    Series result_key_column = Series(ChunkedArray(get_element_size(left_column.type)), left_column.type, left_column.name);
    for (size_t index : result_left) {
        uint8_t* value = left_column.get(index);
        result_key_column.append(value);
    }
    result_columns.push_back(result_key_column);

    // select rest columns
    for (Series series : left_rest_columns) {
        Series result = Series(ChunkedArray(get_element_size(series.type)), series.type, series.name);
        for (size_t index : result_left) {
            uint8_t* value = series.get(index);
            result.append(value);
        }
        result_columns.push_back(result);
    }

    for (Series series : right_rest_columns) {
        Series result = Series(ChunkedArray(get_element_size(series.type)), series.type, series.name);
        for (size_t index : result_right) {
            uint8_t* value = series.get(index);
            result.append(value);
        }
        result_columns.push_back(result);
    }

}

// print the result in csv format
void print_result(std::vector<Series>& result_columns) {
    for (Series series : result_columns) {
        std::cout << series.name << ",";
    }
    std::cout << std::endl;
    for (size_t i = 0; i < result_columns[0].length(); i++) {
        for (Series series : result_columns) {
            uint8_t* value = series.get(i);
            if (series.type == "UINT32") {
                std::cout << *((uint32_t*) value) << ",";
            } else if (series.type == "INT32") {
                std::cout << *((int32_t*) value) << ",";
            } else if (series.type == "FLOAT64") {
                std::cout << *((double*) value) << "f,";
            } else {
                throw std::invalid_argument("Unsupported data type");
            }
        }
        std::cout << std::endl;
    }
}

// print input data in csv format

void print_input_file(std::string file_name, int line_count) {
    // std::string full_path = "/mnt/ssd/haoran/types_proj/dataset/dataframe/my_" + file_name;
    std::string full_path = "/mnt/ssd/shan/grappa/applications/dataframe/my_" + file_name;
    std::ifstream file(full_path);
    std::string line;
    for (int i = 0; i < line_count + 1; i++) {
        if (std::getline(file, line)) {
            std::cout << line << std::endl;
        } else {
            break;
        }
    }
}

void test_groupby_agg(std::string file_name, int line_count) {
    Series series_id1 = read_series(file_name, "UINT32", 0, 6);
    Series series_id2 = read_series(file_name, "UINT32", 1, 6);
    Series series_value1 = read_series(file_name, "INT32", 6, 6);
    Series series_value2 = read_series(file_name, "FLOAT64", 8, 6);
    std::vector<Series> group_by_column;
    group_by_column.push_back(series_id1);
    group_by_column.push_back(series_id2);

    std::vector<std::tuple<Series, std::string>> agg_columns;
    agg_columns.push_back(std::make_tuple(series_value1, "sum"));
    agg_columns.push_back(std::make_tuple(series_value2, "min"));

    std::vector<Series> result_key_column;
    std::vector<Series> result_agg_column;

    groupby_agg(group_by_column, agg_columns, result_key_column, result_agg_column);

    std::vector<Series> result_columns;
    result_columns.insert(result_columns.end(), result_key_column.begin(), result_key_column.end());
    result_columns.insert(result_columns.end(), result_agg_column.begin(), result_agg_column.end());

    // print the result in csv format
    print_result(result_columns);
}

void test_hash_inner_join(std::string file_name, int line_count) {
    Series l_series_id1 = read_series(file_name, "UINT32", 0, line_count);
    Series l_series_id2 = read_series(file_name, "UINT32", 1, line_count);
    Series l_series_value1 = read_series(file_name, "FLOAT64", 8, line_count);
    Series r_series_id1 = read_series(file_name, "UINT32", 0, line_count);
    Series r_series_id2 = read_series(file_name, "UINT32", 1, line_count);
    Series r_series_value1 = read_series(file_name, "FLOAT64", 8, line_count);

    std::vector<Series> left_columns;
    left_columns.push_back(l_series_id2);
    left_columns.push_back(l_series_value1);

    std::vector<Series> right_columns;
    right_columns.push_back(r_series_id2);
    right_columns.push_back(r_series_value1);

    std::vector<Series> result_columns;
    hash_inner_join(l_series_id1, r_series_id1, left_columns, right_columns, result_columns);

    // print the result in csv format
    print_result(result_columns);
}

// int main() {
//     std::cout << "######################################################################" << std::endl;
//     std::cout << "Input CSV File" << std::endl;
//     print_input_file("group_2.csv", 6);
//     std::cout << "######################################################################" << std::endl;
//     std::cout << "Groupby Result" << std::endl;
//     std::cout << "Query: select id1, id2, sum(v1), min(v3) from Table group by id1, id2" << std::endl;
//     test_groupby_agg("group_2.csv", 6);
//     std::cout << "######################################################################" << std::endl;
//     std::cout << "Hash Inner Join Result" << std::endl;
//     std::cout << "Query: select t1.id1, t1.id2, t1.v3, t2.td2, t2.v3 from Table as t1 inner join Table as t2 on t1.id1 = t2.id1" << std::endl;
//     test_hash_inner_join("group_2.csv", 6);
//     std::cout << "######################################################################" << std::endl;
//     return 0;
// }

int main(int argc, char *argv[])
{
    init(&argc, &argv);

    run([]{
        std::cout << "######################################################################" << std::endl;
        std::cout << "Input CSV File" << std::endl;
        print_input_file("group_2.csv", 6);
        std::cout << "######################################################################" << std::endl;
        std::cout << "Groupby Result" << std::endl;
        std::cout << "Query: select id1, id2, sum(v1), min(v3) from Table group by id1, id2" << std::endl;
        test_groupby_agg("group_2.csv", 6);
        std::cout << "######################################################################" << std::endl;
        std::cout << "Hash Inner Join Result" << std::endl;
        std::cout << "Query: select t1.id1, t1.id2, t1.v3, t2.td2, t2.v3 from Table as t1 inner join Table as t2 on t1.id1 = t2.id1" << std::endl;
        test_hash_inner_join("group_2.csv", 6);
        std::cout << "######################################################################" << std::endl;
    });

    finalize();
    return 0;
}