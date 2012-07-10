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

//BOB source
#include <bitset>
#include <math.h>
#include "BOB.h"

using namespace std;

namespace BOBSim
{
#ifdef LOG_OUTPUT
ofstream logOutput;
#endif

unsigned LINK_CPU_CLK_RATIO;
unsigned DRAM_CPU_CLK_RATIO;
unsigned DRAM_CPU_CLK_ADJUSTMENT;
//uint64_t QEMU_MEMORY_SIZE;
BOB::BOB() : priorityPort(0),
	readCounter(0),
	writeCounter(0),
	committedWrites(0),
	clockCycleAdjustmentCounter(0),
	writeIssuedCB(NULL),
	rankBitWidth(log2(NUM_RANKS)),
	bankBitWidth(log2(NUM_BANKS)),
	rowBitWidth(log2(NUM_ROWS)),
	colBitWidth(log2(NUM_COLS)),
	busOffsetBitWidth(log2(BUS_ALIGNMENT_SIZE)),
	channelBitWidth(log2(NUM_CHANNELS)),
	cacheOffset(log2(CACHE_LINE_SIZE))
{
	/*
	  HACK!!! - QEMU does not support the huge memory sizes BOB can simulate.
	  Removing bits from the row address will have the least impact on function
	*/
	uint qemuMemoryBitWidth = log2_64(QEMU_MEMORY_SIZE);
	uint bitDifference = (busOffsetBitWidth + colBitWidth + rowBitWidth + channelBitWidth + rankBitWidth + bankBitWidth) - qemuMemoryBitWidth;
	rowBitWidth = rowBitWidth - bitDifference;
	//end hack


#ifdef LOG_OUTPUT
	string output_filename("BOBsim");
	if (getenv("SIM_DESC"))
		output_filename += getenv("SIM_DESC");
	output_filename += VARIANT_SUFFIX;
	output_filename += ".log";
	logOutput.open(output_filename.c_str());
#endif
	DEBUG("!!!!!!! QEMU_MEMORY_SIZE :"<<QEMU_MEMORY_SIZE<<" qemuMemoryBitWidth : "<<qemuMemoryBitWidth<<"   newRowBitWidth : "<<rowBitWidth<<"\n");
	DEBUG("busoff:"<<busOffsetBitWidth<<" col:"<<colBitWidth<<" row:"<<rowBitWidth<<" rank:"<<rankBitWidth<<" bank:"<<bankBitWidth<<" chan:"<<channelBitWidth);

	currentClockCycle = 0;

	//Compute ratios of various clock frequencies 
	if(fmod(CPU_CLK_PERIOD,LINK_BUS_CLK_PERIOD)>0.00001)
	{		
		ERROR("== ERROR - Link bus clock period ("<<LINK_BUS_CLK_PERIOD<<") must be a factor of CPU clock period ("<<CPU_CLK_PERIOD<<")");
		ERROR("==         fmod="<<fmod(CPU_CLK_PERIOD,LINK_BUS_CLK_PERIOD));
		abort(); 
	}

	LINK_CPU_CLK_RATIO = (CPU_CLK_PERIOD / LINK_BUS_CLK_PERIOD);
	cout<<"Channel to CPU ratio : "<<LINK_CPU_CLK_RATIO<<endl;
	DRAM_CPU_CLK_RATIO = tCK / CPU_CLK_PERIOD;
	cout<<"DRAM to CPU ratio : "<<DRAM_CPU_CLK_RATIO<<endl;
	DRAM_CPU_CLK_ADJUSTMENT = tCK / fmod(tCK,CPU_CLK_PERIOD);
	cout<<"DRAM_CPU_CLK_ADJUSTMENT : "<<DRAM_CPU_CLK_ADJUSTMENT<<endl;

	//Ensure that parameters have been set correclty
	if(NUM_CHANNELS / CHANNELS_PER_LINK_BUS != NUM_LINK_BUSES)
	{
		ERROR("== ERROR - Mismatch in NUM_CHANNELS and NUM_LINK_BUSES");
		exit(0);
	}

	if(DEBUG_BOB) DEBUG("== Channel-CPU clk ratio : "<<LINK_CPU_CLK_RATIO<<"  DRAM-CPU clk ratio : "<<DRAM_CPU_CLK_RATIO);

	//Make port objects
	for(unsigned i=0; i<NUM_PORTS; i++)
	{
		ports.push_back(Port(i));
	}

	//Initialize fields for bookkeeping
	inFlightRequestLinkCountdowns = vector<unsigned>(NUM_LINK_BUSES,0);
	inFlightResponseLinkCountdowns = vector<unsigned>(NUM_LINK_BUSES,0);

	inFlightRequestLink = vector<Transaction *> (NUM_LINK_BUSES, (Transaction*)(NULL));
	inFlightResponseLink = vector<Transaction *> (NUM_LINK_BUSES, (Transaction*)NULL);

	serDesBufferRequest = vector<Transaction *> (NUM_LINK_BUSES, (Transaction*)NULL);
	serDesBufferResponse = vector<Transaction *> (NUM_LINK_BUSES, (Transaction*)NULL);

	responseLinkRoundRobin = vector<unsigned> (NUM_LINK_BUSES,0);

	channelCounters = vector<unsigned>(NUM_CHANNELS,0);
	channelCountersLifetime = vector<uint64_t>(NUM_CHANNELS,0);

	portInputBufferAvg = vector<uint> (NUM_PORTS, 0);
	portOutputBufferAvg = vector<uint> (NUM_PORTS, 0);

	requestLinkIdle = vector<unsigned> (NUM_LINK_BUSES,0);
	responseLinkIdle = vector<unsigned> (NUM_LINK_BUSES,0);

	cmdQFull = vector<uint>(NUM_CHANNELS,0);

	//Define callback function
	Callback<BOB, void, BusPacket*, unsigned> *reportCallback = new Callback<BOB, void, BusPacket*, unsigned>(this, &BOB::ReportCallback);

	//Create channels
	channels.reserve(NUM_CHANNELS);
	for(unsigned i=0; i<NUM_CHANNELS; i++)
	{
		channels.push_back(new DRAMChannel(i,reportCallback));
	}

	//Used for round-robin
	priorityLinkBus = vector<unsigned>(NUM_PORTS,0);
}

void BOB::Update()
{
	//
	//STATS KEEPING
	//

	//keep track of how long data has been waiting in the channel for the switch to become available
	for(unsigned i=0; i<pendingReads.size(); i++)
	{
		//look in the channel that the request was mapped to and search its return queue
		for(unsigned j=0; j<channels[pendingReads[i]->mappedChannel]->readReturnQueue.size(); j++)
		{
			//if the transaction IDs match, then the data is waiting there to return (it might not be ready yet,
			//  which would not trigger this condition)
			if(pendingReads[i]->transactionID==channels[pendingReads[i]->mappedChannel]->readReturnQueue[j]->transactionID)
			{
				pendingReads[i]->cyclesInReadReturnQ++;
			}
		}
	}

	//calculate number of transactions waiting
	for(unsigned i=0; i<NUM_CHANNELS; i++)
	{
		for(unsigned j=0; j<channels[i]->simpleController.commandQueue.size(); j++)
		{
			if(channels[i]->simpleController.commandQueue[j]->busPacketType==ACTIVATE)
			{
				channels[i]->simpleController.commandQueue[j]->queueWaitTime++;
			}
		}
	}

	//keep track of idle link buses
	for(unsigned i=0; i<NUM_LINK_BUSES; i++)
	{
		if(inFlightRequestLink[i]==NULL)
		{
			requestLinkIdle[i]++;
		}

		if(inFlightResponseLink[i]==NULL)
		{
			responseLinkIdle[i]++;
		}
	}

	//keep track of average entries in port in/out-buffers
	for(unsigned i=0; i<NUM_PORTS; i++)
	{
		portInputBufferAvg[i] += ports[i].inputBuffer.size();
		portOutputBufferAvg[i] += ports[i].outputBuffer.size();
	}


	//
	// DEBUG OUTPUT
	//

	if(DEBUG_BOB)
	{
		DEBUG(endl<<" --------- BOB Update Started ["<<currentClockCycle<<"] ---------");

		for(unsigned i=0; i<NUM_PORTS; i++)
		{
			DEBUG("== Port "<<i);
			DEBUG(" - Input Buffer ("<<ports[i].inputBuffer.size()<<") == Busy Countdown:"<<ports[i].inputBusyCountdown);
			for(unsigned j=0; j<ports[i].inputBuffer.size(); j++)
			{
				DEBUG("   "<<j<<"] "<<*ports[i].inputBuffer[j]);
			}
			DEBUG(" - Output Buffer ("<<ports[i].outputBuffer.size()<<") == Busy Countdown:"<<ports[i].outputBusyCountdown);
			for(unsigned j=0; j<ports[i].outputBuffer.size(); j++)
			{
				DEBUG("   "<<j<<"] "<<*ports[i].outputBuffer[j]);
			}
		}

		DEBUG("== SerDe Buffers");
		for(unsigned i=0; i<NUM_LINK_BUSES; i++)
		{
			DEBUGN("   "<<i<<"] request: ");
			if(serDesBufferRequest[i]==NULL)
			{
				DEBUGN("NULL");
			}
			else
			{
				DEBUGN(*serDesBufferRequest[i]);
			}

			DEBUGN("   response: ");
			if(serDesBufferResponse[i]==NULL)
			{
				DEBUG("NULL");
			}
			else
			{
				DEBUG(*serDesBufferResponse[i]);
			}
		}

		DEBUG("== Request Link Transactions");
		for(unsigned i=0; i<NUM_LINK_BUSES; i++)
		{
			if(inFlightRequestLink[i]!=NULL)
			{
				DEBUG("   "<<i<<"] "<<*inFlightRequestLink[i]<<" for "<<inFlightRequestLinkCountdowns[i]<<" cc");
			}
			else
			{
				DEBUG("   "<<i<<"] NULL");
			}
		}

		DEBUG("== Response Link Transactions");
		for(unsigned i=0; i<NUM_LINK_BUSES; i++)
		{
			if(inFlightResponseLink[i]!=NULL)
			{
				DEBUG("   "<<i<<"] "<<*inFlightResponseLink[i]<<" for "<<inFlightResponseLinkCountdowns[i]<<" cc");
			}
			else
			{
				DEBUG("   "<<i<<"] NULL");
			}
		}


		DEBUG("== Pending Read Queue ("<<pendingReads.size()<<")");
		for(unsigned i=0; i<pendingReads.size(); i++)
		{
			DEBUG(" "<<i<<"] "<<*pendingReads[i]);
		}

	}

	//
	// UPDATE BUSES
	//
	if(DEBUG_BOB)DEBUG("== Bus Movement");
	//update each port's bookkeeping
	for(unsigned i=0; i<NUM_PORTS; i++)
	{
		if(ports[i].inputBusyCountdown>0)
		{
			ports[i].inputBusyCountdown--;

			//if the port input is done sending, erase that item
			if(ports[i].inputBusyCountdown==0)
			{

			}
		}

		if(ports[i].outputBusyCountdown>0)
		{
			ports[i].outputBusyCountdown--;
		}
	}

	for(unsigned i=0; i<NUM_LINK_BUSES; i++)
	{
		if(inFlightRequestLinkCountdowns[i]>0)
		{
			inFlightRequestLinkCountdowns[i]--;

			if(inFlightRequestLinkCountdowns[i]==0)
			{
				//compute total time in serDes and travel up channel
				inFlightRequestLink[i]->cyclesReqLink = currentClockCycle - inFlightRequestLink[i]->cyclesReqLink;
				if(DEBUG_BOB) DEBUG("  == Adding to channel "<<inFlightRequestLink[i]->mappedChannel<<" (from link bus "<<i<<") : "<<*inFlightRequestLink[i]);

				//add to channel
				channels[inFlightRequestLink[i]->mappedChannel]->AddTransaction(inFlightRequestLink[i], 0); //0 is not used

				//remove from channel bus
				inFlightRequestLink[i] = NULL;

				//remove from SerDe buffer
				serDesBufferRequest[i] = NULL;
			}
		}

		if(inFlightResponseLinkCountdowns[i]>0)
		{
			inFlightResponseLinkCountdowns[i]--;

			if(inFlightResponseLinkCountdowns[i]==0)
			{
				if(serDesBufferResponse[i]!=NULL)
				{
					ERROR("== Error - Response SerDe Buffer "<<i<<" collision");
					exit(0);
				}

				//note the time
				inFlightResponseLink[i]->channelTimeTotal = currentClockCycle - inFlightResponseLink[i]->channelStartTime;

				//remove from return queue
				delete channels[inFlightResponseLink[i]->mappedChannel]->readReturnQueue[0];
				channels[inFlightResponseLink[i]->mappedChannel]->readReturnQueue.erase(channels[inFlightResponseLink[i]->mappedChannel]->readReturnQueue.begin());


				serDesBufferResponse[i] = inFlightResponseLink[i];
				inFlightResponseLink[i] = NULL;
			}
		}
	}


	//
	// NEW STUFF
	//
	if(DEBUG_BOB) DEBUG("== Moving from SerDe to channel bus");
	for(unsigned c=0; c<NUM_LINK_BUSES; c++)
	{
		//
		//REQUEST
		//
		if(serDesBufferRequest[c]!=NULL &&
		        inFlightRequestLinkCountdowns[c]==0)
		{
			//error checking
			if(inFlightRequestLink[c]!=NULL)
			{
				ERROR("== Error - item in SerDe buffer without channel being free");
				ERROR("   == Channel : "<<c);
				ERROR("   == Current : "<<*inFlightRequestLink[c]);
				ERROR("   == SerDe   : "<<*serDesBufferRequest[c]);
				exit(0);
			}

			//put on channel bus
			inFlightRequestLink[c] = serDesBufferRequest[c];
			//note the time
			inFlightRequestLink[c]->channelStartTime = currentClockCycle;

			//the total number of channel cycles the packet will be on the bus
			unsigned totalChannelCycles;

			if(inFlightRequestLink[c]->transactionType==DATA_READ)
			{
				//
				//widths are in bits
				//
				totalChannelCycles = (RD_REQUEST_PACKET_OVERHEAD * 8) / REQUEST_LINK_BUS_WIDTH +
				                     !!((RD_REQUEST_PACKET_OVERHEAD * 8) % REQUEST_LINK_BUS_WIDTH);
			}
			else if(inFlightRequestLink[c]->transactionType==DATA_WRITE)
			{
				//
				//widths are in bits
				//
				totalChannelCycles = ((WR_REQUEST_PACKET_OVERHEAD + TRANSACTION_SIZE) * 8) / REQUEST_LINK_BUS_WIDTH +
				                     !!(((WR_REQUEST_PACKET_OVERHEAD + TRANSACTION_SIZE) * 8) % REQUEST_LINK_BUS_WIDTH);
			}
			else if(inFlightRequestLink[c]->transactionType==LOGIC_OPERATION)
			{
				//
				//widths are in bits
				//
				totalChannelCycles = (inFlightRequestLink[c]->transactionSize * 8) / REQUEST_LINK_BUS_WIDTH +
				                     !!((inFlightRequestLink[c]->transactionSize * 8) % REQUEST_LINK_BUS_WIDTH);
			}
			else
			{
				ERROR("== Error - wrong type "<<*inFlightRequestLink[c]);
				exit(0);
			}

			//if the channel uses DDR signaling, the cycles is cut in half
			if(LINK_BUS_USE_DDR)
			{
				totalChannelCycles = totalChannelCycles / 2 + !!(totalChannelCycles % 2);
			}

			//since the channel is faster than the CPU, figure out how many CPU
			//  cycles the channel will be in use
			inFlightRequestLinkCountdowns[c] = (totalChannelCycles / LINK_CPU_CLK_RATIO) +
			                                   !!(totalChannelCycles%LINK_CPU_CLK_RATIO);

			//error check
			if(inFlightRequestLinkCountdowns[c]==0)
			{
				ERROR("== Error - countdown is 0");
				exit(0);
			}

			if(DEBUG_BOB)
			{
				DEBUG("  == Channel Bus "<<c<<" getting : "<<*inFlightRequestLink[c]);
				DEBUG("     == CPU Clks:"<<inFlightRequestLinkCountdowns[c]<<"   Channel Clks:"<< totalChannelCycles<<" DDR?:"<<LINK_BUS_USE_DDR);
			}
		}
	}


	//
	//Responses
	//
	for(unsigned p=0; p<NUM_PORTS; p++)
	{
		if(ports[p].outputBusyCountdown==0)
		{
			//check to see if something has been received from a channel
			if(serDesBufferResponse[priorityLinkBus[p]]!=NULL &&
			        serDesBufferResponse[priorityLinkBus[p]]->portID==p)
			{
				serDesBufferResponse[priorityLinkBus[p]]->cyclesRspLink = currentClockCycle - serDesBufferResponse[priorityLinkBus[p]]->cyclesRspLink;
				serDesBufferResponse[priorityLinkBus[p]]->cyclesRspPort = currentClockCycle;
				ports[p].outputBuffer.push_back(serDesBufferResponse[priorityLinkBus[p]]);
				switch(serDesBufferResponse[priorityLinkBus[p]]->transactionType)
				{
				case RETURN_DATA:
					ports[p].outputBusyCountdown = TRANSACTION_SIZE / PORT_WIDTH;
					break;
				case LOGIC_RESPONSE:
					ports[p].outputBusyCountdown = 1;
					break;
				default:
					ERROR("== ERROR - Trying to add wrong type of transaction to output port : "<<*serDesBufferResponse[priorityLinkBus[p]]);
					exit(0);
					break;
				};
				serDesBufferResponse[priorityLinkBus[p]] = NULL;
			}
			priorityLinkBus[p]++;
			if(priorityLinkBus[p]==NUM_LINK_BUSES)priorityLinkBus[p]=0;
		}
	}

	//
	//Move from input ports to serdes
	//
	if(DEBUG_BOB) DEBUG("== Moving from port buffer to SerDe");
	for(unsigned port=0; port<NUM_PORTS; port++)
	{
		unsigned p = priorityPort;

		if(DEBUG_BOB) DEBUG("  == Port "<<p);
		//make sure the port isn't already sending something
		//  and that there is an item there to send
		if(ports[p].inputBusyCountdown==0 &&
		        ports[p].inputBuffer.size()>0)
		{
			//search out-of-order
			for(unsigned i=0; i<ports[p].inputBuffer.size(); i++)
			{
				unsigned channelID = FindChannelID(ports[p].inputBuffer[i]);
				unsigned linkBusID = channelID / CHANNELS_PER_LINK_BUS;

				//make sure the serDe isn't busy and the queue isn't full
				if(serDesBufferRequest[linkBusID]==NULL &&
				        channels[channelID]->simpleController.waitingACTS<CHANNEL_WORK_Q_MAX)
				{
					//put on channel bus
					serDesBufferRequest[linkBusID] = ports[p].inputBuffer[i];
					serDesBufferRequest[linkBusID]->cyclesReqLink = currentClockCycle;
					serDesBufferRequest[linkBusID]->cyclesReqPort = currentClockCycle - serDesBufferRequest[linkBusID]->cyclesReqPort;
					serDesBufferRequest[linkBusID]->mappedChannel = channelID;

					//keep track of requests
					channelCounters[channelID]++;

					if(serDesBufferRequest[linkBusID]->transactionType==DATA_READ)
					{
						//put in pending queue
						//  make it a RETURN_DATA type before we put it in pending queue
						pendingReads.push_back(ports[p].inputBuffer[i]);

						readCounter++;

						//set port busy time
						ports[p].inputBusyCountdown = 1;
					}
					else if(serDesBufferRequest[linkBusID]->transactionType==DATA_WRITE)
					{
						writeCounter++;

						//set port busy time
						ports[p].inputBusyCountdown = serDesBufferRequest[linkBusID]->transactionSize / PORT_WIDTH;
					}
					else if(serDesBufferRequest[linkBusID]->transactionType==LOGIC_OPERATION)
					{
						logicOpCounter++;

						//set port busy time
						ports[p].inputBusyCountdown = serDesBufferRequest[linkBusID]->transactionSize / PORT_WIDTH;
					}
					else
					{
						ERROR("== Error - unknown transaction type going to channel : "<<*serDesBufferRequest[linkBusID]);
						exit(0);
					}

					if(DEBUG_BOB) DEBUG("    == Request SerDes "<<linkBusID<<" getting "<<*serDesBufferRequest[linkBusID]);

					priorityPort++;
					if(priorityPort==NUM_PORTS) priorityPort = 0;

					//remove from port input buffer
					ports[p].inputBuffer.erase(ports[p].inputBuffer.begin()+i);
					break;
				}
				else
				{
					if(DEBUG_BOB)
					{
						if(serDesBufferRequest[linkBusID]!=NULL)
						{
							DEBUG("    == Request SerDes Full : "<<*serDesBufferRequest[linkBusID]);
							DEBUG("             Left : "<<inFlightRequestLinkCountdowns[linkBusID]);
						}

						if(channels[channelID]->simpleController.waitingACTS>=CHANNEL_WORK_Q_MAX)
						{
							cmdQFull[channelID]++;
							DEBUG("    == Channel Queue Full");
						}
					}
				}
			}
		}

		priorityPort++;
		if(priorityPort==NUM_PORTS) priorityPort = 0;

	}
	priorityPort++;
	if(priorityPort==NUM_PORTS) priorityPort = 0;


	if(DEBUG_BOB) DEBUG("== Move from channel return queue to SerDe buffer");
	for(unsigned link=0; link<NUM_LINK_BUSES; link++)
	{
		//make sure output is not busy sending something else
		if(inFlightResponseLinkCountdowns[link]==0 &&
		        serDesBufferResponse[link]==NULL)
		{

			for(unsigned z=0; z<CHANNELS_PER_LINK_BUS; z++)
			{
				unsigned chan = link * CHANNELS_PER_LINK_BUS + responseLinkRoundRobin[link];

				//make sure something is there
				if(channels[chan]->pendingLogicResponse!=NULL)
				{
					//calculate numbers to see how long the response is on the bus
					//
					//widths are in bits
					unsigned totalChannelCycles = (LOGIC_RESPONSE_PACKET_OVERHEAD * 8) / RESPONSE_LINK_BUS_WIDTH +
					                              !!((LOGIC_RESPONSE_PACKET_OVERHEAD * 8) % RESPONSE_LINK_BUS_WIDTH);

					if(LINK_BUS_USE_DDR)
					{
						totalChannelCycles = totalChannelCycles / 2 + !!(totalChannelCycles % 2);
					}

					//channel countdown
					inFlightResponseLinkCountdowns[link] = totalChannelCycles / LINK_CPU_CLK_RATIO +
					                                       !!(totalChannelCycles % LINK_CPU_CLK_RATIO);

					//make sure computation worked
					if(inFlightResponseLinkCountdowns[link]==0)
					{
						ERROR("== ERROR - Countdown 0 on link "<<link);
						ERROR("==         totalChannelCycles : "<<totalChannelCycles);
						exit(0);
					}

					//make in-flight
					if(inFlightResponseLink[link]!=NULL)
					{
						ERROR("== Error - Trying to set Transaction on down channel while something is there");
						ERROR("   Cycle : "<<currentClockCycle);
						ERROR(" Channel : "<<chan);
						ERROR(" LinkBus : "<<link);
						ERROR(" Incoming: "<<*(channels[chan]->pendingLogicResponse));
						ERROR(" Current : "<<*inFlightResponseLink[link]);
						exit(0);
					}

					//make in flight
					inFlightResponseLink[link] = channels[chan]->pendingLogicResponse;
					//remove from channel
					channels[chan]->pendingLogicResponse = NULL;

					if(DEBUG_BOB)
					{
						DEBUG("  == Link Bus "<<link<<" returning "<<*inFlightResponseLink[link]);
						DEBUG("     == CPU Clks:"<<inFlightResponseLinkCountdowns[link]<<"   Channel Clks:"<<totalChannelCycles<<" DDR?:"<<LINK_BUS_USE_DDR);
					}

					break;
				}
				else if(channels[chan]->readReturnQueue.size()>0)
				{
					//remove transaction from pending queue
					for(unsigned p=0; p<pendingReads.size(); p++)
					{
						//find pending item in pending queue
						if(pendingReads[p]->transactionID == channels[chan]->readReturnQueue[0]->transactionID)
						{
							//make the return packet
							pendingReads[p]->transactionType = RETURN_DATA;

							//calculate numbers to see how long the response is on the bus
							//
							//widths are in bits
							unsigned totalChannelCycles = ((RD_RESPONSE_PACKET_OVERHEAD + TRANSACTION_SIZE) * 8) / RESPONSE_LINK_BUS_WIDTH +
							                              !!(((RD_RESPONSE_PACKET_OVERHEAD+TRANSACTION_SIZE) * 8) % RESPONSE_LINK_BUS_WIDTH);

							if(LINK_BUS_USE_DDR)
							{
								totalChannelCycles = totalChannelCycles / 2 + !!(totalChannelCycles % 2);
							}

							//channel countdown
							inFlightResponseLinkCountdowns[link] = totalChannelCycles / LINK_CPU_CLK_RATIO
							                                       + !!(totalChannelCycles % LINK_CPU_CLK_RATIO);

							//make sure computation worked
							if(inFlightResponseLinkCountdowns[link]==0)
							{
								ERROR("== ERROR - Countdown 0 on link "<<link);
								exit(0);
							}

							//make in-flight
							if(inFlightResponseLink[link]!=NULL)
							{
								ERROR("== Error - Trying to set Transaction on down channel while something is there");
								ERROR("   Cycle : "<<currentClockCycle);
								ERROR(" Channel : "<<chan);
								ERROR(" LinkBus : "<<link);
								ERROR(" Incoming: "<<*pendingReads[p]);
								ERROR(" Current : "<<*inFlightResponseLink[link]);
								exit(0);
							}

							inFlightResponseLink[link] = pendingReads[p];
							//note the time
							inFlightResponseLink[link]->cyclesRspLink = currentClockCycle;

							if(DEBUG_BOB)
							{
								DEBUG("  == Link Bus "<<link<<" returning "<<*inFlightResponseLink[link]);
								DEBUG("     == CPU Clks:"<<inFlightResponseLinkCountdowns[link]<<"   Channel Clks:"<<totalChannelCycles<<" DDR?:"<<LINK_BUS_USE_DDR);
							}

							//remove pending queues
							pendingReads.erase(pendingReads.begin()+p);
							//delete channels[chan]->readReturnQueue[0];
							//channels[chan]->readReturnQueue.erase(channels[chan]->readReturnQueue.begin());

							break;
						}
					}

					//check to see if we cound an item, and break out of the loop over chans_per_link
					if(inFlightResponseLinkCountdowns[link]>0)
					{
						//increment round robin (increment here to go to next index before we break)
						responseLinkRoundRobin[link]++;
						if(responseLinkRoundRobin[link]==CHANNELS_PER_LINK_BUS)
							responseLinkRoundRobin[link]=0;

						break;
					}
				}

				//increment round robin (increment here if we found nothing)
				responseLinkRoundRobin[link]++;
				if(responseLinkRoundRobin[link]==CHANNELS_PER_LINK_BUS)
					responseLinkRoundRobin[link]=0;
			}




		}
	}
	//
	//Channels update at DRAM speeds (whatever ratio is with CPU)
	//

	if(currentClockCycle>0 && currentClockCycle%DRAM_CPU_CLK_RATIO==0)
	{
		if(DEBUG_CHANNEL) DEBUG(" --------- Channel updates started ---------");

		//if there is an adjustment, we need to increment counter and handle clock differences
		if(DRAM_CPU_CLK_ADJUSTMENT>0)
		{
			if(clockCycleAdjustmentCounter<DRAM_CPU_CLK_ADJUSTMENT)
			{
				for(unsigned i=0; i<NUM_CHANNELS; i++)
				{
					channels[i]->Update();
				}
			}
			else clockCycleAdjustmentCounter=0;

			clockCycleAdjustmentCounter++;
		}
		//if there is no adjustment, just update normally
		else
		{
			for(unsigned i=0; i<NUM_CHANNELS; i++)
			{
				channels[i]->Update();
			}
		}
	}


	if(DEBUG_BOB) DEBUG(endl<<" --------- BOB Update Ended ["<<currentClockCycle<<"] ---------");

	//increment clock cycle
	currentClockCycle++;
}

unsigned BOB::FindChannelID(Transaction* trans)
{
	unsigned channelID = 0;
	unsigned bitWidth = log2(NUM_CHANNELS);
	uint64_t channelMask = 0;
	unsigned channelIDOffset;// = log2(BUS_ALIGNMENT_SIZE) + CHANNEL_ID_OFFSET;

	switch(mappingScheme)
	{
	case BK_CLH_RW_RK_CH_CLL_BY://bank:col_high:row:rank:chan:col_low:by
		channelIDOffset = cacheOffset;//bus alignment + column low bits should make the cache offset
		break;
	case CLH_RW_RK_BK_CH_CLL_BY://col_high:row:rank:bank:chan:col_low:byte
		channelIDOffset = cacheOffset;//bus alignment + column low bits should make the cache offset
		break;
	case RK_BK_RW_CLH_CH_CLL_BY://rank:bank:row:col_high:chan:col_low:by
		channelIDOffset = cacheOffset;//bus alignment + column low bits should make the cache offset
		break;
	case RW_BK_RK_CH_CL_BY://row:bank:rank:chan:col:byte
		channelIDOffset = colBitWidth + busOffsetBitWidth;
		break;
	case RW_CH_BK_RK_CL_BY://row:chan:bank:rank:col:byte
		channelIDOffset = busOffsetBitWidth + colBitWidth + rankBitWidth + bankBitWidth;
		break;
	case RW_BK_RK_CLH_CH_CLL_BY://row:bank:rank:col_high:chan:col_low:byte
		channelIDOffset = cacheOffset; //bus alignment + column low bits should make the cache offset
		break;
	case RW_CLH_BK_RK_CH_CLL_BY://row:col_high:bank:rank:chan:col_low:byte
		channelIDOffset = cacheOffset;
		break;
	case CH_RW_BK_RK_CL_BY: //chan:row:bank:rank:col:byte
		channelIDOffset = rowBitWidth + bankBitWidth + rankBitWidth + colBitWidth + busOffsetBitWidth;
		break;
	default:
		ERROR("== ERROR - Unknown address mapping???");
		exit(1);
		break;
	};

	if(DEBUG_BOB) DEBUGN("    == Mapping "<<*trans);

	//build channel id mask
	for(unsigned i=0; i<bitWidth; i++)
	{
		channelMask = channelMask<<1;
		channelMask++;
	}

	channelMask = channelMask << channelIDOffset;
	channelID = (trans->address & channelMask) >> channelIDOffset;

	if(DEBUG_BOB) DEBUG(" to channel "<<channelID);
	return channelID;
}

//This is also kind of kludgey, but essentially this function always prints the power
// stats to the output file, but supresses the print to cout until finalPrint is true

void BOB::PrintStats(ofstream &statsOut, ofstream &powerOut, bool finalPrint, unsigned elapsedCycles)
{
	unsigned long dramCyclesElapsed;
	//check if we are on an epoch boundary or if there are cycles left over
	//
	//NOTE : subtract one from currentClockCycle since BOB object Update() has already been called
	//       before this call to PrintStats()

	if((currentClockCycle-1)%EPOCH_LENGTH==0)
	{
		//OLD - doesn't work for non-even ratios
		//dramCyclesElapsed = EPOCH_LENGTH / DRAM_CPU_CLK_RATIO;

		//NEW
		dramCyclesElapsed = EPOCH_LENGTH / ((float)tCK/(float)CPU_CLK_PERIOD);
	}
	else
	{
		//same as above
		dramCyclesElapsed = ((currentClockCycle-1)%EPOCH_LENGTH)/((float)tCK/(float)CPU_CLK_PERIOD);
	}

	//dramCyclesElapsed = channels[0]->currentClockCycle;

#define MAX_TMP_STR 512
	char tmp_str[MAX_TMP_STR];

	PRINT(" === BOB Print === ");
	unsigned long totalRequestsAtChannels = 0L;
	PRINT(" == Ports");
	for(unsigned p=0; p<NUM_PORTS; p++)
	{
		PRINT("  -- Port "<<p<<" - inputBufferAvg : "<<(float)portInputBufferAvg[p]/elapsedCycles<<" ("<<ports[p].inputBuffer.size()<<")   outputBufferAvg : "<<(float)portOutputBufferAvg[p]/elapsedCycles<<" ("<<ports[p].outputBuffer.size()<<")");
		portInputBufferAvg[p]=0;
		portOutputBufferAvg[p]=0;
	}

	//calculate possible bandwidth of part
	float dataperiod = tCK/2;
	float bw = (1/dataperiod)*64/8;//64-bit bus, 8 bits/byte

	unsigned numDevices = 64 / DEVICE_WIDTH;
	unsigned long totalBytesPerDevice = (NUM_COLS * NUM_ROWS * NUM_BANKS * DEVICE_WIDTH) / 8L; //in bytes
	unsigned long gigabytesPerRank = (numDevices * totalBytesPerDevice)>>30;

	//calculate peak bandwidth for link bus
	float reqbwpeak = 0;
	float rspbwpeak = 0;
	if(LINK_BUS_USE_DDR)
	{
		reqbwpeak = ((REQUEST_LINK_BUS_WIDTH * 2) / LINK_BUS_CLK_PERIOD)/8; //in bytes
		rspbwpeak = ((RESPONSE_LINK_BUS_WIDTH * 2) / LINK_BUS_CLK_PERIOD)/8; //in bytes
	}
	else
	{
		reqbwpeak = (REQUEST_LINK_BUS_WIDTH / LINK_BUS_CLK_PERIOD) / 8; //in bytes
		rspbwpeak = (RESPONSE_LINK_BUS_WIDTH / LINK_BUS_CLK_PERIOD) / 8; //in bytes
	}

	PRINT(" == Link Bandwidth (!! Includes packet overhead and request packets !!)")
	PRINT("    Req Link("<<reqbwpeak<<" GB/s peak)  Rsp Link("<<rspbwpeak<<" GB/s peak)");
	double reqtotal = 0;
	double rsptotal = 0;
	for(int l=0; l<NUM_LINK_BUSES; l++)
	{
		reqtotal += (1-(float)requestLinkIdle[l]/(float)elapsedCycles) * reqbwpeak;
		rsptotal += (1-(float)responseLinkIdle[l]/(float)elapsedCycles) * rspbwpeak;
		PRINTN("      "<<(1-(float)requestLinkIdle[l]/(float)elapsedCycles) * reqbwpeak<<" GB/s             ");
		requestLinkIdle[l]=0;
		PRINT((1-(float)responseLinkIdle[l]/(float)elapsedCycles) * rspbwpeak<<" GB/s");
		responseLinkIdle[l]=0;
	}


	PRINT("     -----------");
	PRINT("      "<<reqtotal/NUM_LINK_BUSES<<"          "<<rsptotal/NUM_LINK_BUSES<<" (avgs)");


	PRINT(" == Channel Usage and Stats ("<<(NUM_RANKS * gigabytesPerRank)<<"GB/Chan == "<<NUM_RANKS * gigabytesPerRank * NUM_CHANNELS<<" GB total)");
	PRINT("     reqs   workQAvg  workQMax idleBanks   actBanks  preBanks  refBanks  (totalBanks) BusIdle  BW("<<bw<<")  RRQMax("<<CHANNEL_RETURN_Q_MAX/TRANSACTION_SIZE<<")   RRQFull lifetimeRequests");
	float totalDRAMbw = 0;
	for(unsigned i=0; i<NUM_CHANNELS; i++)
	{
		//compute each DRAM channel's BW
		double DRAMBandwidth= (bw * (1 - ((double)channels[i]->DRAMBusIdleCount/(double)dramCyclesElapsed))) * 1E9 / (1<<30);

		statsOut<<DRAMBandwidth<<",";

		totalDRAMbw += DRAMBandwidth;
		totalRequestsAtChannels+=channelCounters[i];
		channelCountersLifetime[i]+=channelCounters[i];

		// since trying to actually format strings with stream operators is a huge pain
		snprintf(tmp_str, MAX_TMP_STR, "%d]%9d%10.4f%10d%10.4f%10.4f%10.4f%10.4f%10.4f%10.2f%10.3f%10d(%d)%10d%10ld\n",
		         i,
		         channelCounters[i],
		         (float)channels[i]->simpleController.commandQueueAverage/dramCyclesElapsed,
		         channels[i]->simpleController.commandQueueMax,
		         (float)channels[i]->simpleController.numIdleBanksAverage/dramCyclesElapsed,
		         (float)channels[i]->simpleController.numActBanksAverage/dramCyclesElapsed,
		         (float)channels[i]->simpleController.numPreBanksAverage/dramCyclesElapsed,
		         (float)channels[i]->simpleController.numRefBanksAverage/dramCyclesElapsed,
		         (float)(channels[i]->simpleController.numIdleBanksAverage+
		                 channels[i]->simpleController.numActBanksAverage+
		                 channels[i]->simpleController.numPreBanksAverage+
		                 channels[i]->simpleController.numRefBanksAverage)/dramCyclesElapsed,
		         100*((float)channels[i]->DRAMBusIdleCount/(float)dramCyclesElapsed),
		         DRAMBandwidth,
		         channels[i]->readReturnQueueMax,
		         (int)channels[i]->readReturnQueue.size(),
		         channels[i]->simpleController.RRQFull,
		         channelCountersLifetime[i]

		        );

		for(int r=0; r<NUM_RANKS; r++)
		{
			for(int b=0; b<NUM_BANKS; b++)
			{
				//PRINTN(channels[i]->simpleController.bankStates[r][b]);
			}
		}

		//reset
		channels[i]->simpleController.commandQueueAverage=0;
		channels[i]->simpleController.numIdleBanksAverage=0;
		channels[i]->simpleController.numActBanksAverage=0;
		channels[i]->simpleController.numPreBanksAverage=0;
		channels[i]->simpleController.numRefBanksAverage=0;
		channels[i]->simpleController.commandQueueMax=0;
		channels[i]->readReturnQueueMax=0;
		channels[i]->DRAMBusIdleCount=0;
		channelCounters[i]=0;
		channels[i]->simpleController.RRQFull=0;
		cmdQFull[i]=0;

		PRINTN(tmp_str);
	}

	//separates bandwidth numbers and power numbers
	statsOut<<";";

	PRINT("                                                                                          AVG : "<<totalDRAMbw/NUM_CHANNELS);
	PRINT(" == Requests seen at Channels");
	PRINT("  -- Reads  : "<<readCounter);
	PRINT("  -- Writes : "<<writeCounter);
	PRINT("            = "<<totalRequestsAtChannels);
	readCounter = 0;
	writeCounter = 0;
	totalRequestsAtChannels = 0;

	//
	//
	//POWER
	//
	//
	PRINT(" == Channel Power");
	powerOut<<currentClockCycle * CPU_CLK_PERIOD / 1000000<<",";

	bool detailedOutput = false;
	bool shortOutput = true;
	float allChanAveragePower = 0;
	for(unsigned c=0; c<NUM_CHANNELS; c++)
	{
		float totalChannelPower = 0;
		PRINTN("    -- Channel "<<c);
		for(unsigned r=0; r<NUM_RANKS; r++)
		{
			float backgroundPower = ((float)channels[c]->simpleController.backgroundEnergy[r] / (float) dramCyclesElapsed) * Vdd / 1000;
			float burstPower = ((float)channels[c]->simpleController.burstEnergy[r] / (float) dramCyclesElapsed) * Vdd / 1000;
			float actprePower = ((float)channels[c]->simpleController.actpreEnergy[r] / (float) dramCyclesElapsed) * Vdd / 1000;
			float refreshPower = ((float)channels[c]->simpleController.refreshEnergy[r] / (float) dramCyclesElapsed) * Vdd / 1000;

			float averagePower = ((float)(channels[c]->simpleController.actpreEnergy[r] +
			                              channels[c]->simpleController.backgroundEnergy[r] +
			                              channels[c]->simpleController.burstEnergy[r] +
			                              channels[c]->simpleController.refreshEnergy[r]) / (float) dramCyclesElapsed) * Vdd / 1000;
			totalChannelPower += averagePower;

			if(!shortOutput)
			{
				PRINTN("     -- Rank "<<r<<" : ");
				if(detailedOutput)
				{
					PRINT(setprecision(4)<<"TOT :"<<averagePower<<"  bkg:"<<backgroundPower<<" brst:"<<burstPower<<" ap:"<<actprePower<<" ref:"<<refreshPower);
				}
				else
				{
					PRINTN(setprecision(4)<<averagePower<<"w ");
				}
			}

			//clear for next epoch
			channels[c]->simpleController.backgroundEnergy[r]=0;
			channels[c]->simpleController.burstEnergy[r]=0;
			channels[c]->simpleController.actpreEnergy[r]=0;
			channels[c]->simpleController.refreshEnergy[r]=0;
		}

		allChanAveragePower += totalChannelPower;
		statsOut<<totalChannelPower<<",";
		powerOut<<totalChannelPower<<",";

		PRINT(" -- DRAM Power : "<<totalChannelPower<<" w");
	}

	PRINT("   Average Power  : "<<allChanAveragePower/NUM_CHANNELS<<" w");
	PRINT("   Total Power    : "<<allChanAveragePower<<" w");
	PRINT("   SimpCont BG Power : "<<NUM_LINK_BUSES * SIMP_CONT_BACKGROUND_POWER<<" w");
	PRINT("   SimpCont Core Power : "<<NUM_CHANNELS * SIMP_CONT_CORE_POWER<<" w");
	PRINT("   System Power   : "<<allChanAveragePower + NUM_LINK_BUSES * SIMP_CONT_BACKGROUND_POWER + NUM_CHANNELS * SIMP_CONT_CORE_POWER<<" w");

	statsOut << ";" << allChanAveragePower/NUM_CHANNELS << endl;

	//compute static power from controllers
	powerOut<<SIMP_CONT_BACKGROUND_POWER * NUM_LINK_BUSES + NUM_CHANNELS * SIMP_CONT_CORE_POWER <<",";
	powerOut<<(SIMP_CONT_BACKGROUND_POWER * NUM_LINK_BUSES + NUM_CHANNELS * SIMP_CONT_CORE_POWER) + allChanAveragePower<<endl;

	PRINT(" == Time Check");
	PRINT("    CPU Time : "<<currentClockCycle * CPU_CLK_PERIOD<<"ns");
	PRINT("   DRAM Time : "<<(channels[0]->currentClockCycle * tCK)<<"ns");
}


//
//Callback shit
//
void BOB::ReportCallback(BusPacket *bp, unsigned i)
{
	if(bp->busPacketType==ACTIVATE)
	{
		for(unsigned p=0; p<pendingReads.size(); p++)
		{
			if(pendingReads[p]->transactionID==bp->transactionID)
			{
				pendingReads[p]->dramStartTime = currentClockCycle;
				pendingReads[p]->cyclesInWorkQueue=bp->queueWaitTime;
				break;
			}
		}
	}
	else if(bp->busPacketType==WRITE_P)
	{
		if (writeIssuedCB)
		{
			(*writeIssuedCB)(bp->port, bp->address);
		}
	}
	else if(bp->busPacketType==WRITE_DATA)
	{
		committedWrites++;
	}
	else if(bp->busPacketType==READ_DATA)
	{
		for(unsigned p=0; p<pendingReads.size(); p++)
		{
			if(pendingReads[p]->transactionID==bp->transactionID)
			{
				pendingReads[p]->dramTimeTotal = currentClockCycle - pendingReads[p]->dramStartTime;
				break;
			}
		}
	}
	else
	{
		ERROR("== Error - Wrong type of packet received in bob");
		ERROR(*bp);
		exit(0);
	}
}

void BOB::RegisterWriteIssuedCallback(TransactionCompleteCB *cb)
{
	writeIssuedCB = cb;
}

} //namespace BOBSim


