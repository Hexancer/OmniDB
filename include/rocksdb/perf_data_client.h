#ifndef ROCKSDB_PERF_DATA_CLIENT_H
#define ROCKSDB_PERF_DATA_CLIENT_H

#include <functional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace rocksdb {

class PerfDataClientImpl;

class PerfDataClient {
 public:
  PerfDataClient(bool enabled, const std::string& server_address);
  virtual ~PerfDataClient();
  static PerfDataClient& GetPerfDataClient();

  void RegisterMetric(const std::string& name, double& value);
  void RegisterMetric(const std::string& name, std::function<double()> func);

  void DumpMetric(const std::string& prefix);

 private:
  bool enabled_;
  std::unique_ptr<PerfDataClientImpl> pimpl_;
};

template <typename T = uint64_t>
struct PerfCounter {
  static_assert(std::is_convertible<T, double>::value, "T must be convertible to double");

  std::string name_;
  T cnt_{};

  explicit PerfCounter(std::string name) : name_(name) {
    auto& client = PerfDataClient::GetPerfDataClient();
    client.RegisterMetric(name_, [this]() { return static_cast<double>(cnt_); });
  }
  ~PerfCounter() {}

  void Inc(T inc) { cnt_ += inc; }
  void Dec(T dec) { cnt_ -= dec; }
  const T& Value() const { return cnt_; }
  void Dump() { fprintf(stderr, "%s: %f\n", name_.c_str(), static_cast<double>(cnt_)); }

  PerfCounter& operator++() { ++cnt_; return *this; }
  PerfCounter& operator++(int) { ++cnt_; return *this; }
  PerfCounter& operator--() { --cnt_; return *this; }
  PerfCounter& operator--(int) { --cnt_; return *this; }
  PerfCounter& operator+=(T inc) { cnt_ += inc; return *this; }
  PerfCounter& operator-=(T dec) { cnt_ -= dec; return *this; }
};

#define PERFCOUNTER_DEF_T(type, hier, name) PerfCounter<type> name{hier #name}
#define PERFCOUNTER_DEF(hier, name) PERFCOUNTER_DEF_T(uint64_t, hier, name)
#define PERFCOUNTER_INC(name) do { name += 1; } while (0)
#define PERFCOUNTER_DEC(name) do { name -= 1; } while (0)


}  // namespace rocksdb
#endif  // ROCKSDB_PERF_DATA_CLIENT_H
