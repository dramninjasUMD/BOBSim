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

#ifndef BUSPACKET_H
#define BUSPACKET_H

//Bus Packet header

#include "Globals.h"
#include <iostream>

using std::ostream;

namespace BOBSim
{
enum BusPacketType
{
	READ,
	READ_P,
	WRITE,
	WRITE_P,
	ACTIVATE,
	REFRESH,
	PRECHARGE,
	READ_DATA,
	WRITE_DATA
};

class BusPacket
{
public:
	//Functions
	BusPacket();
	BusPacket(BusPacketType packtype, unsigned transactionID, unsigned col, unsigned rw, unsigned r, unsigned b, unsigned prt, unsigned bl, unsigned mappedChannel, uint64_t address, bool fromLogic);
	void PrintVerification(uint64_t currentCycle);

	//Fields
	bool fromLogicOp;
	BusPacketType busPacketType;
	unsigned column;
	unsigned row;
	unsigned bank;
	unsigned rank;
	unsigned transactionID;
	unsigned port;
	unsigned queueWaitTime;
	unsigned burstLength;
	unsigned channel;
	uint64_t address;


};
ostream& operator<<(ostream &out, const BusPacket &bp);
}

#endif
