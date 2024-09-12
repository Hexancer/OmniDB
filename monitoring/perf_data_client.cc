#include "rocksdb/perf_data_client.h"

#include <grpcpp/grpcpp.h>

#include "perfdata.grpc.pb.h"
#include "rocksdb/omnicache.h"

namespace rocksdb {

class PerfDataClientImpl {
 public:
  PerfDataClientImpl(const std::string& server_address) : stopped_(false) {
    addr_ = server_address;
    thread_ = std::thread(&PerfDataClientImpl::SendThreadFunc, this);
  }

  void Initialize() {
    static bool inited = false;
    if (!inited) {
      channel_ = grpc::CreateChannel(addr_, grpc::InsecureChannelCredentials());
      stub_ = PerfDataService::NewStub(channel_);
      inited = true;
    }
  }

  ~PerfDataClientImpl() {
    stopped_ = true;
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  static PerfDataClient& GetPerfDataClient() {
    static PerfDataClient client(OmniCacheEnv::GetOmniCacheEnv().perfEnabled,
                                 OmniCacheEnv::GetOmniCacheEnv().perfServer);
    return client;
  }

  bool IsServerRunning() {
    grpc_connectivity_state state = channel_->GetState(true);
    return (state == GRPC_CHANNEL_READY);
  }

  void RegisterMetric(const std::string& name, double& value) {
    valMetrics_.emplace_back(name, value);
  }

  void RegisterMetric(const std::string& name, std::function<double()> func) {
    funcMetrics_.emplace_back(name, func);
  }

  void DumpMetric(const std::string& prefix) {
    for (const auto& metric : valMetrics_) {
      if (metric.first.rfind(prefix, 0) == 0) {
        std::cerr << metric.first << ": " << metric.second << "\n";
      }
    }
    for (const auto& metric : funcMetrics_) {
      if (metric.first.rfind(prefix, 0) == 0) {
        std::cerr << metric.first << ": " << metric.second() << "\n";
      }
    }
  }

  PerfResponseStatus SendPerfData(double timestamp) {
    Initialize();

    if (!IsServerRunning()) {
      return SERVER_NOT_REACHABLE;
    }

    PerfDataRequest request;
    for (const auto& metric : valMetrics_) {
      Metric* m = request.add_metrics();
      m->set_key(metric.first);
      m->set_value(metric.second);
    }
    for (const auto& metric : funcMetrics_) {
      Metric* m = request.add_metrics();
      m->set_key(metric.first);
      m->set_value(metric.second());
    }
    request.set_timestamp(timestamp);

    PerfDataResponse response;
    grpc::ClientContext context;

    grpc::Status status = stub_->SendPerfData(&context, request, &response);

    if (status.ok()) {
      return response.status();
    } else {
      return FAILURE;
    }
  }

 private:
  void SendThreadFunc() {
    using namespace std::chrono;

    while (!stopped_) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
      duration secs = duration<double>(system_clock::now().time_since_epoch());
      SendPerfData(secs.count());
    }
  }

  std::unique_ptr<PerfDataService::Stub> stub_;
  std::shared_ptr<grpc::Channel> channel_;

  std::vector<std::pair<std::string, double&>> valMetrics_;
  std::vector<std::pair<std::string, std::function<double()>>> funcMetrics_;
  bool stopped_;
  std::thread thread_;
  std::string addr_;
};

PerfDataClient::PerfDataClient(bool enabled,
                               const std::string& server_address) {
  enabled_ = enabled;
  if (enabled_) {
    pimpl_ = std::make_unique<PerfDataClientImpl>(server_address);
  }
}

PerfDataClient::~PerfDataClient() = default;

PerfDataClient& PerfDataClient::GetPerfDataClient() {
  return PerfDataClientImpl::GetPerfDataClient();
}

void PerfDataClient::RegisterMetric(const std::string& name, double& value) {
  if (enabled_) {
    pimpl_->RegisterMetric(name, value);
  }
}

void PerfDataClient::RegisterMetric(const std::string& name,
                                    std::function<double()> func) {
  if (enabled_) {
    pimpl_->RegisterMetric(name, func);
  }
}

void PerfDataClient::DumpMetric(const std::string& prefix) {
  if (enabled_) {
    pimpl_->DumpMetric(prefix);
  }
}

}  // namespace rocksdb
