###############################################################
#
# Purpose: Makefile for "server-OpenCV"
# Author.: Zihong Zheng
# Version: 0.1
# License: 
#
###############################################################

CC = g++

CFLAGS +=-c -g -O0 -Wall -std=c++11 -I$(MF_HOME)/common -I$(MF_HOME)/netapi/c -DLINUX -I/usr/local/include/librdkafka

LFLAGS = -lmfapi -lz -lpthread -lrt `pkg-config --cflags --libs opencv` -lcrypto -lrdkafka++ -lrdkafka -loml2 -locomm -lpopt

SOURCES = server-OpenCV.cpp ImgMatch.cpp MsgDistributor.cpp KafkaProducer.cpp Metrics.cpp MFPackager.cpp aspgenerator.cpp

OBJECTS = $(SOURCES:.cpp=.o)

SERVER  = virtual-server

SCAFFOLD = /usr/bin/oml2-scaffold

SRCS = $(wildcard *.cpp)
OBJS = $(SRCS:%.cpp=%.o)
SRCS_EX_LESS := $(filter-out server-OpenCV.cpp, $(SRCS)	)
OBJS_EX_LESS = $(SRCS_EX_LESS:%.cpp=%.o)

all: build
	
build: $(SERVER)
	
$(SERVER): $(OBJS) 
	$(CC) $(OBJECTS) -o $@ $(LFLAGS) 

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

$(SERVER)_popt.h: $(SERVER).rb
	$(SCAFFOLD) --pots $<
$(SERVER)_oml.h: $(SERVER).rb
	$(SCAFFOLD) --oml $<
run:
	./server-OpenCV

debug:
	./server-OpenCV -d

orbit:
	sudo ./server-OpenCV -orbit -m 102 -o 101

odebug:
	sudo ./server-OpenCV -orbit -m 102 -o 101 -d

clean:
	rm -rf *.o $(EXECUTABLE)
