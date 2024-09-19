# OmniCache: An Unified Cache for Efficient Query Handling in LSM-Tree Based Key-Value Stores
This is the repo of our implementation of OmniCache on RocksDB v9.1.0.

## Features
- A novel data structure combine Hash-table and SkipList.
- A cache directly store query result rather than block data in cache, in order to speedup future point lookups and range queries both.
- A thorough analysis of the range query process.

## Compile
```
mkdir build && cd build
cmake .. -G ninja
ninja
```

## How to Use
You could use OmniCache by setting the environment varibles in your user application.

like below:
```
env OC_ENABLED=true OC_MAXSIZE=102400 ... #( your logic)
```
You could use YCSB benchmark on OmniDB easily.


## Reference
We change the Facebook folly concurrent-skiplist and standard hash-table to store the query result. (https://github.com/facebook/folly/blob/7f69f881f693217889e5765fc07cbcebe8f8918a/folly/ConcurrentSkipList.h#L143)

