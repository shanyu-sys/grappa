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
#include <GlobalHashMap.hpp>
using namespace Grappa;

// datatype: 0 int32 1 float64 2 uint32
#define PATH "/mnt/ssd/haoran/types_proj/dataset/dataframe/my_"

// #define FILE_NAME "G1_1e5_1e2_0_0.csv"
// #define LINE_COUNT 100000
// #define READ_CHUNKS_PER_CORE 100000
// #define FILE_NAME "G1_1e7_1e2_0_0.csv"
// #define LINE_COUNT 10000000
// #define READ_CHUNKS_PER_CORE 100000
#define FILE_NAME "G1_1e8_1e2_0_0.csv"
#define LINE_COUNT 100000000
const size_t CHUNK_SIZE = 32;
#define READ_CHUNKS_PER_CORE 1048576
// const size_t CHUNK_SIZE = 64;
// #define READ_CHUNKS_PER_CORE 524288
// const size_t CHUNK_SIZE = 128;
// #define READ_CHUNKS_PER_CORE 262144



// #define FILE_NAME "group_2.csv"
// #define LINE_COUNT 6

#define AGG_CNT 64



GlobalCompletionEvent read_gce;
GlobalCompletionEvent foralle;
GlobalCompletionEvent groupbye;
CompletionEvent joiner;

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

class Vector {
public:
    size_t* _buffer;
    size_t _capacity;
    size_t _size;
    size_t _padding;
    Vector() : _buffer(nullptr), _capacity(0), _size(0) {}
    // Vector(size_t capacity) : _buffer(Grappa::locale_alloc<size_t>(capacity)), _capacity(capacity), _size(0) {}
    Vector(size_t capacity) : _buffer(new size_t[capacity]), _capacity(capacity), _size(0) {}
    void resize(size_t new_capacity) {
        // size_t* new_buffer = Grappa::locale_alloc<size_t>(new_capacity);
        size_t* new_buffer = new size_t[new_capacity];
        for (size_t i = 0; i < _size; i++) {
            new_buffer[i] = _buffer[i];
        }
        if (_buffer != nullptr) {
            delete []_buffer;
        }
        _buffer = new_buffer;
        _capacity = new_capacity;
    }
    void push_back(size_t value) {
        // std::cout << "push_back" << std::endl;
        // std::cout << "size: " << _size << std::endl;
        // std::cout << "capacity: " << _capacity << std::endl;
        // std::cout << "buffer: " << _buffer << std::endl;
        if (_size == _capacity) {
            resize(std::max(_capacity * 2, (size_t)1));
        }
        // std::cout << "size: " << _size << std::endl;
        // std::cout << "capacity: " << _capacity << std::endl;
        _buffer[_size] = value;
        _size++;
    }
    size_t size() {
        return _size;
    }
    size_t capacity() {
        return _capacity;
    }
    size_t at(size_t index) {
        if (index >= _size) {
            throw std::out_of_range("Index is out of bounds");
        }
        return _buffer[index];
    }
};

class FixedVector {
public:
    size_t length;
    GlobalAddress<size_t> buffer;
};

class Chunk {
public:
    uint8_t buffer[CHUNK_SIZE];
    uint8_t* get(size_t inner_index, size_t element_size) {
        uint8_t* result = new uint8_t[element_size];
        for (size_t i = 0; i < element_size; i++) {
            result[i] = buffer[inner_index * element_size + i];
        }
        return result;
    }

    size_t get_eight(size_t inner_index) {
        size_t result = 0;
        result = *((size_t*) (buffer + inner_index * 8));
        return result;
    }

    uint32_t get_four(size_t inner_index) {
        uint32_t result = 0;
        result = *((uint32_t*) (buffer + inner_index * 4));
        return result;
    }

    void put(size_t inner_index, size_t element_size, const void *data) {
        uint8_t* value = (uint8_t*) data;
        for (size_t i = 0; i < element_size; i++) {
            buffer[inner_index * element_size + i] = value[i];
        }   
    }
};

