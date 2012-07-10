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

#ifndef GLOBALS_H
#define GLOBALS_H

//Global fields header

#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <stdint.h>
#include "Callback.h"

#ifdef LOG_OUTPUT
#define PRINT(str)  {logOutput<< str<<std::endl;}
#define PRINTN(str) {logOutput<< str;}
#define DEBUG(str)  {if (BOBSim::SHOW_SIM_OUTPUT) {logOutput<< str <<std::endl;}}
#define DEBUGN(str) {if (BOBSim::SHOW_SIM_OUTPUT) {logOutput<< str;}}
namespace BOBSim
{
extern std::ofstream logOutput; //defined in BOBWrapper.cpp
}
#else

#ifdef NO_OUTPUT
#define PRINT(str)
#define PRINTN(str)
#define DEBUG(str)
#define DEBUGN(str)

#else
#define PRINT(str)  { std::cout <<str<<std::endl; }
#define PRINTN(str) { std::cout <<str; }
#define DEBUG(str)  {if (BOBSim::SHOW_SIM_OUTPUT) {std::cout<< str <<std::endl;}}
#define DEBUGN(str) {if (BOBSim::SHOW_SIM_OUTPUT) {std::cout<< str;}}
#endif
#endif

#ifdef LOG_OUTPUT
#endif

#define ERROR(str) std::cerr<<"[ERROR ("<<__FILE__<<":"<<__LINE__<<")]: "<<str<<std::endl;

