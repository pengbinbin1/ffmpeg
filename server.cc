#include <iostream>
#include <memory.h>
#include <string.h>

#include <grpcpp/grpcpp.h>
#include "ffmpeg.grpc.pb.h"
#include "ffmpeg.pb.h"

extern "C" {

/*Include ffmpeg header file*/
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/opt.h>
    #include <libavutil/mathematics.h>
    #include <libavutil/samplefmt.h>
    #include <libavfilter/avfilter.h>
    #include <libavdevice/avdevice.h>
}

using namespace std;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using FfmpegPck::Empty;
using FfmpegPck::Ffmpeg;
using FfmpegPck::ErrCode;

void FfmpegInit()
{
    av_register_all();
    avfilter_register_all();
    avformat_network_init();
    avdevice_register_all();
    av_log_set_level(AV_LOG_ERROR);
}

class FfmpegServiceImpl final : public Ffmpeg::Service {
    Status Init(ServerContext* context, const Empty* request, ErrCode* replay ){
        FfmpegInit();
        replay->set_err(0);
        return Status::OK;
    }
};

void RunServer() {
    std::string server_address("0.0.0.0:8099");
    FfmpegServiceImpl service;

    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&service);
    // Finally assemble the server.
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;

    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    server->Wait();
}

int main()
{
    RunServer();
    return 0;
}