class ChunkedArray {
private:
    size_t _elementSize;
    size_t _capacity;
    size_t _num_chunks;
    GlobalAddress<Chunk> _data;

public:
    ChunkedArray() : _elementSize(0), _capacity(0), _num_chunks(0), _data() {}
    ChunkedArray(size_t elementSize, size_t capacity) : _elementSize(elementSize), _capacity(capacity) {
        _num_chunks = (_capacity * _elementSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
        _data = Grappa::global_alloc<Chunk>(_num_chunks);
    }


    size_t length() const {
        return _capacity;
    }

    size_t element_size() const {
        return _elementSize;
    }

    size_t num_chunks() {
        return _num_chunks;
    }

    uint8_t * get(size_t index) {
        size_t chunk_index = index / (CHUNK_SIZE / _elementSize);
        size_t inner_index = index % (CHUNK_SIZE / _elementSize);
        Chunk chunk = delegate::read(_data + chunk_index);
        return chunk.get(inner_index, _elementSize);
    }

    void put(size_t index, const void *data) {
        size_t chunk_index = index / (CHUNK_SIZE / _elementSize);
        size_t inner_index = index % (CHUNK_SIZE / _elementSize);
        Chunk chunk = delegate::read(_data + chunk_index);
        chunk.put(inner_index, _elementSize, data);
        delegate::write(_data + chunk_index, chunk);
    }

    GlobalAddress<Chunk> get_data() {
        return _data;
    }

    void select_from(ChunkedArray* original_column, GlobalAddress<size_t> key_index) {
        GlobalAddress<Chunk> original_data = original_column->get_data();
        size_t element_size = _elementSize;
        forall<SyncMode::Async, &foralle>(_data, _num_chunks, [=](int64_t chunk_idx, Chunk& value) {
        // forall(_data, _num_chunks, [=](int64_t chunk_idx, Chunk& value) {
            size_t result_stidx = chunk_idx * CHUNK_SIZE / element_size;
            size_t result_edidx = (chunk_idx + 1) * CHUNK_SIZE / element_size;
            if(result_edidx > _capacity) {
                // std::cout << "result_edidx: " << result_edidx << std::endl;
                result_edidx = _capacity;
            }
            for (size_t i = result_stidx; i < result_edidx; i++) {
                size_t index = delegate::read(key_index + i);
                size_t chunk_index = index / (CHUNK_SIZE / element_size);
                size_t inner_index = index % (CHUNK_SIZE / element_size);
                // uint32_t element = delegate::call(original_data + chunk_index, [inner_index](Chunk* chunk) {
                //     uint32_t element = element = chunk->get_four(inner_index);
                //     return element;
                // });
                GlobalAddress<uint32_t> element_addr = GlobalAddress<uint32_t>::Raw((size_t)(original_data + chunk_index).raw_bits() + inner_index * 4);
                uint32_t element = delegate::read(element_addr);
                if (element_size == 4) {
                    value.put(i - result_stidx, element_size, (uint8_t*)(&element));
                } else {
                    throw std::invalid_argument("Unsupported element size");
                }
            }
        });
        // foralle.wait();
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

    uint8_t * get(size_t index) {
        return data.get(index);
    }

    void put(size_t index, const void *value) {
        data.put(index, value);
    }

    ChunkedArray* get_data() {
        return &data;
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

uint8_t* convert_string_to_anytype_with_typeid(int datatype, std::string str) {
    if (datatype == 2) {
        uint32_t value = std::stoul(str);
        uint8_t* result = new uint8_t[sizeof(uint32_t)];
        std::memcpy(result, &value, sizeof(uint32_t));
        return result;
    } else if (datatype == 0) {
        int32_t value = std::stoi(str);
        uint8_t* result = new uint8_t[sizeof(int32_t)];
        std::memcpy(result, &value, sizeof(int32_t));
        return result;
    } else if (datatype == 1) {
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



void select_keys(ChunkedArray* original_column, GlobalAddress<size_t> key_index, ChunkedArray* result_key_column) {
    result_key_column->select_from(original_column, key_index);
}

struct Buffer {
    uint32_t buffer[AGG_CNT];
};

void agg_sum(ChunkedArray* original_column, GlobalAddress<Vector> value_index, ChunkedArray* result_value_column, uint8_t type /*0 int32 1 float64*/) {
    GlobalAddress<Chunk> original_data = original_column->get_data();
    GlobalAddress<Chunk> result_data = result_value_column->get_data();
    size_t element_size = original_column->element_size();
    size_t length = result_value_column->length();
    size_t num_chunks = result_value_column->num_chunks();

    forall<SyncMode::Async, &foralle>(result_data, num_chunks, [=](int64_t i, Chunk& value) {
    // forall(result_data, num_chunks, [=](int64_t i, Chunk& value) {
        // std::cout << "Aggregate on core: " << Grappa::mycore() << std::endl;
        size_t result_stidx = i * CHUNK_SIZE / element_size;
        size_t result_edidx = (i + 1) * CHUNK_SIZE / element_size;
        if(result_edidx > length) {
            result_edidx = length;
        }
        for (size_t j = result_stidx; j < result_edidx; j++) {
            size_t result_inner_idx = j % (CHUNK_SIZE / element_size);
            size_t vec_len = delegate::call(value_index + j, [](Vector* value) {
                size_t vec_len = (*value).size();
                return vec_len;
            });
            double sum = 0;
            for (size_t k = 0; k < vec_len; k+=AGG_CNT) {
                size_t num_elems = std::min((size_t)AGG_CNT, vec_len - k);
                struct Buffer b = delegate::call(value_index + j, [num_elems, k](Vector* value) {
                    struct Buffer inner_buffer;
                    for (size_t l = 0; l < num_elems; l++) {
                        inner_buffer.buffer[l] = (uint32_t)(*value).at(k + l);
                    }
                    return inner_buffer;
                });

                for (size_t l = 0; l < num_elems; l++) {
                    size_t kth_idx = b.buffer[l];
                    size_t chunk_index = kth_idx / (CHUNK_SIZE / element_size);
                    size_t inner_index = kth_idx % (CHUNK_SIZE / element_size);
                    if (chunk_index >= (LINE_COUNT * element_size + CHUNK_SIZE - 1) / CHUNK_SIZE) {
                        std::cout << "chunk_index: " << chunk_index << std::endl;
                    }
                    if (type == 0) {
                        // int32_t element = delegate::call(original_data + chunk_index, [inner_index](Chunk* chunk) {
                        //     int32_t element = (int32_t)(chunk->get_four(inner_index));
                        //     return element;
                        // });
                        GlobalAddress<int32_t> element_addr = GlobalAddress<int32_t>::Raw((size_t)(original_data + chunk_index).raw_bits() + inner_index * 4);
                        int32_t element = delegate::read(element_addr);
                        sum += element;
                    } else if (type == 1) {
                        // size_t element = delegate::call(original_data + chunk_index, [inner_index](Chunk* chunk) {
                        //     size_t element = chunk->get_eight(inner_index);
                        //     return element;
                        // });
                        GlobalAddress<size_t> element_addr = GlobalAddress<size_t>::Raw((size_t)(original_data + chunk_index).raw_bits() + inner_index * 8);
                        size_t element = delegate::read(element_addr);
                        sum += *((double*)(&element));
                    } else {
                        throw std::invalid_argument("Unsupported data type");
                    }             
                }
            }
            if (type == 0) {
                uint32_t sum_int = (uint32_t) sum;
                value.put(result_inner_idx, element_size, (uint8_t*)(&sum_int));
            } else if (type == 1) {
                value.put(result_inner_idx, element_size, (uint8_t*)(&sum));
            }
        }
    });
    // foralle.wait();
}

void agg_min(ChunkedArray* original_column, GlobalAddress<Vector> value_index, ChunkedArray* result_value_column, uint8_t type /*0 int32 1 float64*/)   {
    GlobalAddress<Chunk> original_data = original_column->get_data();
    GlobalAddress<Chunk> result_data = result_value_column->get_data();
    size_t element_size = original_column->element_size();
    size_t length = result_value_column->length();
    size_t num_chunks = result_value_column->num_chunks();

    forall<SyncMode::Async, &foralle>(result_data, num_chunks, [=](int64_t i, Chunk& value) {
    // forall(result_data, num_chunks, [=](int64_t i, Chunk& value) {
        size_t result_stidx = i * CHUNK_SIZE / element_size;
        size_t result_edidx = (i + 1) * CHUNK_SIZE / element_size;
        if(result_edidx > length) {
            result_edidx = length;
        }
        for (size_t j = result_stidx; j < result_edidx; j++) {
            size_t result_inner_idx = j % (CHUNK_SIZE / element_size);
            size_t vec_len = delegate::call(value_index + j, [](Vector* value) {
                size_t vec_len = (*value).size();
                return vec_len;
            });
            double minval = (double)(1.0E10);
            for (size_t k = 0; k < vec_len; k+=16) {
                size_t num_elems = std::min((size_t)16, vec_len - k);
                struct Buffer b = delegate::call(value_index + j, [num_elems, k](Vector* value) {
                    struct Buffer inner_buffer;
                    for (size_t l = 0; l < num_elems; l++) {
                        inner_buffer.buffer[l] = (uint32_t)(*value).at(k + l);
                    }
                    return inner_buffer;
                });
                for (size_t l = 0; l < num_elems; l++) {
                    size_t kth_idx = b.buffer[l];
                    size_t chunk_index = kth_idx / (CHUNK_SIZE / element_size);
                    size_t inner_index = kth_idx % (CHUNK_SIZE / element_size);
                    if (chunk_index >= (LINE_COUNT * element_size + CHUNK_SIZE - 1) / CHUNK_SIZE) {
                        std::cout << "chunk_index: " << chunk_index << std::endl;
                    }
                    if (type == 0) {
                        GlobalAddress<int32_t> element_addr = GlobalAddress<int32_t>::Raw((size_t)(original_data + chunk_index).raw_bits() + inner_index * 4);
                        int32_t element = delegate::read(element_addr);
                        minval = std::min(minval, (double)element);
                    } else if (type == 1) {
                        // size_t element = delegate::call(original_data + chunk_index, [inner_index](Chunk* chunk) {
                        //     size_t element = chunk->get_eight(inner_index);
                        //     return element;
                        // });
                        GlobalAddress<size_t> element_addr = GlobalAddress<size_t>::Raw((size_t)(original_data + chunk_index).raw_bits() + inner_index * 8);
                        size_t element = delegate::read(element_addr);
                        minval = std::min(minval, *((double*)(&element)));
                    } else {
                        throw std::invalid_argument("Unsupported data type");
                    }
                }
            }
            if (type == 0) {
                uint32_t min_int = (uint32_t) minval;
                value.put(result_inner_idx, element_size, (uint8_t*)(&min_int));
            } else if (type == 1) {
                value.put(result_inner_idx, element_size, (uint8_t*)(&minval));
            }
        }
    });
    // foralle.wait();
}
// implements group by and aggregation
// input: groupby column, aggregation columns, aggregation functions
// output: result key column, result aggregation column

GlobalAddress<Vector> groupby(std::vector<Series> group_by_column, size_t hash_key_range) {
    GlobalAddress<Vector> hash_map = Grappa::global_alloc<Vector>(hash_key_range);
    // forall<SyncMode::Async, &foralle>(hash_map, hash_key_range, [](int64_t i, Vector& value) {
    forall(hash_map, hash_key_range, [](int64_t i, Vector& value) {
        value._size = 0;
        value._capacity = 1;
        value._buffer = new size_t[1];
    });
    // foralle.wait();
    size_t element_size = get_element_size(group_by_column[0].type);
    size_t capacity = group_by_column[0].length();
    size_t num_series = group_by_column.size();
    GlobalAddress<GlobalAddress<Chunk>> data = Grappa::global_alloc<GlobalAddress<Chunk>>(group_by_column.size());
    for (size_t i = 0; i < group_by_column.size(); i++) {
        GlobalAddress<Chunk> chunk_data = group_by_column[i].get_data()->get_data();
        delegate::write(data + i, chunk_data);
    }

    size_t num_chunks = group_by_column[0].get_data()->num_chunks();
    GlobalAddress<Chunk> first_column_data = group_by_column[0].get_data()->get_data();

    // forall<SyncMode::Async, &foralle>(first_column_data, num_chunks, [=](int64_t i, Chunk& value) {
    forall(first_column_data, num_chunks, [=](int64_t i, Chunk& value) {
        Chunk* other_chunks = new Chunk[num_series];
        other_chunks[0] = value;
        for (size_t j = 1; j < num_series; j++) {
            GlobalAddress<Chunk> chunk_data = delegate::read(data + j);
            other_chunks[j] = delegate::read(chunk_data + i);
        }
        size_t st_idx = i * CHUNK_SIZE / element_size;
        size_t ed_idx = (i + 1) * CHUNK_SIZE / element_size;
        if (ed_idx > capacity) {
            ed_idx = capacity;
        }
        for (size_t j = st_idx; j < ed_idx; j++) {
            std::vector<uint8_t> key(element_size * num_series);
            for (size_t k = 0; k < num_series; k++) {
                uint8_t* element = other_chunks[k].get(j - st_idx, element_size);
                for (size_t l = 0; l < element_size; l++) {
                    key.push_back(element[l]);
                }
                delete[] element;
            }
            // rkhash this byte string key
            size_t hash_key = 0;
            for (size_t k = 0; k < key.size(); k++) {
                hash_key = (hash_key * 257 + key[k]) % ((size_t)1e9 + 7);
            }
            hash_key = hash_key % hash_key_range;
            delegate::call(hash_map + hash_key, [=](Vector* value) {
                (*value).push_back(j);
            });
        }
    });
    // foralle.wait();
    // forall<SyncMode::Async, &foralle>(hash_map, hash_key_range, [](int64_t i, Vector& value) {
    forall(hash_map, hash_key_range, [](int64_t i, Vector& value) {
        if(value.size() == 0) {
            value.push_back(0);
        }
    });
    // foralle.wait();

    return hash_map;
}

void groupby_agg(std::vector<Series> group_by_column,
                 std::vector<std::tuple<Series, std::string>> agg_columns,
                 std::vector<Series>& result_key_column,
                    std::vector<Series>& result_agg_column, size_t hash_key_range) {

    size_t result_siz = hash_key_range;
    GlobalAddress<Vector> value_index = groupby(group_by_column, hash_key_range);
    GlobalAddress<size_t> key_index = Grappa::global_alloc<size_t>(result_siz);
    // forall<SyncMode::Async, &foralle>(key_index, result_siz, [=](int64_t i, size_t& key_id) {
    forall(key_index, result_siz, [=](int64_t i, size_t& key_id) {
        key_id = delegate::call(value_index + i, [=](Vector* inner_vec) {
            size_t key_id = (*inner_vec).at(0);
            return key_id;
        });
    });
    // foralle.wait();
    std::cout << "finish writing index" << std::endl;
    // select keys
    for (Series series : group_by_column) {
        std::cout << "result_siz: " << result_siz << std::endl;
        size_t element_size = get_element_size(series.type);
        ChunkedArray result(element_size, result_siz);
        Series result_series(result, series.type, series.name);
        select_keys(series.get_data(), key_index, result_series.get_data());
        result_key_column.push_back(result_series);
    }

    std::cout << "finish selecting keys size: " << hash_key_range << std::endl;
    std::flush(std::cout);

    // aggregation
    for (std::tuple<Series, std::string> agg_column : agg_columns) {
        Series original_column = std::get<0>(agg_column);
        std::string agg_func = std::get<1>(agg_column);
        std::string name = agg_func + "_" + original_column.name;
        ChunkedArray result(get_element_size(original_column.type), result_siz);
        Series result_series(result, original_column.type, name);
        if (agg_func == "sum") {
            if (original_column.type == "INT32") {
                agg_sum(original_column.get_data(), value_index, result_series.get_data(), 0);
            } else if (original_column.type == "FLOAT64") {
                agg_sum(original_column.get_data(), value_index, result_series.get_data(), 1);
            } else {
                throw std::invalid_argument("Unsupported data type");
            }
        } else if (agg_func == "min") {
            if (original_column.type == "INT32") {
                agg_min(original_column.get_data(), value_index, result_series.get_data(), 0);
            } else if (original_column.type == "FLOAT64") {
                agg_min(original_column.get_data(), value_index, result_series.get_data(), 1);
            } else {
                throw std::invalid_argument("Unsupported data type");
            }
        } else {
            throw std::invalid_argument("Unsupported aggregation function");
        }
        std::cout << "finish aggregation " << name << std::endl;
        result_agg_column.push_back(result_series);
    }

    foralle.wait();

    Grappa::global_free(key_index);
    // forall<SyncMode::Async, &foralle>(value_index, result_siz, [](int64_t i, Vector& value) {
    // forall(value_index, result_siz, [](int64_t i, Vector& value) {
    //     if (value._buffer != nullptr) {
    //         delete []value._buffer;
    //     }
    // });
    // foralle.wait();
    Grappa::global_free(value_index);
}

// print the result in csv format
void print_result(std::vector<Series>& result_columns) {
    for (Series series : result_columns) {
        std::cout << series.name << ",";
    }
    std::cout << std::endl;
    for (size_t i = 0; i < std::min(result_columns[0].length(), (size_t)10); i++) {
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
            delete[] value;
        }
        std::cout << std::endl;
    }
}

// print input data in csv format

void print_input_file(int line_count) {
    std::string full_path = std::string(PATH) + std::string(FILE_NAME);
    std::ifstream file(full_path);
    std::string line;
    for (int i = 0; i < std::min(line_count + 1, 10); i++) {
        if (std::getline(file, line)) {
            std::cout << line << std::endl;
        } else {
            break;
        }
    }
}

struct CoreParam {
    size_t core_id;
};

Series read_series(std::string datatype, size_t series_id, size_t line_count) {
    std::string full_path = std::string(PATH) + std::string(FILE_NAME);
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
    ChunkedArray chunked_data(element_size, line_count);
    

    size_t num_chunks = chunked_data.num_chunks();
    size_t core_num_per_series = (num_chunks + READ_CHUNKS_PER_CORE - 1) / READ_CHUNKS_PER_CORE;
    int datatype_id = 0;
    if (datatype == "UINT32") {
        datatype_id = 2;
    } else if (datatype == "INT32") {
        datatype_id = 0;
    } else if (datatype == "FLOAT64") {
        datatype_id = 1;
    } else {
        throw std::invalid_argument("Unsupported data type");
    }

    for (int i = 0; i < num_chunks; i+=READ_CHUNKS_PER_CORE) {
        struct CoreParam* params = new CoreParam();
        params->core_id = i;
        GlobalAddress<CoreParam> params_addr = make_global(params, (series_id * core_num_per_series + i)%cores());

        uint32_t st_id = i * CHUNK_SIZE / element_size;
        uint32_t ed_id = std::min(line_count, (i + READ_CHUNKS_PER_CORE) * CHUNK_SIZE / element_size);
        uint32_t e_size = element_size;
        uint32_t s_id = series_id;
        size_t st_addr = (size_t)(chunked_data.get_data() + i).raw_bits();


        forall<SyncMode::Async, &read_gce>(params_addr, 1, [=](int64_t i, CoreParam &inner_params){
            // std::cout << "core_id" << params_addr.core() << std::endl;
            size_t line_id = (size_t) st_id;
            size_t end_id = (size_t) ed_id;
            size_t inner_element_size = (size_t) e_size;
            size_t inner_series_id = (size_t) s_id;
            GlobalAddress<Chunk> data = GlobalAddress<Chunk>::Raw(st_addr);
            std::string full_path = std::string(PATH) + std::string(FILE_NAME);
            std::ifstream file(full_path);
            std::string line;
            for (int i = 0; i < line_id + 1; i++) {
                std::getline(file, line);
            }

            size_t elements_num_per_chunk = CHUNK_SIZE / inner_element_size;
            size_t chunk_num = (end_id - line_id + elements_num_per_chunk - 1) / elements_num_per_chunk;
            for (size_t chunk_id = 0; chunk_id < chunk_num; chunk_id++) {
                Chunk chunk;
                for (size_t inner_index = 0; inner_index < elements_num_per_chunk; inner_index++) {
                    if (line_id >= end_id) {
                        break;
                    }
                    std::string line;
                    if (!std::getline(file, line)) {
                        break;
                    }
                    std::stringstream line_stream(line);
                    std::string cell;
                    int tmp_id = 0;
                    while (std::getline(line_stream, cell, ',')) {
                        if (tmp_id == inner_series_id) {
                            break;
                        }
                        tmp_id++;
                    }
                    uint8_t* value = convert_string_to_anytype_with_typeid(datatype_id, cell);
                    chunk.put(inner_index, inner_element_size, value);
                    delete[] value;
                    line_id++;
                }
                delegate::write(data + chunk_id, chunk);

                if (line_id >= end_id) {
                    break;
                }
            }
        });
        delete params;

    }
    return Series(chunked_data, datatype, name);
}

void groupby_test(std::vector<Series> sources, std::vector<size_t> key_ids, std::vector<std::tuple<size_t, std::string>> aggs, size_t hash_key_range) {
    std::vector<Series> group_by_column;
    for (size_t key_id : key_ids) {
        group_by_column.push_back(sources[key_id]);
    }
    std::vector<std::tuple<Series, std::string>> agg_columns;
    for (std::tuple<size_t, std::string> agg : aggs) {
        size_t agg_id = std::get<0>(agg);
        std::string agg_func = std::get<1>(agg);
        agg_columns.push_back(std::make_tuple(sources[agg_id], agg_func));
    }
    std::vector<Series> result_key_column;
    std::vector<Series> result_agg_column;
    groupby_agg(group_by_column, agg_columns, result_key_column, result_agg_column, hash_key_range);
    std::vector<Series> result_columns;
    result_columns.insert(result_columns.end(), result_key_column.begin(), result_key_column.end());
    result_columns.insert(result_columns.end(), result_agg_column.begin(), result_agg_column.end());
    // innergroupbye.wait();
    print_result(result_columns);
    // free memory
    for (Series series : result_columns) {
        GlobalAddress<Chunk> data = series.get_data()->get_data();
        global_free(data);
    }
}

std::string get_type_by_id(int series_id) {
    if(series_id == 0 || series_id == 1 || series_id == 2 || series_id == 3 || series_id == 4 || series_id == 5) {
        return "UINT32";
    } else if (series_id == 6 || series_id == 7) {
        return "INT32";
    } else if (series_id == 8) {
        return "FLOAT64";
    } else {
        throw std::invalid_argument("Unsupported data type");
    }
}

std::string get_name_by_id(int series_id) {
    if(series_id == 0) {
        return "id1";
    } else if (series_id == 1) {
        return "id2";
    } else if (series_id == 2) {
        return "id3";
    } else if (series_id == 3) {
        return "id4";
    } else if (series_id == 4) {
        return "id5";
    } else if (series_id == 5) {
        return "id6";
    } else if (series_id == 6) {
        return "v1";
    } else if (series_id == 7) {
        return "v2";
    } else if (series_id == 8) {
        return "v3";
    } else {
        throw std::invalid_argument("Unsupported data type");
    }
}


void test_groupby_agg(int line_count) {
    std::cout<< "start reading series" << std::endl;
    std::vector<std::string> datatypes = {"UINT32", "UINT32", "UINT32", "UINT32", "UINT32", "UINT32", "INT32", "INT32", "FLOAT64"};

    // Series series_id1 = read_series("UINT32", 0, line_count);
    // std::cout<< "finish reading series_id1" << std::endl;
    // Series series_id2 = read_series("UINT32", 1, line_count);
    // std::cout<< "finish reading series_id2" << std::endl;
    // Series series_value1 = read_series("INT32", 6, line_count);
    // std::cout<< "finish reading series_value1" << std::endl;
    // Series series_value2 = read_series("FLOAT64", 8, line_count);
    // std::cout<< "finish reading series_value2" << std::endl;
    std::vector<Series> original_frame;
    for (int i = 0; i < datatypes.size(); i++) {
        Series series = read_series(datatypes[i], i, line_count);
        original_frame.push_back(series);
    }
    read_gce.wait();
    std::cout<< "finish reading series" << std::endl;

    double dataframe_start = walltime();

    GlobalAddress<std::vector<Series>> original_frame_addr = make_global(&original_frame);


    size_t core_tmp = 0;
    for (int tmp_times = 0; tmp_times < 2; tmp_times ++) {
        GlobalAddress<size_t> gsize = make_global(&core_tmp, cores() - 1 - (tmp_times % cores()));
        forall<SyncMode::Async, &groupbye>(gsize, 1, [original_frame_addr](int64_t i, size_t& core_id){
            std::cout << "task ran on " << mycore() << std::endl;
            std::vector<Series> inner_original_frame;
            size_t series_cnt = delegate::call(original_frame_addr, [](std::vector<Series>* inner_vec) {
                size_t series_cnt = (*inner_vec).size();
                std::cout << "series_cnt: " << series_cnt << std::endl;
                return series_cnt;
            });
            for (size_t i = 0; i < series_cnt; i++) {
                ChunkedArray chunked_data = delegate::call(original_frame_addr, [i](std::vector<Series>* inner_vec) {
                    ChunkedArray chunked_data = *((*inner_vec)[i].get_data());
                    return chunked_data;
                });
                Series series(chunked_data, get_type_by_id(i), get_name_by_id(i));
                inner_original_frame.push_back(series);
            }
            groupby_test(inner_original_frame, {0, 1, 2, 3, 4, 5}, {std::make_tuple(6, "sum"), std::make_tuple(8, "sum")}, 100000000);
        });
    }


    for (int tmp_times = 0; tmp_times < 3; tmp_times ++) {
        // if(tmp_times < 2) {
        //     GlobalAddress<size_t> gsize = make_global(&core_tmp, cores() - 1 - (tmp_times % cores()));
        //     forall<SyncMode::Async, &groupbye>(gsize, 1, [original_frame_addr](int64_t i, size_t& core_id){
        //         std::cout << "task ran on " << mycore() << std::endl;
        //         std::vector<Series> inner_original_frame;
        //         size_t series_cnt = delegate::call(original_frame_addr, [](std::vector<Series>* inner_vec) {
        //             size_t series_cnt = (*inner_vec).size();
        //             std::cout << "series_cnt: " << series_cnt << std::endl;
        //             return series_cnt;
        //         });
        //         for (size_t i = 0; i < series_cnt; i++) {
        //             ChunkedArray chunked_data = delegate::call(original_frame_addr, [i](std::vector<Series>* inner_vec) {
        //                 ChunkedArray chunked_data = *((*inner_vec)[i].get_data());
        //                 return chunked_data;
        //             });
        //             Series series(chunked_data, get_type_by_id(i), get_name_by_id(i));
        //             inner_original_frame.push_back(series);
        //         }
        //         groupby_test(inner_original_frame, {0, 1, 2, 3, 4, 5}, {std::make_tuple(6, "sum"), std::make_tuple(8, "sum")}, 100000000);
        //     });
        // }
        {
            GlobalAddress<size_t> gsize = make_global(&core_tmp, cores() - 1 - ((tmp_times * 9 + 2 + 1) % cores()));
            forall<SyncMode::Async, &groupbye>(gsize, 1, [original_frame_addr](int64_t i, size_t& core_id){
                std::cout << "task ran on " << mycore();
                std::vector<Series> inner_original_frame;
                size_t series_cnt = delegate::call(original_frame_addr, [](std::vector<Series>* inner_vec) {
                    size_t series_cnt = (*inner_vec).size();
                    return series_cnt;
                });
                for (size_t i = 0; i < series_cnt; i++) {
                    ChunkedArray chunked_data = delegate::call(original_frame_addr, [i](std::vector<Series>* inner_vec) {
                        ChunkedArray chunked_data = *((*inner_vec)[i].get_data());
                        return chunked_data;
                    });
                    Series series(chunked_data, get_type_by_id(i), get_name_by_id(i));
                    inner_original_frame.push_back(series);
                }
                groupby_test(inner_original_frame, {0,1}, {std::make_tuple(6, "sum")}, 10000 /*100*/);
            });
        }
        {
            GlobalAddress<size_t> gsize = make_global(&core_tmp, cores() - 1 - ((tmp_times * 9 + 2 + 2) % cores()));
            forall<SyncMode::Async, &groupbye>(gsize, 1, [original_frame_addr](int64_t i, size_t& core_id){
                std::cout << "task ran on " << mycore();
                std::vector<Series> inner_original_frame;
                size_t series_cnt = delegate::call(original_frame_addr, [](std::vector<Series>* inner_vec) {
                    size_t series_cnt = (*inner_vec).size();
                    return series_cnt;
                });
                for (size_t i = 0; i < series_cnt; i++) {
                    ChunkedArray chunked_data = delegate::call(original_frame_addr, [i](std::vector<Series>* inner_vec) {
                        ChunkedArray chunked_data = *((*inner_vec)[i].get_data());
                        return chunked_data;
                    });
                    Series series(chunked_data, get_type_by_id(i), get_name_by_id(i));
                    inner_original_frame.push_back(series);
                }
                groupby_test(inner_original_frame, {0, 1}, {std::make_tuple(6, "sum")}, 10000);
            });
        }
        {
            GlobalAddress<size_t> gsize = make_global(&core_tmp, cores() - 1 - ((tmp_times * 9 + 2 + 3) % cores()));
            forall<SyncMode::Async, &groupbye>(gsize, 1, [original_frame_addr](int64_t i, size_t& core_id){
            std::cout << "task ran on " << mycore();
            std::vector<Series> inner_original_frame;
            size_t series_cnt = delegate::call(original_frame_addr, [](std::vector<Series>* inner_vec) {
                size_t series_cnt = (*inner_vec).size();
                return series_cnt;
            });
            for (size_t i = 0; i < series_cnt; i++) {
                ChunkedArray chunked_data = delegate::call(original_frame_addr, [i](std::vector<Series>* inner_vec) {
                    ChunkedArray chunked_data = *((*inner_vec)[i].get_data());
                    return chunked_data;
                });
                Series series(chunked_data, get_type_by_id(i), get_name_by_id(i));
                inner_original_frame.push_back(series);
            }
            groupby_test(inner_original_frame, {2}, {std::make_tuple(6, "sum"), std::make_tuple(8, "sum")}, 1000000);
        });
        }
        {
            GlobalAddress<size_t> gsize = make_global(&core_tmp, cores() - 1 - ((tmp_times * 9 + 2 + 4) % cores()));
            forall<SyncMode::Async, &groupbye>(gsize, 1, [original_frame_addr](int64_t i, size_t& core_id){
            std::cout << "task ran on " << mycore();
            std::vector<Series> inner_original_frame;
            size_t series_cnt = delegate::call(original_frame_addr, [](std::vector<Series>* inner_vec) {
                size_t series_cnt = (*inner_vec).size();
                return series_cnt;
            });
            for (size_t i = 0; i < series_cnt; i++) {
                ChunkedArray chunked_data = delegate::call(original_frame_addr, [i](std::vector<Series>* inner_vec) {
                    ChunkedArray chunked_data = *((*inner_vec)[i].get_data());
                    return chunked_data;
                });
                Series series(chunked_data, get_type_by_id(i), get_name_by_id(i));
                inner_original_frame.push_back(series);
            }
            groupby_test(inner_original_frame, {1,3}, {std::make_tuple(6, "sum"), std::make_tuple(8, "sum")}, 10000 /*100*/);
        });
        }
        {
            GlobalAddress<size_t> gsize = make_global(&core_tmp, cores() - 1 - ((tmp_times * 9 + 2 + 5) % cores()));
            forall<SyncMode::Async, &groupbye>(gsize, 1, [original_frame_addr](int64_t i, size_t& core_id){
            std::cout << "task ran on " << mycore();
            std::vector<Series> inner_original_frame;
            size_t series_cnt = delegate::call(original_frame_addr, [](std::vector<Series>* inner_vec) {
                size_t series_cnt = (*inner_vec).size();
                return series_cnt;
            });
            for (size_t i = 0; i < series_cnt; i++) {
                ChunkedArray chunked_data = delegate::call(original_frame_addr, [i](std::vector<Series>* inner_vec) {
                    ChunkedArray chunked_data = *((*inner_vec)[i].get_data());
                    return chunked_data;
                });
                Series series(chunked_data, get_type_by_id(i), get_name_by_id(i));
                inner_original_frame.push_back(series);
            }
            groupby_test(inner_original_frame, {5}, {std::make_tuple(6, "sum"), std::make_tuple(8, "sum")}, 1000000);
        });
        }
        {
            GlobalAddress<size_t> gsize = make_global(&core_tmp, cores() - 1 - ((tmp_times * 9 + 2 + 6) % cores()));
            forall<SyncMode::Async, &groupbye>(gsize, 1, [original_frame_addr](int64_t i, size_t& core_id){
            std::cout << "task ran on " << mycore();
            std::vector<Series> inner_original_frame;
            size_t series_cnt = delegate::call(original_frame_addr, [](std::vector<Series>* inner_vec) {
                size_t series_cnt = (*inner_vec).size();
                return series_cnt;
            });
            for (size_t i = 0; i < series_cnt; i++) {
                ChunkedArray chunked_data = delegate::call(original_frame_addr, [i](std::vector<Series>* inner_vec) {
                    ChunkedArray chunked_data = *((*inner_vec)[i].get_data());
                    return chunked_data;
                });
                Series series(chunked_data, get_type_by_id(i), get_name_by_id(i));
                inner_original_frame.push_back(series);
            }
            groupby_test(inner_original_frame, {3, 4}, {std::make_tuple(8, "sum"), std::make_tuple(8, "min")}, 10000);
        });
        }
        {
            GlobalAddress<size_t> gsize = make_global(&core_tmp, cores() - 1 - ((tmp_times * 9 + 2 + 7) % cores()));
            forall<SyncMode::Async, &groupbye>(gsize, 1, [original_frame_addr](int64_t i, size_t& core_id){
            std::cout << "task ran on " << mycore();
            std::vector<Series> inner_original_frame;
            size_t series_cnt = delegate::call(original_frame_addr, [](std::vector<Series>* inner_vec) {
                size_t series_cnt = (*inner_vec).size();
                return series_cnt;
            });
            for (size_t i = 0; i < series_cnt; i++) {
                ChunkedArray chunked_data = delegate::call(original_frame_addr, [i](std::vector<Series>* inner_vec) {
                    ChunkedArray chunked_data = *((*inner_vec)[i].get_data());
                    return chunked_data;
                });
                Series series(chunked_data, get_type_by_id(i), get_name_by_id(i));
                inner_original_frame.push_back(series);
            }
            groupby_test(inner_original_frame, {2}, {std::make_tuple(6, "min"), std::make_tuple(7, "min")}, 1000000);
        });
        }
        {
            GlobalAddress<size_t> gsize = make_global(&core_tmp, cores() - 1 - ((tmp_times * 9 + 2 + 8) % cores()));
            forall<SyncMode::Async, &groupbye>(gsize, 1, [original_frame_addr](int64_t i, size_t& core_id){
            std::cout << "task ran on " << mycore();
            std::vector<Series> inner_original_frame;
            size_t series_cnt = delegate::call(original_frame_addr, [](std::vector<Series>* inner_vec) {
                size_t series_cnt = (*inner_vec).size();
                return series_cnt;
            });
            for (size_t i = 0; i < series_cnt; i++) {
                ChunkedArray chunked_data = delegate::call(original_frame_addr, [i](std::vector<Series>* inner_vec) {
                    ChunkedArray chunked_data = *((*inner_vec)[i].get_data());
                    return chunked_data;
                });
                Series series(chunked_data, get_type_by_id(i), get_name_by_id(i));
                inner_original_frame.push_back(series);
            }
            groupby_test(inner_original_frame, {5}, {std::make_tuple(8, "min")}, 1000000);
        });
        }
        {
            GlobalAddress<size_t> gsize = make_global(&core_tmp, cores() - 1 - ((tmp_times * 9 + 2 + 9) % cores()));
            forall<SyncMode::Async, &groupbye>(gsize, 1, [original_frame_addr](int64_t i, size_t& core_id){
            std::cout << "task ran on " << mycore();
            std::vector<Series> inner_original_frame;
            size_t series_cnt = delegate::call(original_frame_addr, [](std::vector<Series>* inner_vec) {
                size_t series_cnt = (*inner_vec).size();
                return series_cnt;
            });
            for (size_t i = 0; i < series_cnt; i++) {
                ChunkedArray chunked_data = delegate::call(original_frame_addr, [i](std::vector<Series>* inner_vec) {
                    ChunkedArray chunked_data = *((*inner_vec)[i].get_data());
                    return chunked_data;
                });
                Series series(chunked_data, get_type_by_id(i), get_name_by_id(i));
                inner_original_frame.push_back(series);
            }
            groupby_test(inner_original_frame, {1, 3}, {std::make_tuple(6, "sum"), std::make_tuple(7, "sum")}, 10000);
        });
        }
    }


    // joiner.wait();
    groupbye.wait();

    std::ofstream myFile;
    int core_num = cores();
    char logname[256] = "/mnt/ssd/guest/DRust_home/logs/dataframe_grappa_";
    int length = strlen(logname);
    logname[length] = '0' + (core_num/16);
    logname[length+1] = '.';
    logname[length+2] = 't';
    logname[length+3] = 'x';
    logname[length+4] = 't';
    logname[length+5] = 0;
    myFile.open(logname);
    myFile << walltime() - dataframe_start;
    myFile.close();
    
    std::cout << "dataframe time: " << walltime() - dataframe_start << " seconds" << std::endl;
}



int main(int argc, char *argv[])
{
    init(&argc, &argv);

    run([]{
        std::cout << "######################################################################" << std::endl;
        std::cout << "Input CSV File" << std::endl;
        print_input_file(LINE_COUNT);
        std::cout << "######################################################################" << std::endl;
        std::cout << "Groupby Result" << std::endl;
        std::cout << "Query: select id1, id2, sum(v1), min(v3) from Table group by id1, id2" << std::endl;
        test_groupby_agg(LINE_COUNT);
        // std::cout << "######################################################################" << std::endl;
        // std::cout << "Hash Inner Join Result" << std::endl;
        // std::cout << "Query: select t1.id1, t1.id2, t1.v3, t2.td2, t2.v3 from Table as t1 inner join Table as t2 on t1.id1 = t2.id1" << std::endl;
        // test_hash_inner_join("group_2.csv", 6);
        // std::cout << "######################################################################" << std::endl;
    });

    finalize();
    return 0;
}











    

    // groupby_test(original_frame, {0}, {std::make_tuple(6, "sum")}, 10000 /*100*/);
    // std::cout << "dataframe time: " << walltime() - dataframe_start << " seconds" << std::endl;
    // groupby_test(original_frame, {0, 1}, {std::make_tuple(6, "sum")}, 10000);
    // std::cout << "dataframe time: " << walltime() - dataframe_start << " seconds" << std::endl;
    // groupby_test(original_frame, {2}, {std::make_tuple(6, "sum"), std::make_tuple(8, "sum")}, 1000000);
    // std::cout << "dataframe time: " << walltime() - dataframe_start << " seconds" << std::endl;
    // groupby_test(original_frame, {3}, {std::make_tuple(6, "sum"), std::make_tuple(8, "sum")}, 10000 /*100*/);
    // std::cout << "dataframe time: " << walltime() - dataframe_start << " seconds" << std::endl;
    // groupby_test(original_frame, {5}, {std::make_tuple(6, "sum"), std::make_tuple(8, "sum")}, 1000000);
    // std::cout << "dataframe time: " << walltime() - dataframe_start << " seconds" << std::endl;
    // groupby_test(original_frame, {3, 4}, {std::make_tuple(8, "sum"), std::make_tuple(8, "min")}, 10000);
    // std::cout << "dataframe time: " << walltime() - dataframe_start << " seconds" << std::endl;
    // groupby_test(original_frame, {2}, {std::make_tuple(6, "min"), std::make_tuple(7, "min")}, 1000000);
    // std::cout << "dataframe time: " << walltime() - dataframe_start << " seconds" << std::endl;
    // groupby_test(original_frame, {5}, {std::make_tuple(8, "min")}, 1000000);
    // groupby_test(original_frame, {1, 3}, {std::make_tuple(6, "sum"), std::make_tuple(7, "sum")}, 10000);

    // groupby_test(original_frame, {0, 1, 2, 3, 4, 5}, {std::make_tuple(6, "sum"), std::make_tuple(8, "sum")}, 100000000);
    // std::cout << "dataframe time: " << walltime() - dataframe_start << " seconds" << std::endl;
    // groupby_test(original_frame, {0, 1, 2, 3, 4, 5}, {std::make_tuple(6, "sum"), std::make_tuple(8, "sum")}, 100000000);
    // std::cout << "dataframe time: " << walltime() - dataframe_start << " seconds" << std::endl;

    // groupby_test(original_frame, {0}, {std::make_tuple(6, "sum")}, 10000 /*100*/);
    // groupby_test(original_frame, {0, 1}, {std::make_tuple(6, "sum")}, 10000);
    // groupby_test(original_frame, {2}, {std::make_tuple(6, "sum"), std::make_tuple(8, "sum")}, 1000000);
    // groupby_test(original_frame, {3}, {std::make_tuple(6, "sum"), std::make_tuple(8, "sum")}, 10000 /*100*/);
    // groupby_test(original_frame, {5}, {std::make_tuple(6, "sum"), std::make_tuple(8, "sum")}, 1000000);
    // groupby_test(original_frame, {3, 4}, {std::make_tuple(8, "sum"), std::make_tuple(8, "min")}, 10000);
    // groupby_test(original_frame, {2}, {std::make_tuple(6, "min"), std::make_tuple(7, "min")}, 1000000);
    // groupby_test(original_frame, {5}, {std::make_tuple(8, "min")}, 1000000);
    // groupby_test(original_frame, {1, 3}, {std::make_tuple(6, "sum"), std::make_tuple(7, "sum")}, 10000);


    // groupby_test(original_frame, {0}, {std::make_tuple(6, "sum")}, 10000 /*100*/);
    // groupby_test(original_frame, {0, 1}, {std::make_tuple(6, "sum")}, 10000);
    // groupby_test(original_frame, {2}, {std::make_tuple(6, "sum"), std::make_tuple(8, "sum")}, 1000000);
    // groupby_test(original_frame, {3}, {std::make_tuple(6, "sum"), std::make_tuple(8, "sum")}, 10000 /*100*/);
    // groupby_test(original_frame, {5}, {std::make_tuple(6, "sum"), std::make_tuple(8, "sum")}, 1000000);
    // groupby_test(original_frame, {3, 4}, {std::make_tuple(8, "sum"), std::make_tuple(8, "min")}, 10000);
    // groupby_test(original_frame, {2}, {std::make_tuple(6, "min"), std::make_tuple(7, "min")}, 1000000);
    // groupby_test(original_frame, {5}, {std::make_tuple(8, "min")}, 1000000);
    // groupby_test(original_frame, {1, 3}, {std::make_tuple(6, "sum"), std::make_tuple(7, "sum")}, 10000);