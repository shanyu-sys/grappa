#include <Grappa.hpp>
#include <Delegate.hpp>
#include <Collective.hpp>
#include <GlobalAllocator.hpp>
#include <iostream>

using namespace Grappa;

#define N_ELEMENTS_BLOCK 512 // The same as BLOCK_SIZE/8 in Addressing.hpp

struct Block
{
  int64_t x[N_ELEMENTS_BLOCK]; // block_size is 64
};

int grappa_read(size_t addr, size_t buf, size_t byte_size)
{
  GlobalAddress<Block> A = *((GlobalAddress<Block> *)addr);
  size_t count = byte_size / sizeof(Block);

  for (size_t i = 0; i < count; i++)
  {
    ((Block *)buf)[i] = Grappa::delegate::read(A + i);
  }

  return 1; // success
}

int grappa_write(size_t addr, size_t buf, size_t size)
{
  GlobalAddress<Block> A = *((GlobalAddress<Block> *)addr);
  size_t count = size / sizeof(Block);

  // write the buffer to the global address
  for (size_t i = 0; i < count; i++)
  {
    Grappa::delegate::write(A + i, ((Block *)buf)[i]);
  }

  return 1; // success
}

int grappa_dealloc(size_t addr) {
  GlobalAddress<Block> A = *((GlobalAddress<Block>*) addr);
  Grappa::global_free(A);
  return 1; // success
}

GlobalAddress<Block> grappa_alloc(size_t byte_size) {
  size_t count = byte_size / sizeof(Block);
  GlobalAddress<Block> array = Grappa::global_alloc<Block>(count);
  return array;
}

int main(int argc, char *argv[])
{
  init(&argc, &argv);
  run([]
      {
        std::cout << "size of block: " << block_size << std::endl;
        size_t array_byte_size = 1024 * 1024 * 1024;
        GlobalAddress<Block> array = grappa_alloc(array_byte_size);
        size_t array_addr = (size_t) &array;

        // write and then read number of count blocks
        size_t count = 4;

        // initialize the buffer for writing into grappa global array
        Block *buf = new Block[count];
        for (size_t i = 0; i < count; i++)
        {
          buf[i] = Block();
          for (size_t j = 0; j < N_ELEMENTS_BLOCK; j++)
          {
            buf[i].x[j] = i * 2;
          }
        }
        size_t buf_addr = (size_t)&buf[0];

        int wr_success = grappa_write(array_addr, buf_addr, count * sizeof(Block));

        Block *buf2 = new Block[count];
        size_t buf2_addr = (size_t)&buf2[0];

        int rd_success = grappa_read(array_addr, buf2_addr, count * sizeof(Block));

        // check buf2 equals to buf1
        for (size_t i = 0; i < count; i++)
        {
          for (size_t j = 0; j < N_ELEMENTS_BLOCK; j++)
          {
            if (buf2[i].x[j] != buf[i].x[j])
            {
              std::cout << "buf2[" << i << "].x[" << j << "] = " << buf2[i].x[j] << " != buf[" << i << "].x[" << j << "] = " << buf[i].x[j] << std::endl;
              std::cout << "FAILURE" << std::endl;
              return;
            }
          }
        }
        std::cout << "SUCCESS" << std::endl;
        int dealloc_success = grappa_dealloc(array_addr);
      });
  finalize();
}
