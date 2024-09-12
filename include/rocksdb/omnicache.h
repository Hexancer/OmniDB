//
// Created by xuhuai on 6/20/24.
//

#ifndef ROCKSDB_OMNICACHE_H
#define ROCKSDB_OMNICACHE_H

#include "comparator.h"
#include "memtable/follyskiplist.h"

namespace rocksdb {

struct OmniCache {
  typedef FollySkipList::Iterator OmniCacheIterator;

  FollySkipList* follySkipList;

  explicit OmniCache(const ColumnFamilyOptions& cf_options);
  ~OmniCache();
  static bool Enabled();

  std::unique_ptr<OmniCacheIterator> Seek(const Slice& key);
  std::unique_ptr<FollySkipList::Iterator> Insert(const Slice& key,
                                                  const Slice& value);
  std::unique_ptr<FollySkipList::Iterator> Append(const Slice& key,
                                                  const Slice& value);

  std::unique_ptr<OmniCacheIterator> NewIterator();
};

struct OmniCacheEnv {
  const size_t DEFAULT_MAXSIZE = 1ULL << 23;
  const std::string DEFAULT_PERF_SERVER = "localhost:50051";

  bool enabled = false;
  size_t maxsize = DEFAULT_MAXSIZE;
  bool perfEnabled = false;
  std::string perfServer = DEFAULT_PERF_SERVER;

  OmniCacheEnv();

  static OmniCacheEnv& GetOmniCacheEnv();
};

}  // namespace rocksdb

#endif  // ROCKSDB_OMNICACHE_H
