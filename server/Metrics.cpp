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
	if(!requestStartTime.empty()){
		struct timeval startTime = requestStartTime.front();
		requestStartTime.pop();
		return startTime;
	}
	struct timeval startTime;
	gettimeofday(&startTime,NULL);
        return startTime;

}

double Metrics::getRequestConsumingTime(struct timeval tpend){
	struct timeval tpstart = getRequestStartTime();
	double timeuse = (1000000*(tpend.tv_sec-tpstart.tv_sec)+tpend.tv_usec-tpstart.tv_usec) / 1000; 
	printf("Test Request Consuming Time Before submit to calculate average %f\n", timeuse);
	return timeuse;
}

void Metrics::submitRequestConsumingTime(double requestConsumingTime){
	this->requestConsumingTime.push_back(requestConsumingTime);
}

double Metrics::getAverageRequestConsumingTime(unsigned int sizeWindow){
	if(requestConsumingTime.size() > sizeWindow){
		requestConsumingTime.pop_front();
	}

	if(!requestConsumingTime.empty()){
		double averageTime = std::accumulate(requestConsumingTime.begin(),requestConsumingTime.end(),0.0) / requestConsumingTime.size();
	  	printf("Test average time %f\n", averageTime);
		return averageTime;
	}else{
		return 0;
	}
}

double Metrics::getAsrMetric(double aveProcessingTime, double maxProcessingTime){
	if(aveProcessingTime < maxProcessingTime)
		return aveProcessingTime / maxProcessingTime;
	else
		return 1;
}

void Metrics::writeMetricToFile(unsigned int matchedIndex, double singleProcessingTime, double asrMetric){
	std::ofstream out;
	out.open("asrReport", std::ofstream::out | std::ofstream::app);
	out <<"\t\tASR Report\n";
	out <<"matched index: "<< matchedIndex <<", single processing time: "<< singleProcessingTime<<", asr metric: "<< asrMetric<<"\n";
	out.close();
	
}


