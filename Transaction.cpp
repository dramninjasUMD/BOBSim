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

//Transaction source

#include "Transaction.h"

using namespace std;
namespace BOBSim
{
Transaction::Transaction(TransactionType transType, unsigned size, uint64_t addr):
	transactionType(transType),
	address(addr),
	mappedChannel(0),
	transactionSize(size),
	cyclesReqPort(0),
	cyclesRspPort(0),
	cyclesReqLink(0),
	cyclesRspLink(0),
	cyclesInReadReturnQ(0),
	cyclesInWorkQueue(0),
	fullStartTime(0),
	fullTimeTotal(0),
	dramStartTime(0),
	dramTimeTotal(0),
	channelStartTime(0),
	channelTimeTotal(0),
	portID(0),
	coreID(0),
	originatedFromLogicOp(false)
{
	transactionID = globalID++;
}

ostream& operator<<(ostream &out, const Transaction &t)
{
	if(t.transactionType == DATA_READ)
	{
		out <<"T"<<t.transactionID<<" [Read] [0x" << hex << setw(8)<<setfill('0')<<t.address << dec << "] port["<<t.portID<<"] core["<<t.coreID<<"] "<<((t.originatedFromLogicOp)?"[FromLogic]":"");
	}
	else if(t.transactionType == DATA_WRITE)
	{
		out <<"T"<<t.transactionID<<" [Write] [0x" << hex << setw(8)<<setfill('0')<<t.address << dec << "] port["<<t.portID<<"] core["<<t.coreID<<"] "<<((t.originatedFromLogicOp)?"[FromLogic]":"");
	}
	else if(t.transactionType == RETURN_DATA)
	{
		out << "T"<<t.transactionID<<" [Data] [0x" << hex << setw(8)<<setfill('0')<<t.address << dec << "] port["<<t.portID<<"] core["<<t.coreID<<"]";
	}
	else if(t.transactionType == LOGIC_OPERATION)
	{
		out << "T" << t.transactionID << " [LogicOp] [0x" << hex << setw(8) << setfill('0') << t.address << dec << "]";
	}
	else if(t.transactionType == LOGIC_RESPONSE)
	{
		out << "T" << t.transactionID << " [LogicResponse] [0x" << hex << setw(8) << setfill('0') << t.address << dec<<"]";
	}
	else
	{
		out << "T" << t.transactionID << " [?"<<t.transactionType<<"?] [0x" << hex << setw(8) << setfill('0') << t.address << dec<<"]";
		out << "unknown transaction type"<<endl;;
		exit(0);
	}

	return out;
}
}
