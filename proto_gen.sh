protoc --cpp_out=./ ffmpeg.proto
protoc --grpc_out=./ --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` ffmpeg.proto

