#include "nonport.h"

#define NITRO_SOCKET_ERROR -1
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
gsi_time current_time()  //returns current time in milliseconds
{ 
#if defined(_WIN32)
	return (GetTickCount()); 

#elif defined(_PS2)
	unsigned int ticks;
	static unsigned int msec = 0;
	static unsigned int lastticks = 0;
	sceCdCLOCK lasttimecalled; /* defined in libcdvd.h */

	if(!msec)
	{
		sceCdReadClock(&lasttimecalled); /* libcdvd.a */
		msec =  (unsigned int)(DEC(lasttimecalled.day) * 86400000) +
				(unsigned int)(DEC(lasttimecalled.hour) * 3600000) +
				(unsigned int)(DEC(lasttimecalled.minute) * 60000) +
				(unsigned int)(DEC(lasttimecalled.second) * 1000);
	}

	ticks = (unsigned int)GetTicks();
	if(lastticks > ticks)
		msec += (unsigned int)(((unsigned int)(-1) - lastticks) + ticks) / 300000;
	else
		msec += (unsigned int)(ticks-lastticks) / 300000;
	lastticks = ticks;

	return msec;

#elif defined(_UNIX)
	struct timeval time;
	
	gettimeofday(&time, NULL);
	return (time.tv_sec * 1000 + time.tv_usec / 1000);

#elif defined(_NITRO)
	assertWithLine(OS_IsTickAvailable() == TRUE, 265);
	return (gsi_time)OS_TicksToMilliSeconds(OS_GetTick());

#elif defined(_PSP)
	struct SceRtcTick ticks;
	int result = 0;

	result = sceRtcGetCurrentTick(&ticks);
	if (result < 0)
	{
		ScePspDateTime time;
		result = sceRtcGetCurrentClock(&time, 0);
		if (result < 0)
			return 0; // um...error handling? //Nope, should return zero since time cannot be zero					  
		result = sceRtcGetTick(&time, &ticks);
		if (result < 0)
			return 0; //Nope, should return zero since time cannot be zero
	}

	return (gsi_time)(ticks.tick / 1000);

#elif defined(_PS3)
	return (gsi_time)(sys_time_get_system_time()/1000);

#elif defined(_REVOLUTION)
	OSTick aTickNow= OSGetTick();
	gsi_time aMilliseconds = (gsi_time)OSTicksToMilliseconds(aTickNow);
	return aMilliseconds;
#else
	// unrecognized platform! contact devsupport
	assert(0);
#endif
	
}

