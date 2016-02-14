/*************************************************
#
# Purpose: "Metrics" aims to maintain the metrics for CPS server
# Author.: Zihong Zheng (zzhonzi@gmail.com)
# Version: 0.1
# License: 
#
*************************************************/

#include "Metrics.h"

Metrics::Metrics() {
	request_submitted = 0;
	request_finished = 0;
}

Metrics::~Metrics() {

}

void Metrics::submit_request() {
	++request_submitted;
}

void Metrics::finish_request() {
	++request_finished;
}

int Metrics::get_num_ongoing() {
	return request_submitted - request_finished;
}

double Metrics::get_metrics() {
	if (request_submitted == 0) {
		return 0;
	}
	int curNum = get_num_ongoing();
	if (curNum >= max_num_of_requests) {
		return 1;
	}
	return curNum / max_num_of_requests;
}

void Metrics::submitRequestStartTime(struct timeval tpstart){
	requestStartTime.push(tpstart);
}

struct timeval Metrics::getRequestStartTime(){
	struct timeval startTime = requestStartTime.front();
	requestStartTime.pop();
	return startTime;
}

double Metrics::getRequestConsumingTime(struct timeval tpend){
	struct timeval tpstart = getRequestStartTime();
	double timeuse = 1000*(tpend.tv_sec-tpstart.tv_sec)+tpend.tv_usec-tpstart.tv_usec; //request use time in million seconds
	return timeuse;
}

void Metrics::submitRequestConsumingTime(double requestConsumingTime){
	requestConsumingTime.push_back(getRequestConsumingTime);
}

double Metrics::getAverageRequestConsumingTime(int sizeWindow){
	if(requestConsumingTime.size() > sizeWindow){
		requestConsumingTime.pop_front();
	}

	if(!requestConsumingTime.empty()){
		return std::accumulate(requestConsumingTime.begin(),requestConsumingTime.end(),0.0) / requestConsumingTime.size();
	}else{
		return 0;
	}
}
