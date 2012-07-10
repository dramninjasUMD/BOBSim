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

//DRAM Channel source

#include "DRAMChannel.h"
#include "LogicLayerInterface.h"

using namespace std;
using namespace BOBSim;

DRAMChannel::DRAMChannel():
	inFlightCommandPacket(NULL),
	inFlightDataPacket(NULL),
	inFlightDataCountdown(0),
	inFlightCommandCountdown(0),
	channelID(0),
	readReturnQueueMax(0),
	simpleController(this),
	DRAMBusIdleCount(0)
{
	exit(0);
}

DRAMChannel::DRAMChannel(unsigned id, Callback<BOB, void, BusPacket*, unsigned> *reportCB):
	inFlightCommandCountdown(0),
	inFlightDataCountdown(0),
	channelID(id),
	inFlightCommandPacket(NULL),
	inFlightDataPacket(NULL),
	readReturnQueueMax(0),
	simpleController(this),
	logicLayer(NULL),
	DRAMBusIdleCount(0)
{
	ReportCallback = reportCB;

	currentClockCycle = 0;

	Callback<DRAMChannel,void,BusPacket*,unsigned> *dataCallback = new Callback<DRAMChannel,void, BusPacket*,unsigned>(this, &DRAMChannel::ReceiveOnDataBus);
	Callback<DRAMChannel,void,BusPacket*,unsigned> *cmdCallback = new Callback<DRAMChannel,void, BusPacket*,unsigned>(this, &DRAMChannel::ReceiveOnCmdBus);
	simpleController.RegisterCallback(cmdCallback, dataCallback);

	for(int i=0; i<NUM_RANKS; i++)
	{
		ranks.push_back(Rank(i));
		ranks[i].RegisterCallback(dataCallback);
	}

	logicLayer = new LogicLayerInterface(id);

	Callback<LogicLayerInterface, void, Transaction*, unsigned> *logicCallback = new Callback<LogicLayerInterface, void, Transaction*, unsigned>(logicLayer, &LogicLayerInterface::ReceiveLogicOperation);
	SendToLogicLayer = logicCallback;

	Callback<DRAMChannel, bool, Transaction*, unsigned> *returnCallback = new Callback<DRAMChannel, bool, Transaction*, unsigned>(this, &DRAMChannel::AddTransaction);
	logicLayer->RegisterReturnCallback(returnCallback);

}

void DRAMChannel::Update()
{
	if(DEBUG_CHANNEL)
	{
		DEBUG("   == DRAM Channel "<<channelID<<" [DRAM cycle "<<currentClockCycle<<"]");

		DEBUG("     == Read Return Queue");
		for(unsigned i=0; i<readReturnQueue.size(); i++)
		{
			DEBUG("     "<<i<<"] "<<*readReturnQueue[i]);
		}
	}

	if(inFlightDataPacket==NULL)
	{
		DRAMBusIdleCount++;
	}

	//update buses
	if(inFlightCommandPacket!=NULL)
	{
		inFlightCommandCountdown--;
		if(inFlightCommandCountdown==0)
		{
			ranks[inFlightCommandPacket->rank].ReceiveFromBus(inFlightCommandPacket);
			inFlightCommandPacket = NULL;
		}
	}

	if(inFlightDataPacket!=NULL)
	{
		inFlightDataCountdown--;
		if(inFlightDataCountdown==0)
		{
			switch(inFlightDataPacket->busPacketType)
			{
			case READ_DATA:
				if(DEBUG_CHANNEL) DEBUG("     == Data burst complete : " << *inFlightDataPacket);

				//if the bus packet was from a request originating from a logic operation, send it back to logic layer
				if(inFlightDataPacket->fromLogicOp)
				{
					(*SendToLogicLayer)(new Transaction(RETURN_DATA, 64, inFlightDataPacket->address),0);
				}
				//if it was a regular request, add to return queue
				else
				{
					readReturnQueue.push_back(inFlightDataPacket);

					simpleController.outstandingReads--;

					(*ReportCallback)(inFlightDataPacket, 0);

					//keep track of total number of entries in return queue
					if(readReturnQueue.size()>readReturnQueueMax)
					{
						readReturnQueueMax = readReturnQueue.size();
					}
				}
				break;
			case WRITE_DATA:
				//(*ReportCallback)(inFlightDataPacket, 0);
				ranks[inFlightDataPacket->rank].ReceiveFromBus(inFlightDataPacket);
				break;
			default:
				ERROR("Encountered unexpected bus packet type" << *inFlightDataPacket);
				abort();
			}

			inFlightDataPacket = NULL;
		}
	}

	//updates
	if(logicLayer!=NULL)
	{
		logicLayer->Update();
	}

	simpleController.Update();
	for(unsigned i=0; i<NUM_RANKS; i++)
	{
		ranks[i].Update();
	}

	currentClockCycle++;
}