void msleep(gsi_time msec)
{
#if defined(_WIN32)
	Sleep(msec);

#elif defined(_PS2)
	#ifdef SN_SYSTEMS
		sn_delay((int)msec);
	#endif
	#ifdef EENET
		if(msec >= 1000)
		{
			sleep(msec / 1000);
			msec -= (msec / 1000);
		}
		if(msec)
			usleep(msec * 1000);
	#endif
	#ifdef INSOCK
		DelayThread(msec * 1000);
	#endif

#elif defined(_PSP)
	sceKernelDelayThread(msec * 1000);

#elif defined(_UNIX)
	usleep(msec * 1000);

#elif defined(_NITRO)
	OS_Sleep(msec);

#elif defined(_PS3)
	sys_timer_usleep(msec* 1000);
#elif defined (_REVOLUTION)
	OSSleepMilliseconds(msec);
#else
	assert(0); // missing platform handler, contact devsupport
#endif
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SocketStartUp()
{
#if defined(_WIN32) 
	WSADATA data;

	#if defined(_X360)
		XNetStartupParams xnsp;
		memset(&xnsp,0,sizeof(xnsp));
		xnsp.cfgSizeOfStruct=sizeof(xnsp);
		xnsp.cfgFlags=XNET_STARTUP_BYPASS_SECURITY;
		if(0 != XNetStartup(&xnsp))
		{
			OutputDebugString("XNetStartup failed\n");
		}
	#endif

	// added support for winsock2
	#if (!defined(_XBOX) || defined(_X360)) && (defined(GSI_WINSOCK2) || defined(_X360))
		WSAStartup(MAKEWORD(2,2), &data);
	#else
		WSAStartup(MAKEWORD(1,1), &data);
	#endif
	// end added
#endif
}

void SocketShutDown()
{
#if defined(_WIN32)
	WSACleanup();
	#if defined(_X360)
		XNetCleanup();
	#endif
#endif
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
char * goastrdup(const char *src)
{
	char *res;
	if(src == NULL)      //PANTS|02.11.00|check for NULL before strlen
		return NULL;
	res = (char *)gsimalloc(strlen(src) + 1);
	if(res != NULL)      //PANTS|02.02.00|check for NULL before strcpy
		strcpy(res, src);
	return res;
}

#if !defined(_WIN32)

char *_strlwr(char *string)
{
	char *hold = string;
	while (*string)
	{
		*string = (char)tolower(*string);
		string++;
	}

	return hold;
}

char *_strupr(char *string)
{
	char *hold = string;
	while (*string)
	{
		*string = (char)toupper(*string);
		string++;
	}

	return hold;
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int SetSockBlocking(SOCKET sock, int isblocking)
{
	int rcode;

#if defined(_REVOLUTION)
	int val;
	
	val = SOFcntl(sock, SO_F_GETFL, 0);
	
	if(isblocking)
		val &= ~SO_O_NONBLOCK;
	else
		val |= SO_O_NONBLOCK;
	
	rcode = SOFcntl(sock, SO_F_SETFL, val);
#elif defined(_NITRO)
	int val;
	
	val = SOC_Fcntl(sock, SOC_F_GETFL, 0);
	
	if(isblocking)
		val &= ~SOC_O_NONBLOCK;
	else
		val |= SOC_O_NONBLOCK;
	
	rcode = SOC_Fcntl(sock, SOC_F_SETFL, val);
#else
	#if defined(_PS2) || defined(_PS3)
		// EENet requires int
		// SNSystems requires int
		// Insock requires int
		// PS3 requires int
		gsi_i32 argp;
	#else
		unsigned long argp;
	#endif
		
		if(isblocking)
			argp = 0;
		else
			argp = 1;

	#ifdef _PS2
		#ifdef SN_SYSTEMS
			rcode = setsockopt(sock, SOL_SOCKET, (isblocking) ? SO_BIO : SO_NBIO, &argp, sizeof(argp));
		#endif

		#ifdef EENET
			rcode = setsockopt(sock, SOL_SOCKET, SO_NBIO, &argp, sizeof(argp));
		#endif

		#ifdef INSOCK
			if (isblocking)
				argp = -1;
			else
				argp = 5; //added longer timeout to 5ms
			sceInsockSetRecvTimeout(sock, argp);
			sceInsockSetSendTimeout(sock, argp);
			sceInsockSetShutdownTimeout(sock, argp);
			GSI_UNUSED(sock);
			rcode = 0;
		#endif
	#elif defined(_PSP)
		rcode = setsockopt(sock, SCE_NET_INET_SOL_SOCKET, SCE_NET_INET_SO_NBIO, &argp, sizeof(argp));
	#elif defined(_PS3)
		rcode = setsockopt(sock, SOL_SOCKET, SO_NBIO, &argp, sizeof(argp));
	#else
		rcode = ioctlsocket(sock, FIONBIO, &argp);
	#endif
#endif

	if(rcode == 0)
	{
		gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment,
			"SetSockBlocking: Set socket %d to %s\r\n", (unsigned int)sock, isblocking ? "blocking":"non-blocking");
		return 1;
	}

	gsDebugFormat(GSIDebugCat_Common, GSIDebugType_Network, GSIDebugLevel_Comment,
			"SetSockBlocking failed: tried to set socket %d to %s\r\n", (unsigned int)sock, isblocking ? "blocking":"non-blocking");
	return 0;
}

#ifndef INSOCK
	int SetReceiveBufferSize(SOCKET sock, int size)
	{
		int rcode;
		rcode = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char *)&size, sizeof(int));
		return gsiSocketIsNotError(rcode);
	}

	int SetSendBufferSize(SOCKET sock, int size)
	{
		int rcode;
		rcode = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char *)&size, sizeof(int));
		return gsiSocketIsNotError(rcode);
	}

	int GetReceiveBufferSize(SOCKET sock)
	{
		int rcode;
		int size;
		int len;

		len = sizeof(size);

		rcode = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *)&size, &len);

		if(gsiSocketIsError(rcode))
			return -1;

		return size;
	}

	int GetSendBufferSize(SOCKET sock)
	{
		int rcode;
		int size;
		int len;

		len = sizeof(size);

		rcode = getsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char *)&size, &len);

		if(gsiSocketIsError(rcode))
			return -1;

		return size;
	}
	
	// Formerly known as ghiSocketSelect
