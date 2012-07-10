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

#include "LogicLayerInterface.h"

using namespace BOBSim;
using namespace std;

LogicLayerInterface::LogicLayerInterface(uint id):
	simpleControllerID(id),
	currentClockCycle(0),
	currentTransaction(NULL),
	currentLogicOperation(NULL),
	issuedRequests(0)
{

}

void LogicLayerInterface::RegisterReturnCallback(Callback<DRAMChannel, bool, Transaction*, unsigned> *returnCallback)
{
	ReturnToSimpleController = returnCallback;
}

void LogicLayerInterface::ReceiveLogicOperation(Transaction *trans, unsigned i)
{
	if (DEBUG_LOGIC) DEBUG("== Received in logic layer "<<simpleControllerID<<" on cycle "<<currentClockCycle<<" : "<<*trans);
	if (trans->transactionType == LOGIC_OPERATION)
	{
		pendingLogicOpsQueue.push_back(trans);
	}
	else
	{
		newOperationQueue.push_back(trans);
	}
}

void LogicLayerInterface::Update()
{
	//send back to channel if there is something in the outgoing queue
	if(outgoingQueue.size()>0 && (*ReturnToSimpleController)(outgoingQueue[0],0))
	{
		//if we just send the response, we are done and can clear the current stuff
		if(outgoingQueue[0]->transactionType==LOGIC_RESPONSE)
		{
			currentTransaction = NULL;
			currentLogicOperation = NULL;
		}

		outgoingQueue.erase(outgoingQueue.begin());
	}

	// if we aren't working on anything and there are logic ops waiting
	if (currentTransaction == NULL && !pendingLogicOpsQueue.empty())
	{
		//cast logic operation arguments from transaction
		if(pendingLogicOpsQueue[0]->logicOpContents != NULL)
		{

			//extract logic operation from transaction and grab handle to current operations
			currentLogicOperation = (LogicOperation *)pendingLogicOpsQueue[0]->logicOpContents;
			currentTransaction = pendingLogicOpsQueue[0];

			if(DEBUG_LOGIC) DEBUG(" == In logic layer "<<simpleControllerID<<" : interpreting transaction "<<*currentTransaction);

			// grabbed the pointers, now we can remove from queue
			pendingLogicOpsQueue.pop_front();


			//figure out what to do for each type
			switch(currentLogicOperation->logicType)
			{
			case LogicOperation::PAGE_FILL:
				//
				//Arguments:  1) Number of continuous pages
				//            2) Pattern to copy
				//
				uint64_t pageStart;
				uint64_t numPages;
				uint pattern;
				if(currentLogicOperation->arguments.size()!=2)
				{
					ERROR("== ERROR - Incorrect number of arguments for logic operation "<<currentLogicOperation->logicType);
					exit(-1);
				}

				//get arguments
				pageStart = currentTransaction->address;
				numPages = currentLogicOperation->arguments[0];
				pattern = (uint)currentLogicOperation->arguments[1];

				if(DEBUG_LOGIC)
				{
					DEBUG("       PAGE FILL");
					DEBUG("        Start     : "<<pageStart);
					DEBUG("        Num Pages : "<<numPages);
					DEBUG("        Fill with : "<<pattern);
				}

				//generate the writes required to write a pattern to each page
				for(int i=0; i<numPages; i++)
				{
					Transaction *trans = new Transaction(DATA_WRITE, 64, pageStart + i*(1<<(log2(BUS_ALIGNMENT_SIZE)+log2(NUM_CHANNELS))));
					trans->originatedFromLogicOp = true;

					if(DEBUG_LOGIC) DEBUG("      == Logic Op created : "<<*trans);

					outgoingQueue.push_back(trans);

					//pattern not used lol :(
				}

				//put the response at the back of the queue so when all the writes empty out, the response is ready to go
				currentTransaction->transactionType = LOGIC_RESPONSE;
				outgoingQueue.push_back(currentTransaction);

				break;
			case LogicOperation::MEM_COPY:
				//
				//Arguments : 1) Destination Address
				//            2) Size
				//

				uint64_t sourceAddress;
				uint64_t destinationAddress; //start of destination copy
				uint64_t sizeToCopy; //number of 64-byte words to copy
				if(currentLogicOperation->arguments.size()!=2)
				{
					ERROR("== ERROR - Incorrect number of arguments for logic operation "<<currentLogicOperation->logicType);
					exit(-1);
				}

				sourceAddress = currentTransaction->address;
				destinationAddress = currentLogicOperation->arguments[0];
				sizeToCopy = currentLogicOperation->arguments[1];

				if(DEBUG_LOGIC)
				{
					DEBUG("       MEM COPY");
					DEBUG("        Source Address      : 0x"<<hex<<setw(8)<<setfill('0')<<sourceAddress);
					DEBUG("        Dest Address        : 0x"<<hex<<setw(8)<<setfill('0')<<destinationAddress);
					DEBUG("        Size to copy (x64B) : "<<dec<<sizeToCopy);
				}

				for(int i=0; i<sizeToCopy; i++)
				{
					Transaction *trans = new Transaction(DATA_READ, 64, sourceAddress + i*(1<<(log2(BUS_ALIGNMENT_SIZE)+log2(NUM_CHANNELS))));
					trans->originatedFromLogicOp = true;

					if(DEBUG_LOGIC) DEBUG("      == Logic Op created : "<<*trans);

					outgoingQueue.push_back(trans);
				}

				break;
			case LogicOperation::PAGE_TABLE_WALK:
				uint64_t src_address;
				src_address = currentTransaction->address;
				for (size_t i=0; i<currentLogicOperation->arguments.size(); i++)
				{
					Transaction *trans = new Transaction(DATA_READ, 64, currentLogicOperation->arguments[i]);
					trans->originatedFromLogicOp = true;
					outgoingQueue.push_back(trans);
				}

				break;
			default:
				ERROR("== ERROR - Unknown logic type in logic layer : "<<currentLogicOperation->logicType);
				exit(-1);
			}

		}
		else
		{
			ERROR(" == Error - Logic operation has no contents : "<<*newOperationQueue[0]);
			exit(-1);
		}
	}


	//if we're getting data, it is probably from a logic op that generated requests
	if(!newOperationQueue.empty() && newOperationQueue[0]->transactionType == RETURN_DATA)
	{
		//
		//do some checks to ensure the data makes sense
		//
		if(currentTransaction==NULL)
		{
			ERROR("== ERROR - Getting data without a corresponding transaction");
			exit(-1);
		}

		if (DEBUG_LOGIC) DEBUG("   ==[L:"<<simpleControllerID<<"] oh hai, return data"<<*newOperationQueue[0]);
		//handle the return data based on the logic op that is being executed
		Transaction *t;
		switch(currentLogicOperation->logicType)
		{
		case LogicOperation::MEM_COPY:
			if (DEBUG_LOGIC) DEBUG(newOperationQueue[0]->address - currentTransaction->address);
			t = new Transaction(DATA_WRITE, 64, newOperationQueue[0]->address - currentTransaction->address + currentLogicOperation->arguments[0]);
			t->originatedFromLogicOp = true;

			if(DEBUG_LOGIC) DEBUG("      == Logic Op Copy moved data from 0x"<<hex<<setw(8)<<setfill('0')<<newOperationQueue[0]->address<<dec<<" to : "<<*t);

			outgoingQueue.push_back(t);

			issuedRequests++;

			newOperationQueue.erase(newOperationQueue.begin());

			//check to see if we are done with the requests
			if(issuedRequests==currentLogicOperation->arguments[1])
			{
				if(DEBUG_LOGIC) DEBUG("      == All WRITE commands issued for copy - creating response");

				//send back response
				currentTransaction->transactionType = LOGIC_RESPONSE;
				outgoingQueue.push_back(currentTransaction);
				issuedRequests = 0;
			}
			break;
		case LogicOperation::PAGE_TABLE_WALK:
			issuedRequests++;

			if (DEBUG_LOGIC) DEBUG("   ==[L:"<<simpleControllerID<<"] Getting back PT read for 0x"<<std::hex<<(newOperationQueue[0]->address)<<std::dec<<"("<<issuedRequests<<"/3)");

			//check to see if we are done with the requests
			if(issuedRequests == currentLogicOperation->arguments.size())
			{
				if(DEBUG_LOGIC) DEBUG("      ==[L:"<<simpleControllerID<<"] All walked all page table levels, - creating response");

				//send back response
				currentTransaction->transactionType = LOGIC_RESPONSE;
				outgoingQueue.push_back(currentTransaction);
				issuedRequests = 0;
			}
			newOperationQueue.erase(newOperationQueue.begin());

			break;

		default:
			ERROR("== ERROR - Getting data back for a logic op that doesnt create requests : "<<*currentTransaction);
			exit(-1);
			break;
		};
	}
	currentClockCycle++;
}
