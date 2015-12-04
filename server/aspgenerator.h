/*
 * ASPGenerator.h
 *
 *  Created on: Mar 15, 2015
 *      Author: wontoniii
 */

#ifndef ASPGENERATOR_H_
#define ASPGENERATOR_H_

#include <mobilityfirst/mfapi.h>
#include <semaphore.h>
#include <popt.h>
#include <oml2/omlc.h>
#include <pthread.h>

#include "virtual-server_oml.h"
//
//#include "config.h"

class ASPGenerator {

public:
	ASPGenerator();
	~ASPGenerator();

	void logOneRequest();
	void setCurrentLoad(double load);

	void start();
	bool init();
	void stop();

	inline int getPeriod() const {
		return period;
	}

	inline void setPeriod(int period) {
		this->period = period;
	}

	void setMfHandle(struct Handle * mfHandle) {
		this->mfHandle = mfHandle;
	}

	inline int getGuid() const {
		return guid;
	}

	inline void setGuid(int guid) {
		this->guid = guid;
	}

	int getDestinationGuid() const {
		return destinationGuid;
	}

	void setDestinationGuid(int destinationGuid) {
		this->destinationGuid = destinationGuid;
	}

	oml_mps_t* getOmlMps() const {
		return oml_mps;
	}

	void setOmlMps(oml_mps_t* omlMps) {
		oml_mps = omlMps;
	}

	static void *startThread(void *argument);

	void mainLoop();

private:
	struct Handle * mfHandle;
	int guid;
	int destinationGuid;
	int period;
	int sequence;
	int requestsInPeriod;
	double load;
	volatile bool cont;
	sem_t wsem;
	oml_mps_t *oml_mps;
	pthread_t thread;

	void sendASP(double metric);
	void sendOMLData(double metric);
};

#endif /* ASPGENERATOR_H_ */
