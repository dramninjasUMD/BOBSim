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

//Bus Packet source

#include "BusPacket.h"

using namespace std;

namespace BOBSim
{
BusPacket::BusPacket():
	busPacketType(READ),
	column(0),
	row(0),
	bank(0),
	rank(0),
	transactionID(0),
	port(0),
	burstLength(0),
	queueWaitTime(0),
	channel(0),
	fromLogicOp(false)
{}

BusPacket::BusPacket(BusPacketType packtype, unsigned id, unsigned col, unsigned rw, unsigned rnk, unsigned bnk, unsigned prt, unsigned bl, unsigned mappedChannel, uint64_t addr, bool fromLogic):
	burstLength(bl),
	busPacketType(packtype),
	transactionID(id),
	column(col),
	row(rw),
	bank(bnk),
	rank(rnk),
	port(prt),
	channel(mappedChannel),
	queueWaitTime(0),
	address(addr),
	fromLogicOp(fromLogic)
{}

void BusPacket::PrintVerification(uint64_t currentCycle)
{
	switch(busPacketType)
	{
	case ACTIVATE:
		verificationOutput << currentCycle <<": activate (" << rank << "," << bank << "," << row <<");"<<endl;
		break;
	case READ_P:
		verificationOutput << currentCycle << ": read ("<<rank<<","<<bank<<","<<column<<",1,0);"<<endl;
		break;
	case WRITE_P:
		verificationOutput << currentCycle << ": write ("<<rank<<","<<bank<<","<<column<<",1,0,'h0);"<<endl;
		break;
	case REFRESH:
		verificationOutput << currentCycle <<": refresh (" << rank << ");"<<endl;
		break;
	case WRITE_DATA:

		break;
	default:
		ERROR("== Error - Unsupported bus packet type in verification");
		exit(0);
		break;
	}
}

ostream& operator<<(ostream &out, const BusPacket &bp)
{

	switch(bp.busPacketType)
	{
	case READ:
		out<<"BP [READ]";
		break;
	case READ_P:
		out<<"BP [READ_P]";
		break;
	case WRITE:
		out<<"BP [WRITE]";
		break;
	case WRITE_P:
		out<<"BP [WRITE_P]";
		break;
	case ACTIVATE:
		out<<"BP [ACT]";
		break;
	case REFRESH:
		out<<"BP [REF]";
		break;
	case PRECHARGE:
		out<<"BP [PRE]";
		break;
	case WRITE_DATA:
		out<<"BP [WRITE_DATA]";
		break;
	case READ_DATA:
		out<<"BP [READ_DATA]";
		break;
	default:
		ERROR("Trying to print unknown kind of bus packet");
		ERROR("Type : "<<bp.busPacketType);
		exit(-1);
	}


	out << " id[" <<bp.transactionID<<"] r["<<bp.rank<<"] b["<<bp.bank<<"] row["<<hex<<bp.row<<"] col["<<bp.column<<dec<<"] bl["<<bp.burstLength<<"] "<<((bp.fromLogicOp)?"fromLogic":"");
	//reset format back to decimal
	return out;
}
}
