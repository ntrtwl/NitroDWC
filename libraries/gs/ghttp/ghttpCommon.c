/*
GameSpy GHTTP SDK 
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 1999-2007 GameSpy Industries, Inc

devsupport@gamespy.com
*/

#include "gs/ghttp/ghttpCommon.h"

// Disable compiler warnings for issues that are unavoidable.
/////////////////////////////////////////////////////////////
#if defined(_MSC_VER) // DevStudio
// Level4, "conditional expression is constant". 
// Occurs with use of the MS provided macro FD_SET
#pragma warning ( disable: 4127 )
#endif // _MSC_VER

#ifdef WIN32
// A lock.
//////////
typedef void * GLock;

// The lock used by ghttp.
//////////////////////////
static GLock ghiGlobalLock;
#endif

// Proxy server.
////////////////
char * ghiProxyAddress;
unsigned short ghiProxyPort;

// Throttle settings.
/////////////////////
int ghiThrottleBufferSize = 125;
gsi_time ghiThrottleTimeDelay = 250;

// Number of connections
/////////////////////
extern int ghiNumConnections;


#ifdef WIN32
// Creates a lock.
//////////////////
static GLock GNewLock(void)
{
	CRITICAL_SECTION * criticalSection;

	criticalSection = (CRITICAL_SECTION *)gsimalloc(sizeof(CRITICAL_SECTION));
	if(!criticalSection)
		return NULL;

	InitializeCriticalSection(criticalSection);

	return (GLock)criticalSection;
}

// Frees a lock.
////////////////
static void GFreeLock(GLock lock)
{
	CRITICAL_SECTION * criticalSection = (CRITICAL_SECTION *)lock;

	if(!lock)
		return;

	DeleteCriticalSection(criticalSection);

	gsifree(criticalSection);
}

// Locks a lock.
////////////////
static void GLockLock(GLock lock)
{
	CRITICAL_SECTION * criticalSection = (CRITICAL_SECTION *)lock;

	if(!lock)
		return;

	EnterCriticalSection(criticalSection);
}

// Unlocks a lock.
//////////////////
static void GUnlockLock(GLock lock)
{
	CRITICAL_SECTION * criticalSection = (CRITICAL_SECTION *)lock;

	if(!lock)
		return;

	LeaveCriticalSection(criticalSection);
}
#endif

// Creates the ghttp lock.
//////////////////////////
void ghiCreateLock(void)
{
#ifdef WIN32
	// We shouldn't already have a lock.
	////////////////////////////////////
	assertWithLine(!ghiGlobalLock, 108);

	// Create the lock.
	///////////////////
	ghiGlobalLock = GNewLock();
#endif
}

// Frees the ghttp lock.
////////////////////////
void ghiFreeLock(void)
{
#ifdef WIN32
	if(!ghiGlobalLock)
		return;

	GFreeLock(ghiGlobalLock);
	ghiGlobalLock = NULL;
#endif
}

// Locks the ghttp lock.
////////////////////////
void ghiLock
(
	void
)
{
#ifdef WIN32
	if(!ghiGlobalLock)
		return;

	GLockLock(ghiGlobalLock);
#endif
}

// Unlocks the ghttp lock.
//////////////////////////
void ghiUnlock
(
	void
)
{
#ifdef WIN32
	if(!ghiGlobalLock)
		return;

	GUnlockLock(ghiGlobalLock);
#endif
}

// Logs traffic.
////////////////
#ifdef HTTP_LOG
void ghiLogToFile(const char * buffer, int len, const char* fileName)
{
#ifdef _NITRO
	int i;

	if(!buffer || !len)
		return;

	for(i = 0 ; i < len ; i++)
		OS_PutChar(buffer[i]);
#else
	FILE * file;

	if(!buffer || !len)
		return;

	file = fopen(fileName, "ab");
	if(file)
	{
		fwrite(buffer, 1, len, file);
		fclose(file);
	}
#endif
}
#endif

