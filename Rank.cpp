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

//Rank source

#include "Rank.h"
#include "DRAMChannel.h"

using namespace std;
using namespace BOBSim;

Rank::Rank()
{
	currentClockCycle = 0;
}

Rank::Rank(unsigned rankid):
	ReadReturnCallback(NULL)
{
	currentClockCycle = 0;

	id = rankid;

	bankStates = (BankState*)calloc(sizeof(BankState), NUM_BANKS);
}

void Rank::Update()
{
	for(unsigned i=0; i<NUM_BANKS; i++)
	{
		bankStates[i].UpdateStateChange();
	}

	for(unsigned i=0; i<readReturnCountdown.size(); i++)
	{
		readReturnCountdown[i]--;
	}
	if(readReturnCountdown.size()>0 && readReturnCountdown[0]==0)
	{
		(*ReadReturnCallback)(readReturnQueue[0],id);
		readReturnCountdown.erase(readReturnCountdown.begin());
		readReturnQueue.erase(readReturnQueue.begin());
	}

	//increment clock cycle
	currentClockCycle++;
}

void Rank::ReceiveFromBus(BusPacket *busPacket)
{
	if(DEBUG_CHANNEL) DEBUG("     == Rank "<<id<<" received : " << *busPacket);

	if(VERIFICATION_OUTPUT && busPacket->channel==0)
	{
		busPacket->PrintVerification(currentClockCycle);
	}

	switch(busPacket->busPacketType)
	{
	case REFRESH:
		for(unsigned i=0; i<NUM_BANKS; i++)
		{
			if(bankStates[i].currentBankState != IDLE ||
			        bankStates[i].nextActivate > currentClockCycle)
			{
				ERROR("== Error - Refresh when not allowed in bank "<<i);
				ERROR("           NextAct : "<<bankStates[i].nextActivate);
				ERROR("           State : "<<bankStates[i].currentBankState);
				exit(0);
			}

			bankStates[i].lastCommand = REFRESH;
			bankStates[i].currentBankState = REFRESHING;
			bankStates[i].stateChangeCountdown = tRFC;
			bankStates[i].nextActivate = currentClockCycle + tRFC;
		}
		delete busPacket;
		break;
	case READ_P:
		if(bankStates[busPacket->bank].currentBankState != ROW_ACTIVE ||
		        bankStates[busPacket->bank].openRowAddress != busPacket->row ||
		        currentClockCycle < bankStates[busPacket->bank].nextRead)
		{
			ERROR("== Error - Rank receiving READ_P when not allowed");
			ERROR("           Current Clock Cycle : "<<currentClockCycle);
			ERROR(bankStates[busPacket->bank]);
			exit(0);
		}

		//
		//update bankstates
		//
		busPacket->busPacketType = READ_DATA;
		readReturnQueue.push_back(busPacket);
		readReturnCountdown.push_back(tCL);

		for(unsigned i=0; i<NUM_BANKS; i++)
		{
			bankStates[i].nextRead = max(bankStates[i].nextRead, currentClockCycle + tCCD);
			bankStates[i].nextWrite = max(bankStates[i].nextWrite, currentClockCycle + tCCD);
		}

		bankStates[busPacket->bank].lastCommand = READ_P;
		bankStates[busPacket->bank].stateChangeCountdown = tRTP;
		bankStates[busPacket->bank].nextActivate = currentClockCycle + tRTP + tRP;
		bankStates[busPacket->bank].nextRead = bankStates[busPacket->bank].nextActivate;
		bankStates[busPacket->bank].nextWrite = bankStates[busPacket->bank].nextActivate;
		break;
	case WRITE_P:
		if(bankStates[busPacket->bank].currentBankState != ROW_ACTIVE ||
		        bankStates[busPacket->bank].openRowAddress != busPacket->row ||
		        currentClockCycle < bankStates[busPacket->bank].nextWrite)
		{
			ERROR("== Error - Rank "<<id<<" receiving WRITE_P when not allowed");
			ERROR(bankStates[busPacket->bank]);
			ERROR("           currentClockCycle : "<<currentClockCycle);
			exit(0);
		}

		//
		//update bank states
		//

		for(unsigned i=0; i<NUM_BANKS; i++)
		{
			bankStates[i].nextRead = max(bankStates[i].nextRead, currentClockCycle + tCCD);
			bankStates[i].nextWrite = max(bankStates[i].nextWrite, currentClockCycle + tCCD);
		}
		bankStates[busPacket->bank].lastCommand = WRITE_P;
		bankStates[busPacket->bank].stateChangeCountdown = tCWL + TRANSACTION_SIZE/DRAM_BUS_WIDTH + tWR;
		bankStates[busPacket->bank].nextActivate = currentClockCycle + tCWL + busPacket->burstLength + tWR + tRP;
		bankStates[busPacket->bank].nextRead = bankStates[busPacket->bank].nextActivate;
		bankStates[busPacket->bank].nextWrite = bankStates[busPacket->bank].nextActivate;

		delete busPacket;
		break;
	case ACTIVATE:
		if(bankStates[busPacket->bank].currentBankState != IDLE ||
		        currentClockCycle < bankStates[busPacket->bank].nextActivate)
		{
			ERROR("== Error - Rank receiving ACT when not allowed");
			exit(0);
		}

		//
		//update bank states
		//
		bankStates[busPacket->bank].currentBankState = ROW_ACTIVE;
		bankStates[busPacket->bank].openRowAddress = busPacket->row;
		bankStates[busPacket->bank].nextRead = currentClockCycle + tRCD;
		bankStates[busPacket->bank].nextWrite = currentClockCycle + tRCD;
		bankStates[busPacket->bank].nextActivate = currentClockCycle + tRC;

		for(unsigned i=0; i<NUM_BANKS; i++)
		{
			if(i!=busPacket->bank)
			{
				bankStates[i].nextActivate = max(bankStates[i].nextActivate, currentClockCycle + tRRD);
			}
		}

		delete busPacket;
		break;
	case WRITE_DATA:
		if(bankStates[busPacket->bank].currentBankState != ROW_ACTIVE ||
		        bankStates[busPacket->bank].openRowAddress != busPacket->row)
		{
			ERROR("== Error - Clock Cycle : "<<currentClockCycle);
			ERROR("== Error - Rank receiving WRITE_DATA when not allowed: " << *busPacket <<endl << bankStates[busPacket->bank]);
			exit(0);
		}

		bankStates[busPacket->bank].stateChangeCountdown = tWR;
		bankStates[busPacket->bank].nextActivate = currentClockCycle + tWR + tRP;

		delete busPacket;
		break;
	default:
		ERROR("== Error - Rank receiving incorrect type of bus packet");
		break;
	}
}

void Rank::RegisterCallback(Callback<DRAMChannel, void, BusPacket*, unsigned> *readCB)
{
	ReadReturnCallback = readCB;
}
