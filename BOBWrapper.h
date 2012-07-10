/*********************************************************************************
*  Copyright (c) 2010-2011, Elliott Cooper-Balis
*                             Paul Rosenfeld
*                             Bruce Jacob
*                             University of Maryland 
*                             dramninjas [at] gmail [dot] com
*  All rights reserved.
*  
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are met:
*  
*     * Redistributions of source code must retain the above copyright notice,
*        this list of conditions and the following disclaimer.
*  
*     * Redistributions in binary form must reproduce the above copyright notice,
*        this list of conditions and the following disclaimer in the documentation
*        and/or other materials provided with the distribution.
*  
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
*  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
*  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
*  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
*  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
*  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
*  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
*  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*********************************************************************************/

#ifndef BOBWRAPPER_H
#define BOBWRAPPER_H

#include <deque>
#include "SimulatorObject.h"
#include "BOB.h"
#include <math.h>
#include "Callback.h"
#include "Transaction.h"


using namespace std;


namespace BOBSim
{
class BOBWrapper : public SimulatorObject
{
private:
	unsigned TryToSendPending();
public:
	//Functions
	BOBWrapper(uint64_t qemu_memory_size);
	void Update();
	bool AddTransaction(Transaction* trans, unsigned port);
	bool AddTransaction(uint64_t addr, bool isWrite, int coreID, void *logicOp);
	void RegisterCallbacks(
	    TransactionCompleteCB *readDone,
	    TransactionCompleteCB *writeDone,
	    LogicOperationCompleteCB *logicDone);
	void PrintStats(bool finalPrint);
	void UpdateLatencyStats(Transaction *trans);
	int FindOpenPort(uint coreID);
	bool isPortAvailable(unsigned port);

	//Fields
	//BOB object
	BOB *bob;
	//Incoming transactions being sent to each port
	vector<Transaction*> inFlightRequest;
	//Counters for determining how long packet should be sent
	vector<unsigned> inFlightRequestCounter;
	vector<unsigned> inFlightRequestHeaderCounter;

	//Outgoing transactiong being sent to cache
	vector<Transaction*> inFlightResponse;
	//Counters for determining how long packet should be sent
	vector<unsigned> inFlightResponseCounter;
	vector<unsigned> inFlightResponseHeaderCounter;

	//Bookkeeping on request stream
	uint64_t maxReadsPerCycle;
	uint64_t maxWritesPerCycle;
	uint64_t readsPerCycle;
	uint64_t writesPerCycle;

	//Bookkeeping and statistics
	vector<unsigned> requestPortEmptyCount;
	vector<unsigned> responsePortEmptyCount;
	vector<unsigned> requestCounterPerPort;
	vector<unsigned> readsPerPort;
	vector<unsigned> writesPerPort;
	vector<unsigned> returnsPerPort;

	//Callback functions
	TransactionCompleteCB *readDoneCallback;
	TransactionCompleteCB *writeDoneCallback;
	LogicOperationCompleteCB *logicDoneCallback;

	//More bookkeeping and statistics
	vector< vector<unsigned> > perChanFullLatencies;
	vector< vector<unsigned> > perChanReqPort;
	vector< vector<unsigned> > perChanRspPort;
	vector< vector<unsigned> > perChanReqLink;
	vector< vector<unsigned> > perChanRspLink;
	vector< vector<unsigned> > perChanAccess;
	vector< vector<unsigned> > perChanRRQ;
	vector< vector<unsigned> > perChanWorkQTimes;

	vector<unsigned> fullLatencies;
	unsigned fullSum;

	vector<unsigned> dramLatencies;
	unsigned dramSum;

	vector<unsigned> chanLatencies;
	unsigned chanSum;

	unsigned issuedLogicOperations;
	unsigned issuedWrites;
	unsigned committedWrites;
	unsigned returnedReads;
	unsigned totalReturnedReads;
	unsigned totalIssuedWrites;
	unsigned totalTransactionsServiced;
	unsigned totalLogicResponses;

	//Output files
	ofstream statsOut;
	ofstream powerOut;

	//Callback
	void WriteIssuedCallback(unsigned port, uint64_t address);
	
	//Round-robin counter
	uint portRoundRobin;

};
BOBWrapper *getMemorySystemInstance(uint64_t qemu_mem_size);
void *getPageWalkLogicOp(uint64_t baseAddr, vector<uint64_t> *args);
}


#endif
