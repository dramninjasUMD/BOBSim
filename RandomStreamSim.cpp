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

//TraceBasedSim.cpp
//
//File to run a trace-based simulation
//

#include <iostream>
#include <fstream>
#include <sstream>
#include <getopt.h>
#include <iomanip>
#include <map>
#include <bitset>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <deque>
#include "Globals.h"
#include "Transaction.h"
#include "BOB.h"
#include "BOBWrapper.h"
#include "LogicLayerInterface.h"
#include "LogicOperation.h"


using namespace BOBSim;
using namespace std;

unsigned BOBSim::NUM_PORTS =4;

uint64_t currentClockCycle = 0;
int BOBSim::SHOW_SIM_OUTPUT=1;
int c;
int debugBus=0;
string traceFileName = "traces/trace.trc";
string pwdString = "";

string tmp = "";
size_t equalsign;

long numCycles=30l;

ofstream BOBSim::verificationOutput;

vector<unsigned> randomSeeds;

vector<unsigned> waitCounters;
vector<unsigned> useCounters;

void usage()
{
	cout << "Blah" << endl;
}

vector< vector<Transaction *> > transactionBuffer;
vector<unsigned> transactionClockCycles;

void FillTransactionBuffer(int port)
{
	useCounters[port] = 0;

	//Fill with a random number of requests between 2 and 5 or 6
	unsigned randomCount = rand_r(&randomSeeds[port]) % 5 + 2;
	//cout<<"port "<<port<<" - random count is "<<randomCount<<endl;
	for(unsigned i=0; i<randomCount; i++)
	{
		unsigned long temp = rand();
		unsigned long physicalAddress = (temp<<32)|rand_r(&randomSeeds[port]);
		Transaction *newTrans;
		
		if(physicalAddress%1000<READ_WRITE_RATIO*10)
		{
			newTrans = new Transaction(DATA_READ,TRANSACTION_SIZE,physicalAddress);
			//cout<<*newTrans<<endl;
			useCounters[port]+= RD_REQUEST_PACKET_OVERHEAD/PORT_WIDTH+!!(RD_REQUEST_PACKET_OVERHEAD%PORT_WIDTH);
		}
		else
		{
			newTrans = new Transaction(DATA_WRITE,TRANSACTION_SIZE,physicalAddress);
			//cout<<*newTrans<<endl;
			useCounters[port]+=(WR_REQUEST_PACKET_OVERHEAD + TRANSACTION_SIZE)/PORT_WIDTH +
				!!((WR_REQUEST_PACKET_OVERHEAD + TRANSACTION_SIZE)%PORT_WIDTH);
		}
		
		transactionBuffer[port].push_back(newTrans);
	}
	
	//cout<<"use for "<<port<<" is "<<useCounters[port]<<endl;
	//figure out how much idle time between requests
	waitCounters[port] = ceil(useCounters[port]/PORT_UTILIZATION) - useCounters[port];
	//cout<<"wait for "<<port<<" is "<<waitCounters[port]<<endl;
}

int main(int argc, char **argv)
{
	while (1)
	{
		static struct option long_options[] =
		{
			{"pwd", required_argument, 0, 'p'},
			{"numcycles",  required_argument,	0, 'c'},
			{"quiet",  no_argument, &BOBSim::SHOW_SIM_OUTPUT, 'q'},
			{"help", no_argument, 0, 'h'},
			{0, 0, 0, 0}
		};
		int option_index=0; //for getopt
		c = getopt_long (argc, argv, "c:n:p:bkq", long_options, &option_index);
		if (c == -1)
		{
			break;
		}
		switch (c)
		{
		case 0: //TODO: figure out what the hell this does, cuz it never seems to get called
			if (long_options[option_index].flag != 0) //do nothing on a flag
			{
				printf("setting flag\n");
				break;
			}
			printf("option %s",long_options[option_index].name);
			if (optarg)
			{
				printf(" with arg %s", optarg);
			}
			printf("\n");
			break;
		case 'h':
		case '?':
			usage();
			exit(0);
			break;
		case 'c':
			numCycles = atol(optarg);
			break;
		case 'n':
			BOBSim::NUM_PORTS = atoi(optarg);
		case 'p':
			pwdString = string(optarg);
			break;
		case 'q':
			BOBSim::SHOW_SIM_OUTPUT=0;
			break;
		}
	}

	//open up verification file
	if(VERIFICATION_OUTPUT)
	{
		verificationOutput.open("sim_out_DDR3_micron_64M_8B_x4_sg187E.ini.tmp");
	}

	BOBWrapper bobWrapper(0);
	transactionBuffer = vector< vector<Transaction *> >(NUM_PORTS,vector<Transaction *>());

	waitCounters = vector<unsigned>(NUM_PORTS,0);
	useCounters = vector<unsigned>(NUM_PORTS,0);

	randomSeeds = vector<unsigned>(NUM_PORTS,0);
	for(unsigned i=0; i<NUM_PORTS; i++)
	{
		randomSeeds[i]=i*SEED_CONSTANT + SEED_CONSTANT;
	}

	//iterate over total number of cycles
	//  "main loop"
	//   numCycles is the number of CPU cycles to simulate
	for (int cpuCycle=0; cpuCycle<numCycles; cpuCycle++)
	{
		DEBUG("\n=========================["<<currentClockCycle<<"]==========================");

		//adding new stuff
		for(unsigned l=0; l<NUM_PORTS; l++)
		{
			if(transactionBuffer[l].size()>0)
			{
				if(DEBUG_PORTS) DEBUG("== TraceBasedSim trying to send : port "<<l<<" : "<<*transactionBuffer[l][0]);

				if(bobWrapper.AddTransaction(transactionBuffer[l][0],l))
				{
					transactionBuffer[l].erase(transactionBuffer[l].begin());
				}
			}
			else
			{
				if(waitCounters[l]>0) waitCounters[l]--;

				//make sure we are not waiting during idle time
				if(waitCounters[l]==0)
				{
					FillTransactionBuffer(l);
				}
			}
		}


		//
		//Update bobWrapper
		//
		bobWrapper.Update();
		currentClockCycle++;
	}

	//
	//Debug output
	//
	bobWrapper.PrintStats(true);
}