#ifdef SN_SYSTEMS
	#undef FD_SET
	#define FD_SET(s,p)   ((p)->array[((s) - 1) >> SN_FD_SHR] |= \
                       (unsigned int)(1 << (((s) - 1) & SN_FD_BITS)) )

#endif
#endif

int GSISocketSelect(SOCKET theSocket, int* theReadFlag, int* theWriteFlag, int* theExceptFlag)
{
	SOPollFD pollFD;
	int rcode;
	
	pollFD.fd = theSocket;
	pollFD.events = 0;
	if(theReadFlag != NULL)
		pollFD.events |= SOC_POLLRDNORM;
	if(theWriteFlag != NULL)
		pollFD.events |= SOC_POLLWRNORM;
	pollFD.revents = 0;

	rcode = SOC_Poll(&pollFD, 1, 0);
	if(rcode < 0)
		return NITRO_SOCKET_ERROR;

	if(theReadFlag != NULL)
	{
		if((rcode > 0) && (pollFD.revents & (SOC_POLLRDNORM|SOC_POLLHUP)))
			*theReadFlag = 1;
		else
			*theReadFlag = 0;
	}
	if(theWriteFlag != NULL)
	{
		if((rcode > 0) && (pollFD.revents & SOC_POLLWRNORM))
			*theWriteFlag = 1;
		else
			*theWriteFlag = 0;
	}
	if(theExceptFlag != NULL)
	{
		if((rcode > 0) && (pollFD.revents & SOC_POLLERR))
			*theExceptFlag = 1;
		else
			*theExceptFlag = 0;
	}
	return rcode;
}

// Return 1 for immediate recv, otherwise 0
int CanReceiveOnSocket(SOCKET sock)
{
	int aReadFlag = 0;
	if (1 == GSISocketSelect(sock, &aReadFlag, NULL, NULL))
		return aReadFlag;

	// SDKs expect 0 on SOCKET_ERROR
	return 0;
}

// Return 1 for immediate send, otherwise 0
int CanSendOnSocket(SOCKET sock)
{
	int aWriteFlag = 0;
	if (1 == GSISocketSelect(sock, NULL, &aWriteFlag, NULL))
		return aWriteFlag;

	// SDKs expect 0 on SOCKET_ERROR
	return 0;
}

#if defined(_NITRO)
    static char *aliases = NULL;
#endif

#if defined(_PS3) || defined (_PSP)

#else

