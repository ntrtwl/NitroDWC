 /*
GameSpy GHTTP SDK 
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 1999-2007 GameSpy Industries, Inc

devsupport@gamespy.com
*/

#include "gs/ghttp/ghttpConnection.h"
#include "gs/ghttp/ghttpCommon.h"

// Initial size and increment amount for the connections array.
///////////////////////////////////////////////////////////////
#define CONNECTIONS_CHUNK_LEN      4

// An array of pointers to GHIConnection objects.
// A GHTTPRequest is an index into this array.
/////////////////////////////////////////////////
static GHIConnection ** ghiConnections;
static int ghiConnectionsLen;
static int ghiNumConnections;
static int ghiNextUniqueID;

// Finds a gsifree slot in the ghiConnections array.
// If there are no gsifree slots, the array size will be increased.
////////////////////////////////////////////////////////////////
static int ghiFindFreeSlot
(
	void
)
{
	int i;
	GHIConnection ** tempPtr;
	int oldLen;
	int newLen;

	// Look for an open slot.
	/////////////////////////
	for(i = 0 ; i < ghiConnectionsLen ; i++)
	{
		if(!ghiConnections[i]->inUse)
			return i;
	}

	assertWithLine(ghiNumConnections == ghiConnectionsLen, 51);

	// Nothing found, resize the array.
	///////////////////////////////////
	oldLen = ghiConnectionsLen;
	newLen = (ghiConnectionsLen + CONNECTIONS_CHUNK_LEN);
	tempPtr = (GHIConnection **)gsirealloc(ghiConnections, sizeof(GHIConnection *) * newLen);
	if(!tempPtr)
		return -1;
	ghiConnections = tempPtr;

	// Create the new connection objects.
	/////////////////////////////////////
	for(i = oldLen ; i < newLen ; i++)
	{
		ghiConnections[i] = (GHIConnection *)gsimalloc(sizeof(GHIConnection));
		if(!ghiConnections[i])
		{
			for(i-- ; i >= oldLen ; i--)
				gsifree(ghiConnections[i]);
			return -1;
		}
		ghiConnections[i]->inUse = GHTTPFalse;
	}

	// Update the length.
	/////////////////////
	ghiConnectionsLen = newLen;

	return oldLen;
}

GHIConnection * ghiNewConnection
(
	void
)
{
	int slot;
	GHIConnection * connection;
	GHTTPBool bResult;

	ghiLock();

	// Find a gsifree slot.
	////////////////////
	slot = ghiFindFreeSlot();
	if(slot == -1)
	{
		ghiUnlock();
		return NULL;
	}

	// Get a pointer to the object.
	///////////////////////////////
	connection = ghiConnections[slot];

	// Init the object.
	///////////////////
	memset(connection, 0, sizeof(GHIConnection));
	connection->inUse = GHTTPTrue;
	connection->request = (GHTTPRequest)slot;
	connection->uniqueID = ghiNextUniqueID++;
	connection->type = GHIGET;
	connection->state = GHTTPHostLookup;
	connection->URL = NULL;
	connection->serverAddress = NULL;
	connection->serverIP = INADDR_ANY;
	connection->serverPort = (unsigned short)0;
	connection->requestPath = NULL;
	connection->sendHeaders = NULL;
	connection->saveFile = NULL;
	connection->blocking = GHTTPFalse;
	connection->persistConnection = GHTTPFalse;
	connection->result = GHTTPSuccess;
	connection->progressCallback = NULL;
	connection->completedCallback = NULL;
	connection->callbackParam = NULL;
	connection->socket = INVALID_SOCKET;
	connection->socketError = 0;
	connection->userBufferSupplied = GHTTPFalse;
	connection->statusMajorVersion = 0;
	connection->statusMinorVersion = 0;
	connection->statusCode = 0;
	connection->statusStringIndex = 0;
	connection->headerStringIndex = 0;
	connection->tempDataIndex = 0;
	connection->completed = GHTTPFalse;
	connection->fileBytesReceived = 0;
	connection->totalSize = -1;
	connection->redirectURL = NULL;
	connection->redirectCount = 0;
	connection->chunkedTransfer = GHTTPFalse;
	connection->processing = GHTTPFalse;
	connection->throttle = GHTTPFalse;
	connection->lastThrottleRecv = 0;
	connection->post = NULL;
	connection->maxRecvTime = 500; // Prevent blocking in async mode with systems that never generate WSAEWOULDBLOCK
	connection->proxyOverridePort = GHI_DEFAULT_PORT;	
	connection->proxyOverrideServer = NULL;
	connection->encryptor.mInterface = NULL;

	bResult = ghiInitBuffer(connection, &connection->sendBuffer, SEND_BUFFER_INITIAL_SIZE, SEND_BUFFER_INCREMENT_SIZE);
	if(bResult)
		bResult = ghiInitBuffer(connection, &connection->recvBuffer, RECV_BUFFER_INITIAL_SIZE, RECV_BUFFER_INCREMENT_SIZE);
	if (bResult)
		bResult = ghiInitBuffer(connection, &connection->decodeBuffer, DECODE_BUFFER_INITIAL_SIZE, DECODE_BUFFER_INCREMENT_SIZE);
	
	if(!bResult)
	{
		ghiFreeConnection(connection);
		ghiUnlock();
		return NULL;
	}

	// One more connection.
	///////////////////////
	ghiNumConnections++;

	ghiUnlock();

	return connection;
}

