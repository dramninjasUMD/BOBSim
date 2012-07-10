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

#include "BOBWrapper.h"
#include "LogicOperation.h"

using namespace std;

namespace BOBSim
{

uint64_t QEMU_MEMORY_SIZE;

BOBWrapper::BOBWrapper(uint64_t qemu_mem_size) :
	readDoneCallback(NULL),
	writeDoneCallback(NULL),
	logicDoneCallback(NULL),
	fullSum(0),
	dramSum(0),
	chanSum(0),
	issuedWrites(0),
	committedWrites(0),
	issuedLogicOperations(0),
	returnedReads(0),
	totalLogicResponses(0),
	totalReturnedReads(0),
	totalIssuedWrites(0),
	totalTransactionsServiced(0),
	portRoundRobin(0),
	maxReadsPerCycle(0),
	maxWritesPerCycle(0),
	readsPerCycle(0),
	writesPerCycle(0)
{
#define TMP_STR_LEN 80

	//Initialize output files
	char *sim_desc;
	char tmp_str[TMP_STR_LEN];
	if ((sim_desc = getenv("SIM_DESC")) != NULL)
	{
		snprintf(tmp_str, TMP_STR_LEN, "BOBstats%s%s.txt", sim_desc, VARIANT_SUFFIX);
		statsOut.open(tmp_str);
		snprintf(tmp_str, TMP_STR_LEN, "BOBpower%s%s.txt", sim_desc, VARIANT_SUFFIX);
		powerOut.open(tmp_str);
	}
	else
	{
		snprintf(tmp_str, TMP_STR_LEN, "BOBstats%s.txt", VARIANT_SUFFIX);
		statsOut.open(tmp_str);
		snprintf(tmp_str, TMP_STR_LEN, "BOBpower%s.txt", VARIANT_SUFFIX);
		powerOut.open(tmp_str);
	}

	QEMU_MEMORY_SIZE = qemu_mem_size;

	currentClockCycle=0;

	//Create BOB object and register callbacks
	bob = new BOB();

	Callback<BOBWrapper,void,unsigned,uint64_t> *writeIssuedCB = new Callback<BOBWrapper,void,unsigned,uint64_t>(this, &BOBWrapper::WriteIssuedCallback);
	bob->RegisterWriteIssuedCallback(writeIssuedCB);

	//Incoming request packet fields (to be added to ports)
	inFlightRequest = vector<Transaction*>(NUM_PORTS,NULL);
	inFlightRequestCounter = vector<unsigned>(NUM_PORTS,0);
	inFlightRequestHeaderCounter = vector<unsigned>(NUM_PORTS,0);

	//Outgoing response packet fields (being sent back to cache)
	inFlightResponse = vector<Transaction*>(NUM_PORTS,NULL);
	inFlightResponseCounter = vector<unsigned>(NUM_PORTS,0);
	inFlightResponseHeaderCounter = vector<unsigned>(NUM_PORTS,0);

	//For statistics & bookkeeping
	requestPortEmptyCount = vector<unsigned>(NUM_PORTS,0);
	responsePortEmptyCount = vector<unsigned>(NUM_PORTS,0);
	requestCounterPerPort = vector<unsigned>(NUM_PORTS,0);
	readsPerPort = vector<unsigned>(NUM_PORTS,0);
	writesPerPort = vector<unsigned>(NUM_PORTS,0);
	returnsPerPort = vector<unsigned>(NUM_PORTS,0);
	perChanFullLatencies = vector< vector<unsigned> >(NUM_CHANNELS, vector<unsigned>());
	perChanReqPort = vector< vector<unsigned> >(NUM_CHANNELS, vector<unsigned>());
	perChanRspPort = vector< vector<unsigned> >(NUM_CHANNELS, vector<unsigned>());
	perChanReqLink = vector< vector<unsigned> >(NUM_CHANNELS, vector<unsigned>());
	perChanRspLink = vector< vector<unsigned> >(NUM_CHANNELS, vector<unsigned>());
	perChanAccess = vector< vector<unsigned> >(NUM_CHANNELS, vector<unsigned>());
	perChanRRQ = vector< vector<unsigned> >(NUM_CHANNELS, vector<unsigned>());
	perChanWorkQTimes = vector< vector<unsigned> >(NUM_CHANNELS, vector<unsigned>());

	//Write configuration stuff to output file
	//
	//System Parameters
	//
	statsOut<<"!!!!"<<qemu_mem_size<<"???"<<QEMU_MEMORY_SIZE<<endl;
	statsOut<<"!!SYSTEM_INI"<<endl;
	statsOut<<"NUM_RANKS="<<NUM_RANKS<<endl;
	statsOut<<"NUM_CHANS="<<NUM_CHANNELS<<endl;
	statsOut<<"BUS_ALIGNMENT_SIZE="<<BUS_ALIGNMENT_SIZE<<endl;
	statsOut<<"JEDEC_DATA_BUS_WIDTH="<<DRAM_BUS_WIDTH<<endl;
	statsOut<<"TRANS_QUEUE_DEPTH="<<PORT_QUEUE_DEPTH<<endl;
	statsOut<<"CMD_QUEUE_DEPTH="<<CHANNEL_WORK_Q_MAX<<endl;
	statsOut<<"USE_LOW_POWER=false"<<endl;
	statsOut<<"EPOCH_COUNT="<<EPOCH_LENGTH<<endl;
	statsOut<<"ROW_BUFFER_POLICY=close_page"<<endl;
	statsOut<<"SCHEDULING_POLICY=N/A"<<endl;
	statsOut<<"ADDRESS_MAPPING_SCHEME=scheme"<<mappingScheme<<endl;
	statsOut<<"QUEUING_STRUCTURE=N/A"<<endl;
	statsOut<<"DEBUG_TRANS_Q=false"<<endl;
	statsOut<<"DEBUG_CMD_Q=false"<<endl;
	statsOut<<"DEBUG_ADDR_MAP=false"<<endl;
	statsOut<<"DEBUG_BANKSTATE=false"<<endl;
	statsOut<<"DEBUG_BUS=false"<<endl;
	statsOut<<"DEBUG_BANKS=false"<<endl;
	statsOut<<"DEBUG_POWER=false"<<endl;
	statsOut<<"VERIFICATION_OUTPUT="<<(VERIFICATION_OUTPUT?"true":"false")<<endl;

	//
	//Device Parameters
	//
	statsOut<<"!!DEVICE_INI"<<endl;
	statsOut<<"NUM_BANKS="<<NUM_BANKS<<endl;
	statsOut<<"NUM_ROWS="<<NUM_ROWS<<endl;
	statsOut<<"NUM_COLS="<<NUM_COLS<<endl;
	statsOut<<"DEVICE_WIDTH="<<DEVICE_WIDTH<<endl;
	statsOut<<"REFRESH_PERIOD=7800"<<endl;
	statsOut<<"tCK="<<tCK<<endl;
	statsOut<<"CL="<<tCL<<endl;
	statsOut<<"AL=0"<<endl;
	statsOut<<"BL="<<BL<<endl;
	statsOut<<"tRAS="<<tRAS<<endl;
	statsOut<<"tRCD="<<tRCD<<endl;
	statsOut<<"tRRD="<<tRRD<<endl;
	statsOut<<"tRC="<<tRC<<endl;
	statsOut<<"tRP="<<tRP<<endl;
	statsOut<<"tCCD="<<tCCD<<endl;
	statsOut<<"tRTP="<<tRTP<<endl;
	statsOut<<"tWTR="<<tWTR<<endl;
	statsOut<<"tWR="<<tWR<<endl;
	statsOut<<"tRTRS=1"<<endl;
	statsOut<<"tRFC="<<tRFC<<endl;
	statsOut<<"tFAW="<<tFAW<<endl;
	statsOut<<"tCKE=0"<<endl;
	statsOut<<"tXP=0"<<endl;
	statsOut<<"tCMD=1"<<endl;
	statsOut<<"IDD0="<<IDD0<<endl;
	statsOut<<"IDD1="<<IDD1<<endl;
	statsOut<<"IDD2P="<<IDD2P1<<endl;
	statsOut<<"IDD2Q="<<IDD2Q<<endl;
	statsOut<<"IDD2N="<<IDD2N<<endl;
	statsOut<<"IDD3Pf="<<IDD3P<<endl;
	statsOut<<"IDD3Ps=0"<<endl;
	statsOut<<"IDD3N="<<IDD3N<<endl;
	statsOut<<"IDD4W="<<IDD4W<<endl;
	statsOut<<"IDD4R="<<IDD4R<<endl;
	statsOut<<"IDD5="<<IDD5B<<endl;
	statsOut<<"IDD6="<<IDD6<<endl;
	statsOut<<"IDD6L="<<IDD6ET<<endl;
	statsOut<<"IDD7="<<IDD7<<endl;

	statsOut<<"!!EPOCH_DATA"<<endl;
}

BOBWrapper *getMemorySystemInstance(uint64_t qemu_mem_size)
{
	return new BOBWrapper(qemu_mem_size);
}
void *getPageWalkLogicOp(uint64_t baseAddr, vector<uint64_t> *args)
{
	if (!args)
	{
		ERROR(" == Got a logic op without an arguments vector");
	}

	LogicOperation *lo = new LogicOperation(LogicOperation::PAGE_TABLE_WALK, *args);
	return (void*)(lo);
}

inline bool BOBWrapper::isPortAvailable(unsigned port)
{
	return inFlightRequestCounter[port] == 0 &&
	       bob->ports[port].inputBuffer.size()<PORT_QUEUE_DEPTH;
}


//Uses the port heuristic to determine which port should be used to receive a request
inline int BOBWrapper::FindOpenPort(uint coreID)
{
	switch(portHeuristic)
	{
	case FIRST_AVAILABLE:
		for (unsigned i=0; i<NUM_PORTS; i++)
		{
			if (isPortAvailable(i))
			{
				return i;
			}
		}
		break;
	case PER_CORE:
		if(isPortAvailable(coreID%NUM_PORTS))
		{
			return coreID%NUM_PORTS;
		}
		break;
	case ROUND_ROBIN:
		int startIndex = portRoundRobin;
		do
		{
			if(isPortAvailable(portRoundRobin))
			{
				int i=portRoundRobin;
				portRoundRobin=(portRoundRobin+1)%NUM_PORTS;
				return i;
			}
			else
			{
				portRoundRobin=(portRoundRobin+1)%NUM_PORTS;
			}
		}
		while(startIndex!=portRoundRobin);

		break;
	};
	return -1;
}

//MARSS uses this function 
//
//Creates Transaction object and determines which port should receive request
//
bool BOBWrapper::AddTransaction(uint64_t addr, bool isWrite, int coreID, void *logicOperation)
{
	int openPort;
	bool isLogicOp = logicOperation != NULL;
	TransactionType type = isLogicOp ? LOGIC_OPERATION : (isWrite ? DATA_WRITE : DATA_READ) ;
	Transaction *trans = new Transaction(type, TRANSACTION_SIZE, addr);
	trans->coreID=coreID;
	if (isLogicOp)
	{
		trans->logicOpContents = logicOperation;
	}

	if ((openPort = FindOpenPort(coreID)) > -1)
	{
		trans->portID = openPort;

		if(currentClockCycle<5000)
		{
			DEBUG("!! "<<*trans<<" to port "<<openPort);
		}
		if (isWrite)
			writesPerCycle++;
		else
			readsPerCycle++;

		AddTransaction(trans, openPort);
	}
	else 
	{
		return false;
	}

	return true;
}

void BOBWrapper::RegisterCallbacks(TransactionCompleteCB *_readDone, TransactionCompleteCB *_writeDone, LogicOperationCompleteCB *_logicDone)
{
	readDoneCallback = _readDone;
	writeDoneCallback = _writeDone;
	logicDoneCallback = _logicDone;
}

bool BOBWrapper::AddTransaction(Transaction* trans, unsigned port)
{
	if(inFlightRequestCounter[port]==0 &&
	        bob->ports[port].inputBuffer.size()<PORT_QUEUE_DEPTH)
	{
		trans->fullStartTime = currentClockCycle;
		requestCounterPerPort[port]++;

		inFlightRequest[port] = trans;
		inFlightRequest[port]->portID = port;
		inFlightRequest[port]->cyclesReqPort = currentClockCycle;

		inFlightRequestHeaderCounter[port] = 1;

		switch(trans->transactionType)
		{
		case DATA_READ:
			readsPerPort[port]++;
			inFlightRequestCounter[port] = 1;
			break;
		case DATA_WRITE:
			issuedWrites++;
			writesPerPort[port]++;
			inFlightRequestCounter[port] = TRANSACTION_SIZE / PORT_WIDTH;
			break;
		case LOGIC_OPERATION:
			issuedLogicOperations++;
			inFlightRequestCounter[port] = TRANSACTION_SIZE / PORT_WIDTH;
			break;
		default:
			ERROR(" = Error - wrong type");
			ERROR(trans);
			break;
		}

		if(DEBUG_PORTS) DEBUG(" = Putting transaction on port "<<port<<" - "<<*inFlightRequest[port]<<" for "<<inFlightRequestCounter[port]<<" CPU cycles");

		return true;
	}
	else
	{
		if(DEBUG_PORTS) DEBUG(" = Port Busy or Full");

		return false; //port busy
	}
}

//
//Updates the state of the memory system
//
void BOBWrapper::Update()
{
	maxReadsPerCycle = max<uint64_t>(maxReadsPerCycle, readsPerCycle);
	maxWritesPerCycle = max<uint64_t>(maxWritesPerCycle, writesPerCycle);
	readsPerCycle = writesPerCycle = 0;

	bob->Update();

	//BOOK-KEEPING
	for(unsigned i=0; i<NUM_PORTS; i++)
	{
		//STATS
		if(inFlightResponse[i]==NULL)
		{
			responsePortEmptyCount[i]++;
		}

		if(inFlightRequest[i]==NULL)
		{
			requestPortEmptyCount[i]++;
		}

		//
		//Requests
		//
		if(inFlightRequestHeaderCounter[i]>0)
		{
			inFlightRequestHeaderCounter[i]--;
			//done
			if(inFlightRequestHeaderCounter[i]==0)
			{
				if(inFlightRequest[i]!=NULL)
				{
					if(DEBUG_PORTS)DEBUG("== Header done - Adding "<<*inFlightRequest[i]<<" to port "<<i);
					bob->ports[i].inputBuffer.push_back(inFlightRequest[i]);
				}
				else
				{
					ERROR("== Error - inFlightResponse null when not supposed to be");
					exit(0);
				}
			}
		}

		if(inFlightRequestCounter[i]>0)
		{
			inFlightRequestCounter[i]--;
			if(inFlightRequestCounter[i]==0)
			{
				inFlightRequest[i]=NULL;
			}
		}

		//
		//Responses
		//
		if(inFlightResponseHeaderCounter[i]>0)
		{
			//does nothing
			inFlightResponseHeaderCounter[i]--;
		}

		if(inFlightResponseCounter[i]>0)
		{
			inFlightResponseCounter[i]--;
			if(inFlightResponseCounter[i]==0)
			{
				if(inFlightResponse[i]->transactionType==RETURN_DATA)
				{
					if (readDoneCallback)
					{
						(*readDoneCallback)(i, inFlightResponse[i]->address);
					}

					UpdateLatencyStats(inFlightResponse[i]);

					returnsPerPort[i]++;
				}
				else if(inFlightResponse[i]->transactionType==LOGIC_RESPONSE)
				{
					if(logicDoneCallback)
					{
						(*logicDoneCallback)(inFlightResponse[i]->mappedChannel, (void*)inFlightResponse[i]->address);
					}

					UpdateLatencyStats(inFlightResponse[i]);

					totalLogicResponses++;
				}
				else
				{
					ERROR("== ERROR - unknown packet type coming down : "<<*inFlightResponse[i]);
					exit(0);
				}

				bob->ports[i].outputBuffer.erase(bob->ports[i].outputBuffer.begin());
				delete inFlightResponse[i];
				inFlightResponse[i]=NULL;
			}
		}
	}

	//NEW STUFF
	for(unsigned i=0; i<NUM_PORTS; i++)
	{
		//look for new stuff to return from bob controller
		if(bob->ports[i].outputBuffer.size()>0 &&
		        inFlightResponseCounter[i]==0)
		{
			inFlightResponse[i] = bob->ports[i].outputBuffer[0];
			inFlightResponseHeaderCounter[i] = 1;

			if(inFlightResponse[i]->transactionType==RETURN_DATA)
			{
				inFlightResponseCounter[i] = TRANSACTION_SIZE / PORT_WIDTH;
			}
			else if(inFlightResponse[i]->transactionType==LOGIC_RESPONSE)
			{
				inFlightResponseCounter[i] = 1;
			}
			else
			{
				ERROR("== Error - wrong type of transaction out of");
				ERROR("== "<<*inFlightResponse[i]);
				exit(0);
			}
		}
	}

	if(currentClockCycle%EPOCH_LENGTH==0 && currentClockCycle>0)
	{
		PrintStats(false);
	}
	currentClockCycle++;
}

void BOBWrapper::UpdateLatencyStats(Transaction *returnedRead)
{
	if(returnedRead->transactionType==LOGIC_RESPONSE)
	{
		DEBUG("  == Got Logic Response : "<<*returnedRead);
		DEBUG("  == Logic All Done!");
		DEBUG("  == Took : "<<CPU_CLK_PERIOD*(currentClockCycle - returnedRead->fullStartTime)<<"ns");
		return;
	}
	else if(returnedRead->transactionType==RETURN_DATA)
	{
		returnedRead->fullTimeTotal = currentClockCycle - returnedRead->fullStartTime;
		returnedRead->cyclesRspPort = currentClockCycle - returnedRead->cyclesRspPort;

		unsigned chanID = returnedRead->mappedChannel;

		//add latencies to lists
		perChanFullLatencies[chanID].push_back(returnedRead->fullTimeTotal);
		perChanReqPort[chanID].push_back(returnedRead->cyclesReqPort);
		perChanRspPort[chanID].push_back(returnedRead->cyclesRspPort);
		perChanReqLink[chanID].push_back(returnedRead->cyclesReqLink);
		perChanRspLink[chanID].push_back(returnedRead->cyclesRspLink);
		perChanAccess[chanID].push_back(returnedRead->dramTimeTotal);
		perChanRRQ[chanID].push_back(returnedRead->cyclesInReadReturnQ);
		perChanWorkQTimes[chanID].push_back(returnedRead->cyclesInWorkQueue);
		/*
			DEBUGN("== Read Returned");
			DEBUGN(" full: "<< returnedRead->fullTimeTotal * CPU_CLK_PERIOD<<"ns");
			DEBUGN(" dram: "<< returnedRead->dramTimeTotal * CPU_CLK_PERIOD<<"ns");
			DEBUG(" chan: "<< returnedRead->channelTimeTotal * CPU_CLK_PERIOD<<"ns");
		*/
		fullLatencies.push_back(returnedRead->fullTimeTotal);
		fullSum+=returnedRead->fullTimeTotal;

		dramLatencies.push_back(returnedRead->dramTimeTotal);
		dramSum+=returnedRead->dramTimeTotal;

		chanLatencies.push_back(returnedRead->channelTimeTotal);
		chanSum+=returnedRead->channelTimeTotal;

		//latencies[((returnedRead->fullTimeTotal/10)*10)*CPU_CLK_PERIOD]++;

		returnedReads++;
	}
}

//
//Prints all statistics on epoch boundaries
//
void BOBWrapper::PrintStats(bool finalPrint)
{
	float fullMean = (float)fullSum / returnedReads;
	float dramMean = (float)dramSum / returnedReads;
	float chanMean = (float)chanSum / returnedReads;

	//calculate standard deviation
	double fullstdsum = 0;
	double dramstdsum = 0;
	double chanstdsum = 0;
	unsigned fullLatMax = 0;
	unsigned fullLatMin = -1;//max unsigned value
	unsigned dramLatMax = 0;
	unsigned dramLatMin = -1;
	unsigned chanLatMax = 0;
	unsigned chanLatMin = -1;
	for(unsigned i=0; i<fullLatencies.size(); i++)
	{
		fullLatMax = max(fullLatencies[i],fullLatMax);
		fullLatMin = min(fullLatencies[i], fullLatMin);
		fullstdsum+=pow(fullLatencies[i]-fullMean,2);

		if(dramLatencies[i]>dramLatMax) dramLatMax = dramLatencies[i];
		if(dramLatencies[i]<dramLatMin) dramLatMin = dramLatencies[i];
		dramstdsum+=pow(dramLatencies[i]-dramMean,2);

		if(chanLatencies[i]>chanLatMax) chanLatMax = chanLatencies[i];
		if(chanLatencies[i]<chanLatMin) chanLatMin = chanLatencies[i];
		chanstdsum+=pow(chanLatencies[i]-chanMean,2);
	}
	double fullstddev = sqrt(fullstdsum/returnedReads);
	double dramstddev = sqrt(dramstdsum/returnedReads);
	double chanstddev = sqrt(chanstdsum/returnedReads);

	//
	//
	//check - 1E9 or 2E30????
	//
	//
	unsigned elapsedCycles;
	if(currentClockCycle%EPOCH_LENGTH==0)
	{
		elapsedCycles = EPOCH_LENGTH;
	}
	else
	{
		elapsedCycles = currentClockCycle%EPOCH_LENGTH;
	}


	double bandwidth = ((issuedWrites+returnedReads)*TRANSACTION_SIZE)/(double)(elapsedCycles*CPU_CLK_PERIOD*1E-9)/(1<<30);

	double rw_ratio = ((double)returnedReads/(double)committedWrites)*100.0f;

	statsOut  <<currentClockCycle*CPU_CLK_PERIOD/1000000<< ","<< setprecision(4) << bandwidth << "," << fullMean*CPU_CLK_PERIOD<<","<<rw_ratio<<",;";

	totalTransactionsServiced += (issuedWrites + returnedReads);
	totalReturnedReads += returnedReads;
	totalIssuedWrites += issuedWrites;

	PRINT("==================Epoch ["<<currentClockCycle/EPOCH_LENGTH<<"]=======================");
	PRINT("per epoch] reads   : "<<returnedReads<<" writes : "<<issuedWrites<<" ("<<(float)returnedReads/(float)(returnedReads+issuedWrites)*100<<"% Reads) : "<<returnedReads+issuedWrites);
	PRINT("lifetime ] reads   : "<<totalReturnedReads<<" writes : "<<totalIssuedWrites<<" total : "<<totalTransactionsServiced);
	PRINT("issued logic ops   : "<<issuedLogicOperations<< " returned logic responses : "<<totalLogicResponses);
	PRINT("bandwidth          : "<<bandwidth<<" GB/sec");
	PRINT("current cycle      : "<<currentClockCycle);
	PRINT("--------------------------");
	PRINT("full time mean  : "<<fullMean*CPU_CLK_PERIOD<<" ns");
	PRINT("          std   : "<<fullstddev*CPU_CLK_PERIOD<<" ns");
	PRINT("          min   : "<<fullLatMin*CPU_CLK_PERIOD<<" ns");
	PRINT("          max   : "<<fullLatMax*CPU_CLK_PERIOD<<" ns");
	PRINT("chan time mean  : "<<chanMean*CPU_CLK_PERIOD<<" ns");
	PRINT("           std  : "<<chanstddev*CPU_CLK_PERIOD);
	PRINT("           min  : "<<chanLatMin*CPU_CLK_PERIOD<<" ns");
	PRINT("           max  : "<<chanLatMax*CPU_CLK_PERIOD<<" ns");
	PRINT("dram time mean  : "<<dramMean*CPU_CLK_PERIOD<<" ns");
	PRINT("          std   : "<<dramstddev*CPU_CLK_PERIOD<<" ns");
	PRINT("          min   : "<<dramLatMin*CPU_CLK_PERIOD<<" ns");
	PRINT("          max   : "<<dramLatMax*CPU_CLK_PERIOD<<" ns");
	PRINT("-- Per Channel Latency Components in nanoseconds (All from READs) : ");
	maxReadsPerCycle = maxWritesPerCycle = 0;
	PRINT("      reqPort    reqLink     workQ    access     rrq    rspLink   rspPort  total");
	for(unsigned i=0; i<NUM_CHANNELS; i++)
	{
		float tempSum=0;
		unsigned tempMax=0;
		unsigned tempMin=-1;
		double stdsum=0;
		double mean=0;
		double reqLinkSum=0;
		double rspLinkSum=0;
		double reqPortSum=0;
		double rspPortSum=0;
		double vSum=0;
		double rrqSum=0;
		double workQSum=0;

		for(unsigned j=0; j<perChanFullLatencies[i].size(); j++)
		{
			if(perChanFullLatencies[i][j]>tempMax) tempMax = perChanFullLatencies[i][j];
			if(perChanFullLatencies[i][j]<tempMin) tempMin = perChanFullLatencies[i][j];

			tempSum+=perChanFullLatencies[i][j];
			reqLinkSum+=perChanReqLink[i][j];
			rspLinkSum+=perChanRspLink[i][j];
			reqPortSum+=perChanReqPort[i][j];
			rspPortSum+=perChanRspPort[i][j];
			vSum+=perChanAccess[i][j];
			rrqSum+=perChanRRQ[i][j];
			workQSum+=perChanWorkQTimes[i][j];
		}

		mean=tempSum/(float)perChanFullLatencies[i].size();

		//write per channel latency to stats file
		statsOut<<CPU_CLK_PERIOD*mean<<",";

		for(unsigned j=0; j<perChanFullLatencies[i].size(); j++)
		{
			stdsum+=pow(perChanFullLatencies[i][j]-mean,2);
		}

#define MAX_STR 512
		char tmp_str[MAX_STR];

		snprintf(tmp_str,MAX_STR,"%d]%10.4f%10.4f%10.4f%10.4f%10.4f%10.4f%10.4f%10.4f\n",
		         i,
		         CPU_CLK_PERIOD*(reqPortSum/perChanReqPort[i].size()),
		         CPU_CLK_PERIOD*(reqLinkSum/perChanReqLink[i].size()),
		         CPU_CLK_PERIOD*(workQSum/perChanWorkQTimes[i].size()),
		         CPU_CLK_PERIOD*(vSum/perChanAccess[i].size()),
		         CPU_CLK_PERIOD*(rrqSum/perChanRRQ[i].size()),
		         CPU_CLK_PERIOD*(rspLinkSum/perChanRspLink[i].size()),
		         CPU_CLK_PERIOD*(rspPortSum/perChanRspPort[i].size()),
		         CPU_CLK_PERIOD*(mean));

		PRINTN(tmp_str);
		//clear
		perChanWorkQTimes[i].clear();
		perChanFullLatencies[i].clear();
		perChanReqLink[i].clear();
		perChanRspLink[i].clear();
		perChanReqPort[i].clear();
		perChanRspPort[i].clear();
		perChanAccess[i].clear();
		perChanRRQ[i].clear();
	}

	PRINT(" ---  Port stats (per epoch) : ");
	for(unsigned i=0; i<NUM_PORTS; i++)
	{
		PRINTN(" "<<i<<"] ");
		PRINTN(" request: "<<(float)requestPortEmptyCount[i]/(elapsedCycles)*100.0<<"\% idle ");
		PRINTN(" response: "<<(float)responsePortEmptyCount[i]/(elapsedCycles)*100.0<<"\% idle ");
		PRINTN(" rds:"<<readsPerPort[i]);
		PRINTN(" wrts:"<<writesPerPort[i]);
		PRINTN(" rtn:"<<returnsPerPort[i]);
		PRINT(" tot:"<<requestCounterPerPort[i]);

		//clear
		returnsPerPort[i]=0;
		writesPerPort[i]=0;
		readsPerPort[i]=0;
		responsePortEmptyCount[i]=0;
		requestPortEmptyCount[i]=0;
		requestCounterPerPort[i]=0;
	}

	issuedWrites = 0;
	returnedReads = 0;
	fullSum = 0;
	dramSum = 0;
	chanSum = 0;

	fullLatencies.clear();
	dramLatencies.clear();
	chanLatencies.clear();
	bob->PrintStats(statsOut, powerOut, finalPrint, elapsedCycles);
}

void BOBWrapper::WriteIssuedCallback(unsigned port, uint64_t address)
{
	committedWrites++;
	if (writeDoneCallback)
	{
		(*writeDoneCallback)(port,address);
	}
}


} //namespace BOBSim


//for autoconf
extern "C"
{
	void libbobsim_is_present(void)
	{
		;
	}
}