HOSTENT * getlocalhost(void)
{
#ifdef EENET
	#define MAX_IPS  5

	static HOSTENT localhost;
	static char * aliases = NULL;
	static char * ipPtrs[MAX_IPS + 1];
	static unsigned int ips[MAX_IPS];

	struct sceEENetIfname * interfaces;
	struct sceEENetIfname * interface;
	int num;
	int i;
	int count;
	int len;
	u_short flags;
	IN_ADDR address;

	// initialize the host
	localhost.h_name = "localhost";
	localhost.h_aliases = &aliases;
	localhost.h_addrtype = AF_INET;
	localhost.h_length = 0;
	localhost.h_addr_list = ipPtrs;

	// get the local interfaces
	sceEENetGetIfnames(NULL, &num);
	interfaces = (struct sceEENetIfname *)gsimalloc(num * sizeof(struct sceEENetIfname));
	if(!interfaces)
		return NULL;
	sceEENetGetIfnames(interfaces, &num);

	// loop through the interfaces
	count = 0;
	for(i = 0 ; i < num ; i++)
	{
		// the next interface
		interface = &interfaces[i];
		//printf("eenet%d: %s\n", i, interface->ifn_name);

		// get the flags
		len = sizeof(flags);
		if(sceEENetGetIfinfo(interface->ifn_name, sceEENET_IFINFO_IFFLAGS, &flags, &len) != 0)
			continue;
		//printf("eenet%d flags: 0x%X\n", i, flags);

		// check for up, running, and non-loopback
		if(!(flags & (IFF_UP|IFF_RUNNING)) || (flags & IFF_LOOPBACK))
			continue;
		//printf("eenet%d: up and running, non-loopback\n", i);

		// get the address
		len = sizeof(address);
		if(sceEENetGetIfinfo(interface->ifn_name, sceEENET_IFINFO_ADDR, &address, &len) != 0)
			continue;
		//printf("eenet%d: %s\n", i, inet_ntoa(address));

		// add this address
		ips[count] = address.s_addr;
		ipPtrs[count] = (char *)&ips[count];
		count++;
	}

	// free the interfaces
	gsifree(interfaces);

	// check that we got at least one IP
	if(!count)
		return NULL;

	// finish filling in the host struct
	localhost.h_length = (gsi_u16)sizeof(ips[0]);
	ipPtrs[count] = NULL;

	return &localhost;

	////////////////////
	// INSOCK
#elif defined(INSOCK)
	// Global storage
	#define MAX_IPS  sceLIBNET_MAX_INTERFACE
	static HOSTENT   localhost;
	static char    * aliases = NULL;
	static char    * ipPtrs[MAX_IPS + 1];
	static unsigned int ips[MAX_IPS];

	// Temp storage
	int aInterfaceIdArray[MAX_IPS];
	int aNumInterfaces = 0;
	int aInterfaceNum = 0;
	int aCount = 0;
	
	// Get the list of interfaces
	aNumInterfaces = sceInetGetInterfaceList(&gGSIInsockClientData, 
		                 &gGSIInsockSocketBuffer, aInterfaceIdArray, MAX_IPS);
	if (aNumInterfaces < 1)
		return NULL;

	// initialize the HOSTENT
	localhost.h_name      = "localhost";
	localhost.h_aliases   = &aliases;
	localhost.h_addrtype  = AF_INET;
	localhost.h_addr_list = ipPtrs;

	// Look up each address and copy into the HOSTENT structure
	aCount = 0; // count of valid interfaces
	for (aInterfaceNum = 0; aInterfaceNum < aNumInterfaces; aInterfaceNum++)
	{
		sceInetAddress_t anAddr;
		int result = sceInetInterfaceControl(&gGSIInsockClientData, &gGSIInsockSocketBuffer,
			                    aInterfaceIdArray[aInterfaceNum], sceInetCC_GetAddress,
								&anAddr, sizeof(anAddr));
		if (result == 0)
		{
			// Add this interface to the array
			memcpy(&ips[aCount], anAddr.data, sizeof(ips[aCount]));
			ips[aCount] = htonl(ips[aCount]);
			ipPtrs[aCount] = (char*)&ips[aCount];
			aCount++;
		}
	}

	// Set the final hostent data, then return
	localhost.h_length = (gsi_u16)sizeof(ips[0]);
	ipPtrs[aCount]     = NULL;
	return &localhost;
	
#elif defined(_NITRO)
	#define MAX_IPS  5

	static HOSTENT localhost;
	static char * ipPtrs[MAX_IPS + 1];
	static unsigned int ips[MAX_IPS];

	int count = 0;

	localhost.h_name = "localhost";
	localhost.h_aliases = &aliases;
	localhost.h_addrtype = AF_INET;
	localhost.h_length = 0;
	localhost.h_addr_list = (u8 **)ipPtrs;

	ips[count] = 0;
	IP_GetAddr(NULL, (u8*)&ips[count]);
	if(ips[count] == 0)
		return NULL;
	ipPtrs[count] = (char *)&ips[count];
	count++;

	localhost.h_length = (gsi_u16)sizeof(ips[0]);
	ipPtrs[count] = NULL;

	return &localhost;

#elif defined(_REVOLUTION)
	#define MAX_IPS  5
	static HOSTENT aLocalHost;
	static char * aliases = NULL;
	int aNumOfIps, i;
	int aSizeNumOfIps;
	static IPAddrEntry aAddrs[MAX_IPS];
	int aAddrsSize, aAddrsSizeInitial;
	static u8 * ipPtrs[MAX_IPS + 1];
	static unsigned int ips[MAX_IPS];
	int ret;
	aSizeNumOfIps = sizeof(aNumOfIps);
	ret = SOGetInterfaceOpt(NULL, SO_SOL_CONFIG, SO_CONFIG_IP_ADDR_NUMBER, &aNumOfIps, &aSizeNumOfIps);
	if (ret != 0)
		return NULL;
	
	aAddrsSize = (int)(MAX_IPS * sizeof(IPAddrEntry));
	aAddrsSizeInitial = aAddrsSize;
	ret = SOGetInterfaceOpt(NULL, SO_SOL_CONFIG, SO_CONFIG_IP_ADDR_TABLE, &aAddrs, &aAddrsSize);
	if (ret != 0)
		return NULL;
	
	if (aAddrsSize != aAddrsSizeInitial)
	{
		aNumOfIps = aAddrsSize / (int)sizeof(IPAddrEntry);
	}
	
	aLocalHost.h_name = "localhost";
	aLocalHost.h_aliases = &aliases;
	aLocalHost.h_addrtype = AF_INET;
	aLocalHost.h_length = SO_IP4_ALEN;

	for (i = 0; i < MAX_IPS; i++)
	{
		if (i < aNumOfIps)
		{
			memcpy(&ips[i], &aAddrs[i].addr, sizeof(aAddrs[i].addr));
			ipPtrs[i] = (u8 *)&ips[i];
		}			
		else 
			ipPtrs[i] = NULL;
	}
	aLocalHost.h_addr_list = ipPtrs;
	
	return &aLocalHost;

#elif defined(_X360)
	XNADDR addr;
	DWORD rcode;
	static HOSTENT localhost;
	static char * ipPtrs[2];
	static IN_ADDR ip;

	while((rcode = XNetGetTitleXnAddr(&addr)) == XNET_GET_XNADDR_PENDING)
		msleep(1);

	if((rcode == XNET_GET_XNADDR_NONE) || (rcode == XNET_GET_XNADDR_TROUBLESHOOT))
		return NULL;

	localhost.h_name = "localhost";
	localhost.h_aliases = NULL;
	localhost.h_addrtype = AF_INET;
	localhost.h_length = (gsi_u16)sizeof(IN_ADDR);
	localhost.h_addr_list = (gsi_i8 **)ipPtrs;

	ip = addr.ina;
	ipPtrs[0] = (char *)&ip;
	ipPtrs[1] = NULL;

	return &localhost;

#elif defined(_XBOX)
	return NULL;


#else
	char hostname[256] = "";

	// get the local host's name
	gethostname(hostname, sizeof(hostname));

	// return the host for that name
	return gethostbyname(hostname);
#endif
}
#endif

