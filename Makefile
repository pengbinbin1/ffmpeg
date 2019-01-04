CC = g++
FFMPEGLIBS = -lavcodec -lavdevice -lavutil -lavfilter -lavformat 
%.o:%.cc
	$(CC) -std=c++11 -I /usr/local/include -pthread -c $< -o $@
all:server 

server:ffmpeg.grpc.pb.o ffmpeg.pb.o server.o
	$(CC) $^ -L /usr/local/lib `pkg-config --libs grpc++ grpc` -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed -lprotobuf $(FFMPEGLIBS) -lpthread -ldl -lssl -o $@ 

clean:
	rm -f *.o
