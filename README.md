# Sparse Parallel Approximate Priority-Queue

## Description

This is a header library containing a parallel implementation of an approximate priority queue. It is **lock-free** and uses only a single atomic to keep track of whether the queue is empty or not.

The queue works on a work-sharing principle whose dynamics is determined by the underlying queue network, i.e., the directed graph of work-sharing channels between workers including frequencies of sharing their work with their neighbours, and sending batchsizes. This allows flexibility in the mixing rate of high-priority tasks across domains in non-uniform memory architectures (NUMA) to balance memory movement.

## Build

The following bash script will automatically download dependencies on GoogleTest and GoogleBenchmark and then build the tests and benchmarks in the folders build/tests and build/benchmarks, respectively.

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

To run all tests, run the following command.

```bash
make run_tests
```

To run all benchmarks, run the following command.

```bash
make run_BM
```