int IsPrivateIP(IN_ADDR * addr)
{
	int b1;
	int b2;
	unsigned int ip;

	// get the first 2 bytes
	ip = ntohl(addr->s_addr);
	b1 = (int)((ip >> 24) & 0xFF);
	b2 = (int)((ip >> 16) & 0xFF);

	// 10.X.X.X
	if(b1 == 10)
		return 1;

	// 172.16-31.X.X
	if((b1 == 172) && ((b2 >= 16) && (b2 <= 31)))
		return 1;

	// 192.168.X.X
	if((b1 == 192) && (b2 == 168))
		return 1;

	return 0;
}

static int GSINitroErrno;

static int CheckRcode(int rcode, int errCode)
{
	if(rcode >= 0)
		return rcode;
	GSINitroErrno = rcode;
	return errCode;
}

int socket(int pf, int type, int protocol)
{
	int rcode = SOC_Socket(pf, type, protocol);
	return CheckRcode(rcode, INVALID_SOCKET);
}
int closesocket(SOCKET sock)
{
	int rcode = SOC_Close(sock);
	return CheckRcode(rcode, NITRO_SOCKET_ERROR);
}
int shutdown(SOCKET sock, int how)
{
	int rcode = SOC_Shutdown(sock, how);
	return CheckRcode(rcode, NITRO_SOCKET_ERROR);
}
int bind(SOCKET sock, const SOCKADDR* addr, int len)
{
	SOCKADDR localAddr;
	int rcode;

	// with nitro, don't bind to 0, just start using the port
	if(((const SOCKADDR_IN*)addr)->port == 0)
		return 0;

	memcpy(&localAddr, addr, sizeof(SOCKADDR));
	localAddr.len = (u8)len;

	rcode = SOC_Bind(sock, &localAddr);
	return CheckRcode(rcode, NITRO_SOCKET_ERROR);
}

