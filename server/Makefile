
CC = g++

CFLAGS +=-c -g -O0 -Wall -std=c++11 -I$(MF_HOME)/common -I$(MF_HOME)/netapi/c -DLINUX -I/usr/local/include/librdkafka

LFLAGS = -lmfapi -lz -lpthread -lrt `pkg-config --cflags --libs opencv` -lcrypto -lrdkafka++ -lrdkafka -loml2 -locomm -lpopt

SOURCES = virtual-server.cpp ImgMatch.cpp MsgDistributor.cpp KafkaProducer.cpp Metrics.cpp MFPackager.cpp aspgenerator.cpp

OBJECTS = $(SOURCES:.cpp=.o)

SERVER  = virtual-server

SCAFFOLD = /usr/bin/oml2-scaffold

SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:%.cpp=%.o)
SRCS_EX_LESS := $(filter-out $(SERVER).cpp, $(SRCS))
OBJS_EX_LESS = $(SRCS_EX_LESS:%.cpp=%.o)

all: build
	
build: $(SERVER)
	
$(SERVER): config.h $(SERVER)_popt.h $(SERVER)_oml.h $(OBJS) 
	$(info "Generating executalbe virtual-server")
	$(CC) -o $@ $(OBJS_EX_LESS) $(SERVER).o $(LFLAGS) 

.cpp.o:
	$(CC) $(CFLAGS) -c $< -o $@

$(SERVER)_popt.h: $(SERVER).rb
	$(SCAFFOLD) --opts $<

$(SERVER)_oml.h: $(SERVER).rb
	$(SCAFFOLD) --oml $<
clean:
	rm -rf *.o $(SERVER)
