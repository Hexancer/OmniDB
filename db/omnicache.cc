#include "rocksdb/omnicache.h"

#include "rocksdb/debug.h"
#include "rocksdb/options.h"
#include "rocksdb/utilities/omnicache.h"
#include "stdlib.h"
#include "string"

namespace rocksdb {

OmniCacheEnv::OmniCacheEnv() {
  fprintf(stderr, "OmniCacheEnv called\n");
  const char* pEnabledStr = std::getenv("OC_ENABLED");
  enabled = strenabled(pEnabledStr);

  const char* pMaxSizeStr = std::getenv("OC_MAXSIZE");
  if (pMaxSizeStr != nullptr) {
    maxsize = strtoll(pMaxSizeStr, NULL, 10);
    dbglprintf("maxsize set to %lu\n", maxsize);
  }

  const char* pPerfEnabled = std::getenv("OC_PERF");
  perfEnabled = strenabled(pPerfEnabled);

  const char* pPerfServer = std::getenv("OC_PERFSERVER");
  if (pPerfServer != nullptr) {
    perfServer = std::string(pPerfServer);
    dbglprintf("perfServer set to %s\n", perfServer.c_str());
  }
}

OmniCacheEnv& OmniCacheEnv::GetOmniCacheEnv() {
  static OmniCacheEnv env;
  return env;
}

OmniCache::OmniCache(const ColumnFamilyOptions& cf_options) {
  auto& env = OmniCacheEnv::GetOmniCacheEnv();
  if (env.enabled) {
    follySkipList = new FollySkipList(32, cf_options.comparator, env.maxsize);
  }
}

OmniCache::~OmniCache() { delete follySkipList; }

bool OmniCache::Enabled() {
  auto& env = OmniCacheEnv::GetOmniCacheEnv();
  return env.enabled;
}

std::unique_ptr<OmniCache::OmniCacheIterator> OmniCache::Seek(
    const Slice& target) {
  auto iter = NewIterator();
  iter->Seek(target);
  return iter;
}

std::unique_ptr<OmniCache::OmniCacheIterator> OmniCache::Insert(
    const Slice& key, const Slice& value) {
  return follySkipList->Insert(key, value);
}

std::unique_ptr<OmniCache::OmniCacheIterator> OmniCache::Append(
    const Slice& key, const Slice& value) {
  return follySkipList->Append(key, value);
}

std::unique_ptr<OmniCache::OmniCacheIterator> OmniCache::NewIterator() {
  return follySkipList->NewIterator();
}
}  // namespace rocksdb