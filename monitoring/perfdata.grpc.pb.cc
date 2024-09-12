// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: perfdata.proto

#include "perfdata.pb.h"
#include "perfdata.grpc.pb.h"

#include <functional>
#include <grpcpp/support/async_stream.h>
#include <grpcpp/support/async_unary_call.h>
#include <grpcpp/impl/channel_interface.h>
#include <grpcpp/impl/client_unary_call.h>
#include <grpcpp/support/client_callback.h>
#include <grpcpp/support/message_allocator.h>
#include <grpcpp/support/method_handler.h>
#include <grpcpp/impl/rpc_service_method.h>
#include <grpcpp/support/server_callback.h>
#include <grpcpp/impl/server_callback_handlers.h>
#include <grpcpp/server_context.h>
#include <grpcpp/impl/service_type.h>
#include <grpcpp/support/sync_stream.h>
namespace rocksdb {

static const char* PerfDataService_method_names[] = {
  "/rocksdb.PerfDataService/SendPerfData",
};

std::unique_ptr< PerfDataService::Stub> PerfDataService::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< PerfDataService::Stub> stub(new PerfDataService::Stub(channel, options));
  return stub;
}

PerfDataService::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options)
  : channel_(channel), rpcmethod_SendPerfData_(PerfDataService_method_names[0], options.suffix_for_stats(),::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status PerfDataService::Stub::SendPerfData(::grpc::ClientContext* context, const ::rocksdb::PerfDataRequest& request, ::rocksdb::PerfDataResponse* response) {
  return ::grpc::internal::BlockingUnaryCall< ::rocksdb::PerfDataRequest, ::rocksdb::PerfDataResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), rpcmethod_SendPerfData_, context, request, response);
}

void PerfDataService::Stub::async::SendPerfData(::grpc::ClientContext* context, const ::rocksdb::PerfDataRequest* request, ::rocksdb::PerfDataResponse* response, std::function<void(::grpc::Status)> f) {
  ::grpc::internal::CallbackUnaryCall< ::rocksdb::PerfDataRequest, ::rocksdb::PerfDataResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_SendPerfData_, context, request, response, std::move(f));
}

void PerfDataService::Stub::async::SendPerfData(::grpc::ClientContext* context, const ::rocksdb::PerfDataRequest* request, ::rocksdb::PerfDataResponse* response, ::grpc::ClientUnaryReactor* reactor) {
  ::grpc::internal::ClientCallbackUnaryFactory::Create< ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(stub_->channel_.get(), stub_->rpcmethod_SendPerfData_, context, request, response, reactor);
}

::grpc::ClientAsyncResponseReader< ::rocksdb::PerfDataResponse>* PerfDataService::Stub::PrepareAsyncSendPerfDataRaw(::grpc::ClientContext* context, const ::rocksdb::PerfDataRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderHelper::Create< ::rocksdb::PerfDataResponse, ::rocksdb::PerfDataRequest, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(channel_.get(), cq, rpcmethod_SendPerfData_, context, request);
}

::grpc::ClientAsyncResponseReader< ::rocksdb::PerfDataResponse>* PerfDataService::Stub::AsyncSendPerfDataRaw(::grpc::ClientContext* context, const ::rocksdb::PerfDataRequest& request, ::grpc::CompletionQueue* cq) {
  auto* result =
    this->PrepareAsyncSendPerfDataRaw(context, request, cq);
  result->StartCall();
  return result;
}

PerfDataService::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      PerfDataService_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< PerfDataService::Service, ::rocksdb::PerfDataRequest, ::rocksdb::PerfDataResponse, ::grpc::protobuf::MessageLite, ::grpc::protobuf::MessageLite>(
          [](PerfDataService::Service* service,
             ::grpc::ServerContext* ctx,
             const ::rocksdb::PerfDataRequest* req,
             ::rocksdb::PerfDataResponse* resp) {
               return service->SendPerfData(ctx, req, resp);
             }, this)));
}

PerfDataService::Service::~Service() {
}

::grpc::Status PerfDataService::Service::SendPerfData(::grpc::ServerContext* context, const ::rocksdb::PerfDataRequest* request, ::rocksdb::PerfDataResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


}  // namespace rocksdb

