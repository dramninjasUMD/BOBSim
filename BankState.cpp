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

//Bank State source

#include "BankState.h"

using namespace std;
namespace BOBSim
{

BankState::BankState():
	currentBankState(IDLE),
	openRowAddress(0),
	nextActivate(0),
	nextRead(0),
	nextWrite(0),
	nextStrobeMin(0),
	nextStrobeMax(0),
	nextRefresh(0),
	stateChangeCountdown(0)
{}

void BankState::UpdateStateChange()
{
	if(stateChangeCountdown>0)
	{
		stateChangeCountdown--;

		if(stateChangeCountdown==0)
		{
			switch(lastCommand)
			{
			case REFRESH:
				currentBankState = IDLE;
				break;
			case WRITE_P:
			case READ_P:
				currentBankState = PRECHARGING;
				stateChangeCountdown = tRP;
				lastCommand = PRECHARGE;
				break;
			case PRECHARGE:
				currentBankState = IDLE;
				break;
			default:
				ERROR("== WTF STATE? : "<<lastCommand);
				exit(0);
			}
		}
	}
}

ostream &operator<<(ostream &out, const BankState &bs)
{
	//out << dec <<" == Bank State " <<endl;
	switch(bs.currentBankState)
	{
	case IDLE:
		out<<"    State : Idle" <<endl;
		break;
	case ROW_ACTIVE:
		out<<"    State : Active" <<endl;
		break;
	case PRECHARGING:
		out<<"    State : Precharging" <<endl;
		break;
	case REFRESHING:
		out<<"    State : Refreshing" <<endl;
		break;
	}
	/*
	out<<"    StateChange    : " << bs.stateChangeCountdown<<endl;
	out<<"    OpenRowAddress : " << bs.openRowAddress <<endl;
	out<<"    nextRead       : " << bs.nextRead <<endl;
	out<<"    nextWrite      : " << bs.nextWrite <<endl;
	out<<"    nextActivate   : " << bs.nextActivate <<endl;
	out<<"    nextStrobeMin  : " << bs.nextStrobeMin << endl;
	out<<"    nextStrobeMax  : " << bs.nextStrobeMax << endl;
	*/
	return out;
}
}