int connect(SOCKET sock, const SOCKADDR* addr, int len)
{
	SOCKADDR remoteAddr;
	int rcode;

	memcpy(&remoteAddr, addr, sizeof(SOCKADDR));
	remoteAddr.len = (u8)len;

	rcode = SOC_Connect(sock, &remoteAddr);
	return CheckRcode(rcode, NITRO_SOCKET_ERROR);
}
int listen(SOCKET sock, int backlog)
{
	int rcode = SOC_Listen(sock, backlog);
	return CheckRcode(rcode, NITRO_SOCKET_ERROR);
}
SOCKET accept(SOCKET sock, SOCKADDR* addr, int* len)
{
	int rcode;
	addr->len = (u8)*len;
	rcode = SOC_Accept(sock, addr);
	*len = addr->len;
	return CheckRcode(rcode, NITRO_SOCKET_ERROR);
}

int recv(SOCKET sock, char* buf, int len, int flags)
{
	int rcode = SOC_Recv(sock, buf, len, flags);
	return CheckRcode(rcode, NITRO_SOCKET_ERROR);
}
int recvfrom(SOCKET sock, char* buf, int len, int flags, SOCKADDR* addr, int* fromlen)
{
	int rcode;
	addr->len = (u8)*fromlen;
	rcode = SOC_RecvFrom(sock, buf, len, flags, addr);
	*fromlen = addr->len;
	return CheckRcode(rcode, NITRO_SOCKET_ERROR);
}
SOCKET send(SOCKET sock, const char* buf, int len, int flags)
{
	int rcode = SOC_Send(sock, buf, len, flags);
	return CheckRcode(rcode, NITRO_SOCKET_ERROR);
}
SOCKET sendto(SOCKET sock, const char* buf, int len, int flags, const SOCKADDR* addr, int tolen)
{
	SOCKADDR remoteAddr;
	int rcode;

	memcpy(&remoteAddr, addr, sizeof(SOCKADDR));
	remoteAddr.len = (u8)tolen;

	rcode = SOC_SendTo(sock, buf, len, flags, &remoteAddr);
	return CheckRcode(rcode, NITRO_SOCKET_ERROR);
}

int getsockopt(SOCKET sock, int level, int optname, char* optval, int* optlen)
{
	int rcode = SOC_GetSockOpt(sock, level, optname, optval, optlen);
	return CheckRcode(rcode, NITRO_SOCKET_ERROR);
}
SOCKET setsockopt(SOCKET sock, int level, int optname, const char* optval, int optlen)
{
	int rcode = SOC_SetSockOpt(sock, level, optname, optval, optlen);
	return CheckRcode(rcode, NITRO_SOCKET_ERROR);
}

