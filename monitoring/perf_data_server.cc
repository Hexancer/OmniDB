#include "perfdata.grpc.pb.h"

#include <grpcpp/grpcpp.h>

#include <iostream>
#include <memory>
#include <string>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using rocksdb::PerfDataService;
using rocksdb::PerfDataRequest;
using rocksdb::PerfDataResponse;
using rocksdb::Metric;
using rocksdb::PerfResponseStatus;

class PerfDataServiceImpl final : public PerfDataService::Service {
  Status SendPerfData(ServerContext* context, const PerfDataRequest* request, PerfDataResponse* response) override {
    std::cout << "Received performance data at timestamp: " << request->timestamp() << std::endl;
    for (const Metric& metric : request->metrics()) {
      std::cout << metric.key() << ": " << metric.value() << std::endl;
    }
    response->set_status(PerfResponseStatus::SUCCESS);
    return Status::OK;
  }
};

void RunServer() {
  std::string server_address("0.0.0.0:50051");
  PerfDataServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  server->Wait();
}

int main(int argc, char** argv) {
  RunServer();
  return 0;
}