namespace BOBSim
{
enum PortHeuristicScheme
{
	FIRST_AVAILABLE,
	ROUND_ROBIN,
	PER_CORE
};

enum AddressMappingScheme
{
	RW_BK_RK_CH_CL_BY, //row:bank:rank:chan:col:byte
	RW_CH_BK_RK_CL_BY, //row:chan:bank:rank:col:byte
	RW_BK_RK_CLH_CH_CLL_BY, //row:bank:rank:col_high:chan:col_low:byte
	RW_CLH_BK_RK_CH_CLL_BY, //row:col_high:bank:rank:chan:col_low:byte
	CH_RW_BK_RK_CL_BY, //chan:row:bank:rank:col:byte
	RK_BK_RW_CLH_CH_CLL_BY, //chan:rank:bank:row:col:byte
	CLH_RW_RK_BK_CH_CLL_BY, //col_high:row:rank:bank:chan:col_low:byte
	BK_CLH_RW_RK_CH_CLL_BY //bank:col_high:row:rank:chan:col_low:byte
};

//
//Debug Flags
//
//Prints debug information relating to the DRAM channel
static bool DEBUG_CHANNEL = false;
//Prints debug information relating to the main BOB controller and link buses 
static bool DEBUG_BOB =  false;
//Prints debug information relating to the ports on the main BOB controller
static bool DEBUG_PORTS = false;
//prints debug information relating to the logic layer in each simple controller
static bool DEBUG_LOGIC = false;

//Verification for modelsim
static bool VERIFICATION_OUTPUT = false;
extern std::ofstream verificationOutput;

//
//Random Stream Sim stuff
//
//Number of CPU cycles between epoch boundaries.  
static uint EPOCH_LENGTH = 1000000; 
//Flag for quiet mode (-q)
extern int SHOW_SIM_OUTPUT;

//Total number of reads (as a percentage) in request stream
static float READ_WRITE_RATIO = 66.6;//%
//Frequency of requests - 0.0 = no requests; 1.0 = as fast as possible
static float PORT_UTILIZATION = 1.0;

//Changes random seeding for request stream
static uint SEED_CONSTANT = 11;

//Text appended to output filenames for differentiating between simulations
static const char *VARIANT_SUFFIX = "_testing";

//QEMU defined total memory size
extern uint64_t QEMU_MEMORY_SIZE;

//
//BOB Architecture Config
//
//
//NOTE : NUM_LINK_BUSES * CHANNELS_PER_LINK_BUS = NUM_CHANNELS
//
//Number of link buses in the system
static uint NUM_LINK_BUSES = 4;
//Number of DRAM channels in the system
static uint NUM_CHANNELS = 8;
//Multi-channel optimization degree
static uint CHANNELS_PER_LINK_BUS = 2;

//Number of lanes for both request and response link bus
static uint REQUEST_LINK_BUS_WIDTH = 8;//Bit Lanes
static uint RESPONSE_LINK_BUS_WIDTH = 12; //Bit Lanes

//Clock frequency for link buses
//static float LINK_BUS_CLK_PERIOD = .15625; // ns - 6.4 GHz
static float LINK_BUS_CLK_PERIOD = .3125; // ns - 3.2 GHz
//Ratio between CPU and link bus clocks - computed at runtime
extern uint LINK_CPU_CLK_RATIO;
//Flag to turn on/off double-data rate transfer on link bus
static bool LINK_BUS_USE_DDR = true;

//Size of DRAM request
static uint TRANSACTION_SIZE = 64;
//Width of DRAM bus as standardized by JEDEC
static uint DRAM_BUS_WIDTH = 16; //bytes - DOUBLE TO ACCOUNT FOR DDR - 64-bits wide JEDEC bus

//Number of ports on main BOB controller
extern uint NUM_PORTS;
//Number of bytes each port can transfer in a CPU cycle
static uint PORT_WIDTH = 16;
//Number of transaction packets that each port buffer may hold
static uint PORT_QUEUE_DEPTH = 8;
//Heuristic used for adding new requests to the available ports
static PortHeuristicScheme portHeuristic = FIRST_AVAILABLE;

//Number of requests each simple controller can hold in its work queue
static uint CHANNEL_WORK_Q_MAX = 16; //entries
//Amount of response data that can be held in each simple controller return queue
static uint CHANNEL_RETURN_Q_MAX = 1024; //bytes

//
//Logic Layer Stuff
//
//Lets logic responses be sent before regular data requests
static bool GIVE_LOGIC_PRIORITY = true;
//Size of logic request packet
static uint LOGIC_RESPONSE_PACKET_OVERHEAD = 8;

//
//Packet sizes
//
//Packet overhead for requests and responses
static uint RD_REQUEST_PACKET_OVERHEAD = 8; //bytes
static uint RD_RESPONSE_PACKET_OVERHEAD = 8; //bytes
static uint WR_REQUEST_PACKET_OVERHEAD = 8; //bytes

//
//CPU
//
//CPU clock frequency in nanoseconds
static float CPU_CLK_PERIOD = 0.3125; //ns

//
//Power
//
//Average power consumption of a single simple controller package
static uint SIMP_CONT_BACKGROUND_POWER = 7;//watts
//Additional power consumption for each simple controller core in the package
static float SIMP_CONT_CORE_POWER = 3.5;//watts

//
//DRAM Stuff
//
//Alignment to determine width of DRAM bus, used in mapping
static uint BUS_ALIGNMENT_SIZE = 8;
//Cache line size in bytes 
static uint CACHE_LINE_SIZE = 64; 
//Offset of channel ID
static uint CHANNEL_ID_OFFSET = 0;
//Ratio of clock speeds between DRAM and CPU - computed at runtime
extern uint DRAM_CPU_CLK_RATIO;
//Address mapping scheme - defined at the top of this file
static AddressMappingScheme mappingScheme = RW_CLH_BK_RK_CH_CLL_BY;

//
//DRAM Timing
//
#ifdef DDR3_1333
//DDR3-1333 Micron Part : MT41J1G4-15E
//Clock Rate : 666MHz
static uint NUM_RANKS = 4;
static uint NUM_BANKS = 8;
static ulong NUM_ROWS = 65536;
static ulong NUM_COLS = 2048;

static ulong DEVICE_WIDTH = 4;
static uint BL = 8; //only used in power calculation

static float Vdd = 1.5;

//CLOCK PERIOD
static float tCK = 1.5; //ns

//in clock ticks
//ACT to READ or WRITE
static uint tRCD = 9;
//PRE command period
static uint tRP = 9;
//ACT to ACT
static uint tRC = 33;
//ACT to PRE
static uint tRAS = 24;

//CAS latency
static uint tCL = 9;
//CAS Write latency
static uint tCWL = 7;

//ACT to ACT (different banks)
static uint tRRD = 4;
//4 ACT Window
static uint tFAW = 20;
//WRITE recovery
static uint tWR = 10;
//WRITE to READ
static uint tWTR = 5;
//READ to PRE
static uint tRTP = 5;
//CAS to CAS
static uint tCCD = 4;
//REF to ACT
static uint tRFC = 107;
//CMD time
static uint tCMDS = 1;
//Rank to rank switch
static uint tRTRS = 2; //clk

//IDD Values
static uint IDD0 = 75;
static uint IDD1 = 90;
static uint IDD2P0 = 12;
static uint IDD2P1 = 30;
static uint IDD2Q = 35;
static uint IDD2N = 40;
static uint IDD2NT = 55;
static uint IDD3P = 35;
static uint IDD3N = 45;
static uint IDD4R = 150;
static uint IDD4W = 155;
static uint IDD5B = 230;
static uint IDD6 = 12;
static uint IDD6ET = 16;
static uint IDD7 = 290;
static uint IDD8 = 0;
#endif
#ifdef DDR3_1600
//DDR3-1600 Micron Part : MT41J256M4-125E
//Clock Rate : 800MHz
static uint NUM_RANKS = 4;
static uint NUM_BANKS = 8;
static uint NUM_ROWS = 16384;
static uint NUM_COLS = 2048;

static uint DEVICE_WIDTH = 4;
static uint BL = 8; //only used in power calculation

static float Vdd = 1.5;

//CLOCK PERIOD
static float tCK = 1.25; //ns

//in clock ticks
//ACT to READ or WRITE
static uint tRCD = 11;
//PRE command period
static uint tRP = 11;
//ACT to ACT
static uint tRC = 39;
//ACT to PRE
static uint tRAS = 28;

//CAS latency
static uint tCL = 11;
//CAS Write latency
static uint tCWL = 8;

//ACT to ACT (different banks)
static uint tRRD = 5;
//4 ACT Window
static uint tFAW = 24;
//WRITE recovery
static uint tWR = 12;
//WRITE to READ
static uint tWTR = 6;
//READ to PRE
static uint tRTP = 6;
//CAS to CAS
static uint tCCD = 4;
//REF to ACT
static uint tRFC = 88;
//CMD time
static uint tCMDS = 1;
//Rank to rank switch
static uint tRTRS = 2; //clk

//IDD Values
static uint IDD0 = 95;
static uint IDD1 = 115;
static uint IDD2P0 = 12;
static uint IDD2P1 = 45;
static uint IDD2Q = 67;
static uint IDD2N = 70;
static uint IDD2NT = 95;
static uint IDD3P = 45;
static uint IDD3N = 67;
static uint IDD4R = 250;
static uint IDD4W = 250;
static uint IDD5B = 260;
static uint IDD6 = 6;
static uint IDD6ET = 9;
static uint IDD7 = 400;
static uint IDD8 = 0;
#endif
#ifdef DDR3_1066
//DDR3-1066 Micron Part : MT41J512M4-187E
//Clock Rate : 553MHz
static uint NUM_RANKS = 4;
static uint NUM_BANKS = 8;
static uint NUM_ROWS = 32768;
static uint NUM_COLS = 2048;

static uint DEVICE_WIDTH = 4;
static uint BL = 8; //only used in power calculation

static float Vdd = 1.5;

//CLOCK PERIOD
static float tCK = 1.875; //ns

//in clock ticks
//ACT to READ or WRITE
static uint tRCD = 7; //13.125ns
//PRE command period
static uint tRP = 7; //13.125ns
//ACT to ACT
static uint tRC = 27; //50.625ns
//ACT to PRE
static uint tRAS = 20; //37.5ns

//CAS latency
static uint tCL = 7;
//CAS Write latency
static uint tCWL = 6;

//ACT to ACT (different banks)
static uint tRRD = 4; //7.5ns
//4 ACT Window
static uint tFAW = 20; //37.5ns
//WRITE recovery
static uint tWR = 8; //15ns
//WRITE to READ
static uint tWTR = 4; //7.5ns
//READ to PRE
static uint tRTP = 4; //7.5ns
//CAS to CAS
static uint tCCD = 4; //7.5ns
//REF to ACT
static uint tRFC = 86; //160ns
//CMD time
static uint tCMDS = 1; //clk
//Rank to rank switch
static uint tRTRS = 2; //clk

//IDD Values
static uint IDD0 = 90;
static uint IDD1 = 115;
static uint IDD2P0 = 12;
static uint IDD2P1 = 35;
static uint IDD2Q = 65;
static uint IDD2N = 70;
static uint IDD2NT = 90;
static uint IDD3P = 55;
static uint IDD3N = 80;
static uint IDD4R = 200;
static uint IDD4W = 255;
static uint IDD5B = 290;
static uint IDD6 = 10;
static uint IDD6ET = 14;
static uint IDD7 = 345;
static uint IDD8 = 0;
#endif

uint inline log2(unsigned value)
{
	uint logbase2 = 0;
	unsigned orig = value;
	value>>=1;
	while(value>0)
	{
		value >>= 1;
		logbase2++;
	}
	if(1<<logbase2<orig)logbase2++;
	return logbase2;
}

uint inline log2_64(uint64_t value)
{
	uint logbase2 = 0;
	uint64_t orig = value;
	value>>=1;
	while(value>0)
	{
		value>>=1;
		logbase2++;
	}
	if(1<<logbase2<orig)logbase2++;
	return logbase2;
}
}

#endif
