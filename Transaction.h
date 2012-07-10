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

#ifndef TRANSACTION_H
#define TRANSACTION_H

//Transaction header

#include "Globals.h"

using namespace std;

namespace BOBSim
{

static unsigned globalID=0;

enum TransactionType
{
	DATA_READ,
	DATA_WRITE,
	RETURN_DATA,
	LOGIC_OPERATION,
	LOGIC_RESPONSE
};

class Transaction
{
public:
	//Functions
	Transaction(TransactionType transType, unsigned size, uint64_t addr);
	Transaction();

	//Fields
	//Type of transaction (defined above)
	TransactionType transactionType;
	//Physical address of request
	uint64_t address;
	//Channel used to service request
	unsigned mappedChannel;
	//Size of data
	unsigned transactionSize;
	//Unique identifier 
	unsigned transactionID;
	//Port ID used to send request
	unsigned portID;
	//CPU Core which generated request
	unsigned coreID;
	void *logicOpContents;
	bool originatedFromLogicOp;

	//Latency break-up
	uint64_t cyclesReqPort; //add to Port until remove from port
	uint64_t cyclesRspPort; //add to port until remove from port

	uint64_t cyclesReqLink; //add to request SerDe until remove from SerDe
	uint64_t cyclesRspLink; //add to response SerDe until remove

	unsigned cyclesInReadReturnQ;
	unsigned cyclesInWorkQueue;

	uint64_t fullStartTime; //start sending out on port until...
	uint64_t fullTimeTotal; //analyzed in TraceBasedSim

	uint64_t dramStartTime; //ACTIVATE is issued until...
	uint64_t dramTimeTotal; //READ_DATA packet in queue

	uint64_t channelStartTime; //start sending on BOB Channel bus until...
	uint64_t channelTimeTotal; //completely added to SerDeDownBuffer
};

ostream& operator<<(ostream &out, const Transaction &t);
}

#endif
