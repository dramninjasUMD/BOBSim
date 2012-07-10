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

#ifndef RANK_H
#define RANK_H

//Rank header

#include "Globals.h"
#include "BankState.h"
#include "SimulatorObject.h"

using namespace std;

namespace BOBSim
{
class DRAMChannel;
class Rank : public SimulatorObject
{
public:
	//Functions
	Rank();
	Rank(unsigned id);
	void Update();
	void ReceiveFromBus(BusPacket *busPacket);
	void RegisterCallback(Callback<DRAMChannel, void, BusPacket*, unsigned> *readCB);

	//Fields
	//Rank ID in relation to the channel
	unsigned id;

	//Amount of time until data is returend from the DRAM
	vector<unsigned> readReturnCountdown;
	//Storage for response data
	vector<BusPacket*> readReturnQueue;

	//Callback for returning data
	Callback<DRAMChannel, void, BusPacket*, unsigned> *ReadReturnCallback;
	
	//State of all banks in the DRAM channel
	BankState *bankStates;
};
}

#endif
