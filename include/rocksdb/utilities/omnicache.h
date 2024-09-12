#ifndef ROCKSDB_UTILITIES_OMNICACHE_H
#define ROCKSDB_UTILITIES_OMNICACHE_H

#include <stdlib.h>
#include <string.h>

static inline bool strenabled(const char* p) {
  return p != NULL && (strcasecmp(p, "TRUE") == 0 || strcasecmp(p, "ON") == 0 ||
                       strcasecmp(p, "1") == 0);
}
static inline bool strdisabled(const char* p) {
  return p != NULL && (strcasecmp(p, "FALSE") == 0 ||
                       strcasecmp(p, "OFF") == 0 || strcasecmp(p, "0") == 0);
}

static inline bool envenabled(const char* env) {
  return strenabled(getenv(env));
}
static inline bool envdisabled(const char* env) {
  return strdisabled(getenv(env));
}

#endif  // ROCKSDB_UTILITIES_OMNICACHE_H
