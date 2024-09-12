#include "rocksdb/perf_data_client.h"

namespace rocksdb {


class PerfDataClientImpl {
};

PerfDataClient::PerfDataClient(bool enabled,
                               const std::string& server_address) {
}

PerfDataClient::~PerfDataClient() = default;

PerfDataClient& PerfDataClient::GetPerfDataClient() {
  static PerfDataClient client(false, "");
  return client;
}

void PerfDataClient::RegisterMetric(const std::string& name, double& value) {
}

void PerfDataClient::RegisterMetric(const std::string& name,
                                    std::function<double()> func) {
}

void PerfDataClient::DumpMetric(const std::string& prefix) {
}

}  // namespace rocksdb