// Reads encrypted data from decodeBuffer
// Appends decrypted data to recvBuffer
// Returns GHTTPFalse if there was a fatal error
////////////////////////////////////////////////
GHTTPBool ghiDecryptReceivedData(struct GHIConnection * connection)
{
	// Decrypt data from decodeBuffer to recvBuffer
	GHIEncryptionResult aResult = GHIEncryptionResult_None;

	// data to be decrypted
	char* aReadPos  = NULL;
	char* aWritePos = NULL;
	int   aReadLen  = 0;
	int   aWriteLen = 0;

	// Call the decryption func
	do 
	{
		aReadPos  = connection->decodeBuffer.data + connection->decodeBuffer.pos;
		aReadLen  = connection->decodeBuffer.len  - connection->decodeBuffer.pos; 
		aWritePos = connection->recvBuffer.data + connection->recvBuffer.len;
		aWriteLen = connection->recvBuffer.size - connection->recvBuffer.len;    // the amount of room in recvbuffer

		aResult = (connection->encryptor.mDecryptFunc)(connection, &connection->encryptor, 
			aReadPos, &aReadLen, aWritePos, &aWriteLen);
		if (aResult == GHIEncryptionResult_BufferTooSmall)
		{
			// Make some more room
			if (GHTTPFalse == ghiResizeBuffer(&connection->recvBuffer, connection->recvBuffer.sizeIncrement))
				return GHTTPFalse; // error
		}
	} while (aResult == GHIEncryptionResult_BufferTooSmall && aWriteLen == 0);

	connection->decodeBuffer.pos += aReadLen;
	connection->recvBuffer.len   += aWriteLen;

	// Discard data from the decodedBuffer in chunks
	if (connection->decodeBuffer.pos > 0xFF)
	{
		int bytesToKeep = connection->decodeBuffer.len - connection->decodeBuffer.pos;
		if (bytesToKeep == 0)
			ghiResetBuffer(&connection->decodeBuffer);
		else
		{
			memmove(connection->decodeBuffer.data,
					connection->decodeBuffer.data + connection->decodeBuffer.pos,
					(size_t)bytesToKeep);
			connection->decodeBuffer.pos = 0;
			connection->decodeBuffer.len = bytesToKeep;
		}
	}

	if (aResult == GHIEncryptionResult_Error)
	{
		connection->completed = GHTTPTrue;
		connection->result = GHTTPEncryptionError;
		return GHTTPFalse;
	}

	return GHTTPTrue; 
}

