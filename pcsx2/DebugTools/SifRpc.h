/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#if 0
Here my understand of RPC/SIF. Line marked with H: are hypothesis

Architecture is client/server and mostly symetric.

/* Init phase */
Command must be send to a specific address to be process. EE reception side is
the value of SBUS_F210 which is set by IOP. IOP reception side will be set by
EE with a SIF_CMD_INIT RPC.
H: they also use the INIT phase to synchronize both side.

/* Command info */
There are 2 kinds of command: system (bit 31 is 1) and user. All commands begin
with an header (SifCmdHdr) that contains the size of the header and the data.
Note: data are send to another address.

You can install new user command (fcode) with the SifAddCmdHandler (after
allocating a cmd buffer with SifSetCmdBuffer). By default only system
cmd are registered.

SIF_CMD_CH_SADDR	=> change source address (after init)
SIF_CMD_SREG		=> set sif regs. Note there is 32 registers on the EE side, including system register SBUS_F2*
					   h: Likely to be used only on IOP-EE transfer. A get reg command mights exists too.
SIF_CMD_INIT		=> init + set source address
SIF_CMD_RESET		=> reset iop with a new set of module
SIF_CMD_RSP 		=> response of a RPC transaction
SIF_CMD_BIND		=> Allow to access a server service by an ID. Server will reply with the server address
					   See Server ID chapter
SIF_CMD_CALL		=> Call a RPC service.
SIF_CMD_OTHERDATA	=> ??? Not yet done :P

/* Server ID / Server info */
Server address is dynamic (depends of the loading of others modules). So we get
the address of the server by a compile time constant. On the client side it is
parameter of SifBindRpc (SIF_CMD_BIND) function. On the server side it is a
parameter of the SifRegisterRpc function (likely to be call at startup of the
module).

The server is a actually a server. Some server parameter will be populated by the SifCallRpc function.
Parameters of the server side: 
	* the service number (function is set by SifRegisterRpc)
		+ a buffer of data to send
		+ size of buffer
	* the reply calling mode (default, asynchronous, no cache write back)
Parameters of the client side:
	* a function to be called when rsp is done (executed as an interrupt handler)
		+ buffer of data for response
		+ size of buffer

Additional paramater had already been set by the SifRegisterRpc
	* A function to be called when request is received
		+ buffer
		+ size of buffer
	* A function to be called when the request was cancelled (not sure it was implemented in the end)
		+ buffer
		+ size of buffer

Note the server will also contains the packet source address and the sif command (often SIF_CMD_CALL)
#endif

enum CONST_SIF_CMD
{
	SIF_CMD_CH_SADDR	= 0x80000000,
	SIF_CMD_SREG		= 0x80000001,
	SIF_CMD_INIT		= 0x80000002,
	SIF_CMD_RESET		= 0x80000003,
	SIF_CMD_RSP 		= 0x80000008,
	SIF_CMD_BIND		= 0x80000009,
	SIF_CMD_CALL		= 0x8000000A,
	SIF_CMD_OTHERDATA	= 0x8000000C,
};

namespace sif_tracer {
	extern void analyze_dma_transfer(const u32 sif_number, u32 transfers, u32 count);
}

struct SifCmdHdr {
	u32 h_size:8; // Size of command packet including this header (16 <= psize <= 112);
	u32 d_size:24; // Size of accompanying data that is sent together with packet
	u32 d_addr; // Address of accompanying data
	u32 fcode; // Number of command function that is called
	u32 opt; // Data area that programmer can use
};

struct SifRsp {
	SifCmdHdr h;
	u32 unknown1;
	u32	p_src;
	u32 p_id;
	u32 client_data_ptr;
	u32 req_cmd;
	u32 server_data_ptr;
};

struct SifBind {
	SifCmdHdr h;
	u32 unknown1;
	u32	p_src;
	u32 p_id;
	u32 client_data_ptr;
	u32 server_id;
};

struct SifCall {
	SifCmdHdr h;
	u32 unknown1;
	u32	p_src;
	u32 p_id;
	u32 client_data_ptr;
	u32 fno;
	u32 size;
	u32 rsp_add;
	u32 rsp_size;
	u32 rsp_mode;
	u32 server_data_ptr;
};

struct SifDma {
	u32 src;
	u32 dest;
	u32 size;
	u32 attr;
};

// For reference
#if 0

typedef struct _sif_client_data {
	struct _sif_rpc_data rpcd;
	unsigned int command;
	void *buff;
	void *gp;
	sceSifEndFunc func;
	void *para;
	struct _sif_serve_data *serve;
} sceSifClientData;

typedef struct _sif_queue_data {
	int key;
	int active;
	struct _sif_serve_data *link;
	struct _sif_serve_data *start;
	struct _sif_serve_data *end;
	struct _sif_queue_data *next;
} sceSifQueueData;

typedef struct _sif_receive_data {
	struct _sif_rpc_data rpcd;
	void *src;
	void *dest;
	int size;
} sceSifReceiveData;

typedef struct _sif_rpc_data {
	void *paddr; Packet address
		unsigned int pid; Packet ID
		int tid; Thread ID
		unsigned int mode;
} sceSifRpcData;

typedef void * (* sceSifRpcFunc)(
		unsigned int fno, fno of sceSifCallRpc()
		void *data, Address where receive data is stored
		int size) Size of receive data

typedef void (* sceSifEndFunc)(
		void *data);

typedef struct _sif_serve_data {
	unsigned int command;
	sceSifRpcFunc func;
	void *buff;
	int size;
	sceSifRpcFunc cfunc;
	void *cbuff;
	int csize;
	sceSifClientData *client;
	void *paddr;
	unsigned int fno;
	void *receive;
	int rsize;
	int rmode;
	unsigned int rid;
	struct _sif_serve_data *link;
	struct _sif_serve_data *next;
	struct _sif_queue_data *base;
} sceSifServeData;

typedef struct {
	sceSifCmdHdr	chdr;
	int		size;
	int		flag;
	char		arg[80];
} sceSifCmdResetData;

#endif