int getsockname(SOCKET sock, SOCKADDR* addr, int* len)
{
	int rcode;
	addr->len = (u8)*len;
	rcode = SOC_GetSockName(sock, addr);
	*len = addr->len;
	return CheckRcode(rcode, NITRO_SOCKET_ERROR);
}

unsigned long inet_addr(const char* name)
{
	int rcode;
	SOInAddr addr;
	rcode = SOC_InetAtoN(name, &addr);
	if(rcode == FALSE)
		return INADDR_NONE;
	return addr.addr;
}

int GOAGetLastError(SOCKET sock)
{
	GSI_UNUSED(sock);
	return GSINitroErrno;
}

// note that this doesn't return the standard time() value
// because the DS doesn't know what timezone it's in
time_t time(time_t *timer)
{
	time_t t;

	assertWithLine(OS_IsTickAvailable() == TRUE, 1639);
	t = (time_t)OS_TicksToSeconds(OS_GetTick());

	if(timer)
		*timer = t;

	return t;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Cross platform random number generator
#define RANa 16807                 // multiplier
#define LONGRAND_MAX 2147483647L   // 2**31 - 1

static long randomnum = 1;

static long nextlongrand(long seed)
{
	unsigned

	long lo, hi;
	lo = RANa *(unsigned long)(seed & 0xFFFF);
	hi = RANa *((unsigned long)seed >> 16);
	lo += (hi & 0x7FFF) << 16;

	if (lo > LONGRAND_MAX)
	{
		lo &= LONGRAND_MAX;
		++lo;
	}
	lo += hi >> 15;

	if (lo > LONGRAND_MAX)
	{
		lo &= LONGRAND_MAX;
		++lo;
	}

	return(long)lo;
}

// return next random long
static long longrand(void)
{
	randomnum = nextlongrand(randomnum);
	return randomnum;
}

// to seed it
void Util_RandSeed(unsigned long seed)
{
	// nonzero seed
	randomnum = seed ? (long)(seed & LONGRAND_MAX) : 1;
}

int Util_RandInt(int low, int high)
{
	int range = high-low;
	int num;
	
	if (range == 0)
		return (low); // Prevent divide by zero

	num = (int)(longrand() % range);

	return(num + low);
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*****************************
UNICODE ENCODING
******************************/

static void QuartToTrip(char *quart, char *trip, int inlen)
{
	if (inlen >= 2)
		trip[0] = (char)(quart[0] << 2 | quart[1] >> 4);
	if (inlen >= 3)
		trip[1] = (char)((quart[1] & 0x0F) << 4 | quart[2] >> 2);
	if (inlen >= 4)
		trip[2] = (char)((quart[2] & 0x3) << 6 | quart[3]);
}

static void TripToQuart(const char *trip, char *quart, int inlen)
{
	unsigned char triptemp[3];
	int i;
	for (i = 0; i < inlen ; i++)
	{
		triptemp[i] = (unsigned char)trip[i];
	}
	while (i < 3) //fill the rest with 0
	{
		triptemp[i] = 0;
		i++;
	}
	quart[0] = (char)(triptemp[0] >> 2);
	quart[1] = (char)(((triptemp[0] & 3) << 4) | (triptemp[1] >> 4));
	quart[2] = (char)((triptemp[1] & 0x0F) << 2 | (triptemp[2] >> 6));
	quart[3] = (char)(triptemp[2] & 0x3F);

}

const char alternateEncoding[] = {'[',']','_'};
const char urlSafeEncodeing[] = {'-','_','='};
const char defaultEncoding[] = {'+','/','='};

void B64Decode(char *input, char *output, int * len, int encodingType)
{
	const char *encoding = NULL;
	const char *holdin = input;
	int readpos = 0;
	int writepos = 0;
	char block[4];
	
	int outlen = -1;
	int inlen = (int)strlen(input);

	// 10-31-2004 : Added by Saad Nader
	// now supports URL safe encoding
	////////////////////////////////////////////////
	switch(encodingType)
	{	
		case 1: 
			encoding = alternateEncoding;
			break;
		case 2:
			encoding = urlSafeEncodeing;
			break;
		default: encoding = defaultEncoding;
	}

	//GS_ASSERT(inlen >= 0);
	if (inlen <= 0)
	{
		if (outlen)
			*len = 0;
		output[0] = '\0';
		return;
	}

	// Break at end of string or padding character
	while (readpos < inlen && input[readpos] != encoding[2])
	{
		//    'A'-'Z' maps to 0-25
		//    'a'-'z' maps to 26-51
		//    '0'-'9' maps to 52-61
		//    62 maps to encoding[0]
		//    63 maps to encoding[1]
		if (input[readpos] >= '0' && input[readpos] <= '9')
			block[readpos%4] = (char)(input[readpos] - 48 + 52);
		else if (input[readpos] >= 'a' && input[readpos] <= 'z')
			block[readpos%4] = (char)(input[readpos] - 71);
		else if (input[readpos] >= 'A' && input[readpos] <= 'Z')
			block[readpos%4] = (char)(input[readpos] - 65);
		else if (input[readpos] == encoding[0])
			block[readpos%4] = 62;
		else if (input[readpos] == encoding[1])
			block[readpos%4] = 63;

		// padding or '\0' characters also mark end of input
		else if (input[readpos] == encoding[2])
			break;
		else if (input[readpos] == '\0')
			break;
		else 
		{
			//	(assert(0)); //bad input data
			if (outlen)
				*len = 0;
			output[0] = '\0';
			return; //invaid data
		}

		// every 4 bytes, convert QuartToTrip into destination
		if (readpos%4==3) // zero based, so (3%4) means four bytes, 0-1-2-3
		{
			QuartToTrip(block, &output[writepos], 4);
			writepos += 3;
		}
		readpos++;
	}

	// Convert any leftover characters in block
	if ((readpos != 0) && (readpos%4 != 0))
	{
		// fill block with pad (required for QuartToTrip)
		memset(&block[readpos%4], encoding[2], (unsigned int)4-(readpos%4)); 
		QuartToTrip(block, &output[writepos], readpos%4);

		// output bytes depend on the number of non-pad input bytes
		if (readpos%4 == 3)
			writepos += 2;
		else 
			writepos += 1;
	}

	if (outlen)
		*len = writepos;

	GSI_UNUSED(holdin);
}



void B64Encode(const char *input, char *output, int inlen, int encodingType)
{
	const char *encoding;
	char *holdout = output;
	char *lastchar;
	int todo = inlen;
	
	// 10-31-2004 : Added by Saad Nader
	// now supports URL safe encoding
	////////////////////////////////////////////////
	switch(encodingType)
	{	
		case 1: 
			encoding = alternateEncoding;
			break;
		case 2:
			encoding = urlSafeEncodeing;
			break;
		default: encoding = defaultEncoding;
	}
	
//assume interval of 3
	while (todo > 0)
	{
		TripToQuart(input, output, min(todo, 3));
		output += 4;
		input += 3;
		todo -= 3;
	}
	lastchar = output;
	if (inlen % 3 == 1)
		lastchar -= 2;
	else if (inlen % 3 == 2)
		lastchar -= 1;
	*output = 0; //null terminate!
	while (output > holdout)
	{
		output--;
		if (output >= lastchar) //pad the end
			*output = encoding[2];
		else if (*output <= 25)
			*output = (char)(*output + 65);
		else if (*output <= 51)
			*output = (char)(*output + 71);
		else if (*output <= 61)
			*output = (char)(*output + 48 - 52);
		else if (*output == 62)
			*output = encoding[0];
		else if (*output == 63)
			*output = encoding[1];
	} 
}
