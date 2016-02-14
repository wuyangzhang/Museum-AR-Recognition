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
private:
    int request_submitted;
    int request_finished;
    double max_num_of_requests = 100;
    std::queue<struct timeval> requestStartTime;
    std::list<double> requestConsumingTime;
};

#endif /* METRICS_H */