GHTTPBool ghiFreeConnection
(
	GHIConnection * connection
)
{
	assertWithLine(connection, 179);
	assertWithLine(connection->request >= 0, 180);
	assertWithLine(connection->request < ghiConnectionsLen, 181);
	assertWithLine(connection->inUse, 182);

	// Check args.
	//////////////
	if(!connection)
		return GHTTPFalse;
	if(!connection->inUse)
		return GHTTPFalse;
	if(connection->request < 0)
		return GHTTPFalse;
	if(connection->request >= ghiConnectionsLen)
		return GHTTPFalse;

	ghiLock();

	// Free data.
	/////////////
	gsifree(connection->URL);
	gsifree(connection->serverAddress);
	gsifree(connection->requestPath);
	gsifree(connection->sendHeaders);
	gsifree(connection->redirectURL);
	gsifree(connection->proxyOverrideServer);
#ifndef NOFILE
	if(connection->saveFile)
		fclose(connection->saveFile);
#endif
	if(connection->socket != INVALID_SOCKET)
	{
		shutdown(connection->socket, 2);
		closesocket(connection->socket);
	}
	ghiFreeBuffer(&connection->sendBuffer);
	ghiFreeBuffer(&connection->recvBuffer);
	ghiFreeBuffer(&connection->decodeBuffer);
	ghiFreeBuffer(&connection->getFileBuffer);
	if(connection->postingState.states)
		ghiPostCleanupState(connection);

	// Check for an auto-free post.
	///////////////////////////////
	if(connection->post && ghiIsPostAutoFree(connection->post))
	{
		ghiFreePost(connection->post);
		connection->post = NULL;
	}

	// Check for an encryptor
	if (connection->encryptor.mInitialized != GHTTPFalse)
	{
		if (connection->encryptor.mCleanupFunc)
			(connection->encryptor.mCleanupFunc)(connection, &connection->encryptor);
		connection->encryptor.mInitialized = GHTTPFalse;
	}

	// Free the slot.
	/////////////////
	connection->inUse = GHTTPFalse;

	// One less connection.
	///////////////////////
	ghiNumConnections--;

	ghiUnlock();

	return GHTTPTrue;
}

GHIConnection * ghiRequestToConnection
(
	GHTTPRequest request
)
{
	GHIConnection * connection;

	assertWithLine(request >= 0, 257);
	assertWithLine(request < ghiConnectionsLen, 258);

	ghiLock();

	// Check args.
	//////////////
	if((request < 0) || (request >= ghiConnectionsLen))
	{
		ghiUnlock();
		return NULL;
	}

	connection = ghiConnections[request];

	// Check for not in use.
	////////////////////////
	if(!connection->inUse)
		connection = NULL;

	ghiUnlock();

	return connection;
}

void ghiEnumConnections
(
	GHTTPBool (* callback)(GHIConnection *)
)
{
	int i;

	// Check for no connections.
	////////////////////////////
	if(ghiNumConnections <= 0)
		return;

	ghiLock();
	for(i = 0 ; i < ghiConnectionsLen ; i++)
		if(ghiConnections[i]->inUse)
			callback(ghiConnections[i]);
	ghiUnlock();
}

void ghiRedirectConnection
(
	GHIConnection * connection
)
{
	assertWithLine(connection, 306);
	assertWithLine(connection->redirectURL, 307);
	
	gsDebugFormat(GSIDebugCat_HTTP, GSIDebugType_State, GSIDebugLevel_Comment, "Redirecting Connection\n");

	// Reset state.
	///////////////
	connection->state = GHTTPHostLookup;

	// New URL.
	///////////
	gsifree(connection->URL);
	connection->URL = connection->redirectURL;
	connection->redirectURL = NULL;

	// Reset stuff parsed from the URL.
	///////////////////////////////////
	gsifree(connection->serverAddress);
	connection->serverAddress = NULL;
	connection->serverIP = 0;
	connection->serverPort = 0;
	gsifree(connection->requestPath);
	connection->requestPath = NULL;

	// Close the socket.
	////////////////////
	shutdown(connection->socket, 2);
	closesocket(connection->socket);
	connection->socket = INVALID_SOCKET;

	// Reset buffers.
	/////////////////
	ghiResetBuffer(&connection->sendBuffer);
	ghiResetBuffer(&connection->recvBuffer);
	ghiResetBuffer(&connection->decodeBuffer);

	// Reset status.
	////////////////
	connection->statusMajorVersion = 0;
	connection->statusMinorVersion = 0;
	connection->statusCode = 0;
	connection->statusStringIndex = 0;

	connection->headerStringIndex = 0;
	connection->tempDataIndex = 0;

	// The connection isn't closed.
	///////////////////////////////
	connection->connectionClosed = GHTTPFalse;

	// One more redirect.
	/////////////////////
	connection->redirectCount++;
}

void ghiCleanupConnections
(
	void
)
{
	int i;

	if(!ghiConnections)
		return;

	// Cleanup all running connections.
	///////////////////////////////////
	ghiEnumConnections(ghiFreeConnection);

	// Cleanup the connection states.
	/////////////////////////////////
	for(i = 0 ; i < ghiConnectionsLen ; i++)
		gsifree(ghiConnections[i]);
	gsifree(ghiConnections);
	ghiConnections = NULL;
	ghiConnectionsLen = 0;
	ghiNumConnections = 0;
}
