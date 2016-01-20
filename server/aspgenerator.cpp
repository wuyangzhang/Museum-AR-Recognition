/*
 * ASPGenerator.cpp
 *
 *  Created on: Mar 15, 2015
 *      Author: wontoniii
 *	send a value ( metric * UINT_NMAX ) to access router specified by a GUID
 */



#include <mobilityfirst/mfapi.h>
#include <mobilityfirst/mfflags.h>
#include <include/mfvirtualctrlmsg.hh>
#include <semaphore.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <climits>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>     // std::cout, std::ios
#include <sstream>

#include "aspgenerator.h"

ASPGenerator::ASPGenerator() {
	// TODO Auto-generated constructor stub
	requestsInPeriod = 0;
	sequence = 0;
	cont = false;
}

ASPGenerator::~ASPGenerator() {
	// TODO Auto-generated destructor stub
}

void ASPGenerator::logOneRequest(){
	sem_wait(&wsem);
	requestsInPeriod ++;
	sem_post(&wsem);
}

void ASPGenerator::setCurrentLoad(double load){
	sem_wait(&wsem);
	this->load = load;
	sem_post(&wsem);
}

//struct VIRTUAL_NETWORK_ASP {
//  uint32_t type;
//  uint32_t senderVirtualLSA;
//  uint32_t seq;
//  uint32_t nodeMetric;
//};

/* 
	send out ASP packet to network
	guid, destinationGuid (destinationGUID is )
	
*/
void ASPGenerator::sendASP(double metric) {
	unsigned int reqps = (unsigned int)(metric * (double)UINT_MAX);
	requestsInPeriod = 0;
	/*virtual network asp [type, senderVirtualGuild, sequence(number of asp), nodeMetric], destinationGUID->access router*/
	virtual_network_asp_t asp;
	asp.type = VIRTUAL_ASP_PKT;
	asp.senderVirtual = guid;
	asp.seq = sequence;
	asp.nodeMetric = reqps;

	std::cout << "Sending ASP message " << sequence << " with metric value " << reqps
		  << " given " << requestsInPeriod << " requests in period " << "destinationGuid"<<destinationGuid<<std::endl;
	
	mfsend(mfHandle, (void *)&asp, VIRTUAL_ASP_SIZE, destinationGuid, MF_VIRTUAL_ASP);
	sequence ++;
}

void ASPGenerator::sendOMLData(double metric) {
	std::string Result;          // string which will contain the result
	std::ostringstream convert;   // stream used for the conversion
	convert << guid;      // insert the textual representation of 'Number' in the characters in the stream
	Result = convert.str();
	/* The oml_inject_MPNAME() helpers are defined in virtual-server_oml.h*/
	std::cout << "Injecting into OML" << std::endl;
	if(oml_inject_app_metrics(oml_mps->app_metrics, Result.c_str(), metric) != 0) {
	  logwarn("Failed to inject data into MP 'app_metrics'\n");
	}
}

bool ASPGenerator::init() {
	requestsInPeriod = 0;
	sequence = 0;
	if(!sem_init(&wsem, 0, 1)){
		return false;
	}
	return true;
}

/* sendASP(load) : load -> metrics */
void ASPGenerator::mainLoop() {
  
	std::cout << "Start ASP generator with period " << period << std::endl;
	cont = true;
	while(cont){
		// Zzz TODO
		//usleep(period*1000);
		usleep(1000*1000);
		sem_wait(&wsem);
//		unsigned int reqs = (requestsInPeriod > 100) ? 100 : (unsigned int)requestsInPeriod;
//		double metric = (double) reqs / (double) 100;
		sem_post(&wsem);
		if(cont){
		//	sendOMLData(metric);
			//Zzz TODO
		  //			sendASP(load);
		  sendASP(0.2);
		      
		}
	}
}

void ASPGenerator::stop() {
	cont = false;
}

void ASPGenerator::start() {
	int error = pthread_create(&thread, NULL, &startThread, this);
	if (error) {
		 std::cerr << "Thread creation failed" << std::endl;
	}
}

void *ASPGenerator::startThread(void *argument){
	ASPGenerator *worker = (ASPGenerator *)argument;
    worker->mainLoop();
	return NULL;
}


