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

## Useful Links
We noticed a interesting skipList-like structure in here. (https://github.com/topling/cspp-memtable)

## Disclaimer 
This project represents independent research that we completed in May 2024. You may notice that some of the ideas in our work are similar to those presented in the paper "Range Cache: An Efficient Cache Component for Accelerating Range Queries on LSM-Based Key-Value Stores". However, it is important to note that while this paper was accepted by ICDE in late November 2023, it was not publicly available until its IEEE publication in July 2024.

Given the timing of our work, we believe this to be a case of concurrent work, rather than any form of plagiarism. While the conceptual approaches are aligned, our implementation diverges in some key areas.  For instance, while Range Cache uses only a Skiplist structure, our OmniCache employs a hybrid structure combining both HashTable and Skiplist. We found that, for frequent point queries, Skiplist queries were still too slow, and thus used a HashTable to speed up access to frequently accessed items. Additionally, we explored the impact of different cache eviction strategies on performance, which distinguishes our work from the ideas presented in Range Cache. It is also worth noting that Range Cache has not released its code, whereas our work is fully open-source. We have made our implementation publicly available to encourage collaboration and further development.

We have cited Range Cache in our work and fully acknowledge its contributions to the field.
