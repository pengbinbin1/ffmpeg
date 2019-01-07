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
using FfmpegPck::InputParam;
using FfmpegPck::OutputParam;


//set dictionary when open the input
struct OpenDic{
    string maxdelay;
    string timeout;
    string protocol;
    string codec ;
};


class FfmpegServiceImpl final : public Ffmpeg::Service {
    Status Init(ServerContext* context, const Empty* request, ErrCode* replay ){
        FfmpegInit();
        replay->set_err(0);
        return Status::OK;
    }

    Status OpenInput(ServerContext* context, const InputParam* request, ErrCode* replay){
        string inputUrl = request->inputurl();
        OpenDic dic;
        char delay[16] = {0};
        char timeout[16] = {0};
        dic.codec = request->codec();
        dic.protocol = request->protocol();
        int maxdelay = request->maxdelay();
        sprintf(delay,"%d",maxdelay);
        int to = request->timeout();
        sprintf(timeout,"%d",to);
        dic.maxdelay = delay;
        dic.timeout = timeout;

        int ret = OpenSource(inputUrl,dic);
        if (ret != 0)
        {
            cout << "Open input:"<<inputUrl<<" failed,errCode = "<<ret<<endl;
        }

        replay->set_err(ret);
        return Status::OK;
    }

    Status OpenOutput(ServerContext* context,const OutputParam* request, ErrCode* replay){
        int ret = 0;
        string url = request->outputurl();
        string formatName = request->formatname();
        ret = OpenDes(url,formatName);
        if(ret != 0)
        {
            cout<<"Open output:"<<url<<" failed,errCode = "<<ret<<endl;
        }

        replay->set_err(ret);
        return Status::OK;
    }

private:
    void FfmpegInit()
    {
        av_register_all();
        avfilter_register_all();
        avformat_network_init();
        avdevice_register_all();
        av_log_set_level(AV_LOG_ERROR);
    }

    int OpenSource(string inputUrl,OpenDic dic){
        int ret = 0;
        m_inputContext = avformat_alloc_context();
        //inputContext->interrupt_callback.callback = interrupt_cb;
        AVInputFormat *ifmt = av_find_input_format("dshow");

        //set option
        AVDictionary* option = NULL;
        av_dict_set(&option,"buffer_sizes","102400",0);
        av_dict_set(&option,"rtsp_transport",dic.protocol.c_str(),0);
        av_dict_set(&option,"stimeout",dic.timeout.c_str(),0);
        av_dict_set(&option,"maxdelay",dic.maxdelay.c_str(),0);
        av_dict_set(&option,"vcodec",dic.codec.c_str(),0);

        ret = avformat_open_input(&m_inputContext, inputUrl.c_str(), ifmt,&option);
        if(ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Input file open input failed\n");
            return  ret;
        }

        ret = avformat_find_stream_info(m_inputContext,nullptr);
        if(ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Find input file stream inform failed\n");
        }
        else
        {
            av_log(NULL, AV_LOG_FATAL, "Open input file  %s success\n",inputUrl.c_str());
        }
        return ret;
    }

    //open the dest about push the stream
    int OpenDes(string outPuturl,string formatType){
        int ret = 0;

        //allcate bufffer
        ret  = avformat_alloc_output_context2(&m_outputContext, nullptr,formatType.c_str(), outPuturl.c_str());
        if(ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "open output context failed\n");
           // add some err process
            return ret;
        }

        ret = avio_open2(&m_outputContext->pb, outPuturl.c_str(), AVIO_FLAG_WRITE,nullptr, nullptr);
        if(ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "open avio failed");
            //add some err process
            return ret;
        }

        for(int i = 0; i < m_inputContext->nb_streams; i++)
        {
            if(m_inputContext->streams[i]->codec->codec_type == AVMediaType::AVMEDIA_TYPE_AUDIO)
            {
                continue;
            }
            AVStream * stream = avformat_new_stream(m_outputContext, m_encodeContext->codec);
            ret = avcodec_copy_context(stream->codec, m_encodeContext);
            if(ret < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "copy coddec context failed");
               // add some err process
                return ret;
            }
        }

        ret = avformat_write_header(m_outputContext, nullptr);
        if(ret < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "format write header failed");
            //...........
            return ret;
        }

        av_log(NULL, AV_LOG_FATAL, " Open output file success %s\n",outPuturl.c_str());
        return ret ;
    }

    //Close input source
    void CloseInput(){
        if (m_inputContext!= NULL)
        {
            avformat_close_input(&m_inputContext);
        }
    }

    //Close output source
    int CloseOutput(){
        int ret = 0;
        if(m_outputContext != nullptr)
        {

            ret = av_write_trailer(m_outputContext);
            if(ret != 0)
            {
                 av_log(NULL, AV_LOG_ERROR, "format write header failed");
                 return ret;
            }
            for(int i = 0 ; i < m_outputContext->nb_streams; i++)
            {
                AVCodecContext *codecContext = m_outputContext->streams[i]->codec;
                avcodec_close(codecContext);
            }
            avformat_close_input(&m_outputContext);
        }

        return ret;
    }
private:
    OpenDic m_dic;
    AVFormatContext* m_inputContext = nullptr;
    AVFormatContext* m_outputContext = nullptr;
    AVCodecContext* m_encodeContext = nullptr;

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