// Receive some data.
/////////////////////
GHIRecvResult ghiDoReceive
(
	GHIConnection * connection,
	char buffer[],
	int * bufferLen
)
{
	int rcode;
	int socketError;
	int len;

	// How much to try and receive.
	///////////////////////////////
	len = (*bufferLen - 1);

	// Are we throttled?
	////////////////////
	if(connection->throttle)
	{
		unsigned long now;

		// Don't receive too often.
		///////////////////////////
		now = current_time();
		if(now < (connection->lastThrottleRecv + ghiThrottleTimeDelay))
			return GHINoData;

		// Update the receive time.
		///////////////////////////
		connection->lastThrottleRecv = (unsigned int)now;

		// Don't receive too much.
		//////////////////////////
		len = min(len, ghiThrottleBufferSize);
	}

	// Receive some data.
	/////////////////////
	if (connection->recvBuffer.pos < connection->recvBuffer.len)
	{
        ghiReadDataFromBuffer(&connection->recvBuffer, buffer, bufferLen);
        if (connection->recvBuffer.pos == connection->recvBuffer.len)
		{
            connection->recvBuffer.len = connection->tempDataIndex;
            connection->recvBuffer.pos = connection->tempDataIndex;
        }
        return GHIRecvData;
    }

	rcode = recv(connection->socket, buffer, len, 0);

	// There was an error.
	//////////////////////
	if(gsiSocketIsError(rcode))
	{
		// Get the error code.
		//////////////////////
		socketError = GOAGetLastError(connection->socket);

		// Check for nothing waiting.
		/////////////////////////////
		if((socketError == WSAEWOULDBLOCK) || (socketError == WSAEINPROGRESS) || (socketError == WSAETIMEDOUT))
			return GHINoData;

		// There was a real error.
		//////////////////////////
		connection->completed = GHTTPTrue;
		connection->result = GHTTPSocketFailed;
		connection->socketError = socketError;
		connection->connectionClosed = GHTTPTrue;

		return GHIError;
	}

	// The connection was closed.
	/////////////////////////////
	if(rcode == 0)
	{
		connection->connectionClosed = GHTTPTrue;
		return GHIConnClosed;
	}

	if (connection->encryptor.mEngine != GHTTPEncryptionEngine_None)
	{
		if (!ghiAppendDataToBuffer(&connection->decodeBuffer, buffer, rcode))
			return GHIError;
		if (!ghiDecryptReceivedData(connection))
		{
			connection->completed = GHTTPTrue;
			connection->result = GHTTPEncryptionError;
			return GHIError;
		}
		if (connection->recvBuffer.len - connection->recvBuffer.pos <= 0)
		{
			buffer[0] = '\0';
			*bufferLen = 0;
			return GHINoData;
		}
		rcode = *bufferLen - 1;
		if (!ghiReadDataFromBuffer(&connection->recvBuffer, buffer, &rcode))
			return GHIError;
		if (connection->recvBuffer.pos == connection->recvBuffer.len)
		{
			connection->recvBuffer.len = connection->tempDataIndex;
			connection->recvBuffer.pos = connection->tempDataIndex;
		}
		if (rcode <= 0)
			return GHINoData;
	}

	// Cap the buffer.
	//////////////////
	buffer[rcode] = '\0';
	*bufferLen = rcode;

	gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_Network, GSIDebugLevel_RawDump, "Received %d bytes\n", rcode);

	if (rcode <= 0)
		return GHINoData;
	// Notify app.
	//////////////
	return GHIRecvData;
}

int ghiDoSend
(
	struct GHIConnection * connection,
	const char * buffer,
	int len
)
{
	int rcode;

	// send directly to socket
	rcode = send(connection->socket, buffer, len, 0);

	// Check for an error.
	//////////////////////
	if(gsiSocketIsError(rcode))
	{
		int error;

		// Would block just means 0 bytes sent.
		///////////////////////////////////////
		error = GOAGetLastError(connection->socket);
		if((error == WSAEWOULDBLOCK) || (error == WSAEINPROGRESS) || (error == WSAETIMEDOUT))
			return 0;

		connection->completed = GHTTPTrue;
		connection->result = GHTTPSocketFailed;
		connection->socketError = error;

		return -1;
	}

	//do not add CRLF as part of bytes posted - make sure waitPostContinue is false
	if(connection->state == GHTTPPosting)
	{
		connection->postingState.bytesPosted += rcode;
		ghiLog(buffer, rcode);
	}

	return rcode;
}

GHITrySendResult ghiTrySendThenBuffer
(
	GHIConnection * connection,
	const char * buffer,
	int len
)
{
	int rcode = 0;

	// If we already have something buffered, don't send.
	/////////////////////////////////////////////////////
	if(connection->sendBuffer.len == 0)
	{
		// Try and send.
		////////////////
		rcode = ghiDoSend(connection, buffer, len);
		if(gsiSocketIsError(rcode))
			return GHITrySendError;

		// Was it all sent?
		///////////////////
		if(rcode == len)
			return GHITrySendSent;
	}
	
	// Buffer whatever wasn't sent.
	///////////////////////////////
	if(!ghiAppendDataToBuffer(&connection->sendBuffer, buffer + rcode, len - rcode))
		return GHITrySendError;
	return GHITrySendBuffered;
}

// Re-enable previously disabled compiler warnings
///////////////////////////////////////////////////
#if defined(_MSC_VER)
#pragma warning ( default: 4127 )
#endif // _MSC_VER

