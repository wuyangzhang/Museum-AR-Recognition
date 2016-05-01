/*************************************************
#
# Purpose: header for "Metrics Class"
# Author.: Wuyang Zhang
# Version: 0.1
# License: 
#
*************************************************/

#ifndef METRICS_H
#define METRICS_H

#include <queue>
#include <list>
#include <numeric>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <fstream>
class Metrics {
public:
    Metrics();
    ~Metrics();
    void submit_request();
    void finish_request();
    int get_num_ongoing();
    double get_metrics();

//process historical processing time 
    void submitRequestStartTime(struct timeval tpstart);
    struct timeval getRequestStartTime();
    double getRequestConsumingTime(struct timeval tpend);
    void submitRequestConsumingTime(double requestConsumingTime);
    double getAverageRequestConsumingTime(unsigned int sizeWindow);
    double getAsrMetric(double aveProcessingTime, double maxProcessingTime);
    void writeMetricToFile(unsigned int matchedIndex, double singleProcessingTime, double asrMetric);
private:
    int request_submitted;
    int request_finished;
    double max_num_of_requests = 100;
    std::queue<struct timeval> requestStartTime;
    std::list<double> requestConsumingTime;
    std::ofstream out;
};

#endif /* METRICS_H */