bool DRAMChannel::AddTransaction(Transaction *trans, unsigned notused)
{
	if(DEBUG_CHANNEL)DEBUG("    In AddTransaction - got : "<<*trans);

	if(trans->transactionType==LOGIC_OPERATION)
	{
		if(SendToLogicLayer!=NULL)
		{
			(*SendToLogicLayer)(trans,0);
		}
		else
		{
			ERROR("== Logic Layers are not registered, yet receiving a logic operation.  Please register");
			exit(0);
		}
	}
	else if(trans->transactionType==LOGIC_RESPONSE)
	{
		if(pendingLogicResponse==NULL)
		{
			if (DEBUG_LOGIC) DEBUG("== Made it back to channel on cycle "<<currentClockCycle<<" : "<<*trans);
			pendingLogicResponse = trans;
		}
		else return false;
	}
	else
	{
		if(simpleController.waitingACTS<CHANNEL_WORK_Q_MAX)
		{
			simpleController.AddTransaction(trans);
		}
		else return false;
	}

	return true;
}

void DRAMChannel::ReceiveOnCmdBus(BusPacket *busPacket, unsigned id)
{
	if(inFlightCommandPacket!=NULL)
	{
		ERROR("== Error - Bus collision while trying to receive from controller");
		ERROR("   Existing : "<<*inFlightCommandPacket);
		ERROR("   Incoming : "<<*busPacket);
		exit(0);
	}

	if(DEBUG_CHANNEL) DEBUG("     == Putting command on bus : " << *busPacket);

	//Report the time we waited in the queue
	if(busPacket->busPacketType==ACTIVATE)
	{
		(*ReportCallback)(busPacket, 0);
	}
	//Report the WRITE is finally going
	else if(busPacket->busPacketType==WRITE_P)
	{
		(*ReportCallback)(busPacket, 0);
	}

	inFlightCommandPacket = busPacket;
	inFlightCommandCountdown = tCMDS;
}

void DRAMChannel::ReceiveOnDataBus(BusPacket *busPacket, unsigned id)
{
	if(!(busPacket->busPacketType==READ_DATA ||
	        busPacket->busPacketType==WRITE_DATA))
	{
		ERROR("== Error - Trying to put non-data packet on data bus:" << *busPacket);
		exit(0);
	}

	if(inFlightDataPacket!=NULL)
	{
		ERROR("== Error - Bus collision while trying to receive from a rank in channel "<<channelID);
		ERROR("           Incoming Packet : "<<*busPacket);
		ERROR("           Existing Packet : "<<*inFlightDataPacket);
		ERROR("               (Time Left) : "<<inFlightDataCountdown);
		ERROR("           Clock Cycle : "<<currentClockCycle);
		exit(0);
	}
	if(DEBUG_CHANNEL) DEBUG("     == Putting data on bus [rank "<<id<<"] : " << *busPacket);

	inFlightDataPacket = busPacket;
	inFlightDataCountdown = busPacket->burstLength;
}

void DRAMChannel::RegisterCallback(Callback<BOB, void, BusPacket*, unsigned> *rptCallback)
{
	ReportCallback = rptCallback;
}
