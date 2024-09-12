#include "follyskiplist.h"

#include "utilities/persistent_cache/lrulist.h"
#include "rocksdb/cache.h"

namespace rocksdb {
LRUList<folly::ConcurrentSkipList<FollyKV, FollyKVComparator>::NodeType>
    *lru_list;


Cache::CacheItemHelper helper1_wos(CacheEntryRole::kDataBlock, nullptr);

}
