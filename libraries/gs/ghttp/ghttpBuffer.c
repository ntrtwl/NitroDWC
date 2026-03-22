 /*
GameSpy GHTTP SDK 
Dan "Mr. Pants" Schoenblum
dan@gamespy.com

Copyright 1999-2007 GameSpy Industries, Inc

devsupport@gamespy.com
*/

#include "gs/ghttp/ghttpBuffer.h"
#include "gs/ghttp/ghttpConnection.h"
#include "gs/ghttp/ghttpMain.h"
#include "gs/ghttp/ghttpCommon.h"


// Resize the buffer.
/////////////////////
GHTTPBool ghiResizeBuffer
(
	GHIBuffer * buffer,
	int sizeIncrement
)
{
	char * tempPtr;
	int newSize;

	assertWithLine(buffer, 32);
	assertWithLine(sizeIncrement > 0, 33);

	// Check args.
	//////////////
	if(!buffer)
		return GHTTPFalse;
	if(sizeIncrement <= 0)
		return GHTTPFalse;

	// Reallocate with the bigger size.
	///////////////////////////////////
	newSize = (buffer->size + sizeIncrement);
	tempPtr = (char *)gsirealloc(buffer->data, (unsigned int)newSize);
	if(!tempPtr)
		return GHTTPFalse;

	// Set the new info.
	////////////////////
	buffer->data = tempPtr;
	buffer->size = newSize;

	return GHTTPTrue;
}

GHTTPBool ghiInitBuffer
(
	struct GHIConnection * connection,
	GHIBuffer * buffer,
	int initialSize,
	int sizeIncrement
)
{
	GHTTPBool bResult;

	assertWithLine(connection, 67);
	assertWithLine(buffer, 68);
	assertWithLine(initialSize > 0, 69);
	assertWithLine(sizeIncrement > 0, 70);

	// Check args.
	//////////////
	if(!connection)
		return GHTTPFalse;
	if(!buffer)
		return GHTTPFalse;
	if(initialSize <= 0)
		return GHTTPFalse;
	if(sizeIncrement <= 0)
		return GHTTPFalse;

	// Init the struct.
	///////////////////
	buffer->connection = connection;
	buffer->data = NULL;
	buffer->size = 0;
	buffer->len = 0;
	buffer->pos = 0;
	buffer->sizeIncrement = sizeIncrement;
	buffer->fixed = GHTTPFalse;
	buffer->dontFree = GHTTPFalse;
	buffer->encrypted = GHTTPFalse;

	// Do the initial resize.
	/////////////////////////
	bResult = ghiResizeBuffer(buffer, initialSize);
	if(!bResult)
		return GHTTPFalse;

	// Start with an empty string.
	//////////////////////////////
	*buffer->data = '\0';

	return GHTTPTrue;
}

GHTTPBool ghiInitFixedBuffer
(
	struct GHIConnection * connection,
	GHIBuffer * buffer,
	char * userBuffer,
	int size
)
{
	assertWithLine(connection, 116);
	assertWithLine(buffer, 117);
	assertWithLine(userBuffer, 118);
	assertWithLine(size > 0, 119);

	// Check args.
	//////////////
	if(!connection)
		return GHTTPFalse;
	if(!buffer)
		return GHTTPFalse;
	if(!userBuffer)
		return GHTTPFalse;
	if(size <= 0)
		return GHTTPFalse;

	// Init the struct.
	///////////////////
	buffer->connection = connection;
	buffer->data = userBuffer;
	buffer->size = size;
	buffer->len = 0;
	buffer->sizeIncrement = 0;
	buffer->fixed = GHTTPTrue;
	buffer->dontFree = GHTTPTrue;
	buffer->encrypted = GHTTPFalse;

	// Start with an empty string.
	//////////////////////////////
	*buffer->data = '\0';

	return GHTTPTrue;
}

void ghiFreeBuffer
(
	GHIBuffer * buffer
)
{
	assertWithLine(buffer, 155);

	// Check args.
	//////////////
	if(!buffer)
		return;
	if(!buffer->data)
		return;

	// Cleanup the struct.
	//////////////////////
	if(!buffer->dontFree)
		gsifree(buffer->data);
	memset(buffer, 0, sizeof(GHIBuffer));
}

GHTTPBool ghiAppendDataToBuffer
(
	GHIBuffer * buffer,
	const char * data,
	int dataLen
)
{
	GHTTPBool bResult;
	int newLen;
	GHIConnection *connection = buffer->connection;

	assertWithLine(buffer, 183);
	assertWithLine(data, 184);
	assertWithLine(dataLen >= 0, 185);

	// Check args.
	//////////////
	if(!buffer)
		return GHTTPFalse;
	if(!data)
		return GHTTPFalse;
	if(dataLen < 0)
		return GHTTPFalse;

	// Get the string length if needed.
	///////////////////////////////////
	if(dataLen == 0)
		dataLen = (int)strlen(data);

	if(buffer->encrypted == GHTTPTrue)
	{
		assertWithLine(connection->encryptor.mEngine != GHTTPEncryptionEngine_None, 206);

		int aResult;
		do
		{
			int bufSpace = buffer->size - buffer->len;

			aResult = connection->encryptor.mEncryptFunc(connection, &connection->encryptor,
														 data, &dataLen,
														 buffer->data + buffer->len, &bufSpace);

			if(aResult == GHIEncryptionResult_BufferTooSmall)
			{
				if(buffer->fixed)
				{
					buffer->connection->completed = GHTTPTrue;
					buffer->connection->result = GHTTPBufferOverflow;
					return GHTTPFalse;
				} else
				{
					int bResult = ghiResizeBuffer(buffer, buffer->sizeIncrement);
					if(bResult)
					{
						buffer->connection->completed = GHTTPTrue;
						buffer->connection->result = GHTTPOutOfMemory;
						return GHTTPFalse;
					}
				}
			} else
				buffer->len += bufSpace;
		} while (aResult == GHIEncryptionResult_BufferTooSmall);
	} else
	{
		// Get the new length.
		//////////////////////
		newLen = (buffer->len + dataLen);

		// Make sure the array is big enough.
		/////////////////////////////////////
		while(newLen >= buffer->size)
		{
			// Check for a fixed buffer.
			////////////////////////////
			if(buffer->fixed)
			{
				buffer->connection->completed = GHTTPTrue;
				buffer->connection->result = GHTTPBufferOverflow;
				return GHTTPFalse;
			}

			bResult = ghiResizeBuffer(buffer, buffer->sizeIncrement);
			if(!bResult)
			{
				buffer->connection->completed = GHTTPTrue;
				buffer->connection->result = GHTTPOutOfMemory;
				return GHTTPFalse;
			}
		}

		// Add the data.
		////////////////
		memcpy(buffer->data + buffer->len, data, (unsigned int)dataLen);
		buffer->len = newLen;
		buffer->data[buffer->len] = '\0';
	}

	return GHTTPTrue;
}

GHTTPBool ghiAppendHeaderToBuffer
(
	GHIBuffer * buffer,
	const char * name,
	const char * value
)
{
	if(!ghiAppendDataToBuffer(buffer, name, 0))
		return GHTTPFalse;
	if(!ghiAppendDataToBuffer(buffer, ": ", 2))
		return GHTTPFalse;
	if(!ghiAppendDataToBuffer(buffer, value, 0))
		return GHTTPFalse;
	if(!ghiAppendDataToBuffer(buffer, CRLF, 2))
		return GHTTPFalse;

	return GHTTPTrue;
}

GHTTPBool ghiAppendCharToBuffer
(
	GHIBuffer * buffer,
	int c
)
{
	char ch = c;

	assertWithLine(buffer, 305);

	// Check args.
	//////////////
	if(!buffer)
		return GHTTPFalse;

	return ghiAppendDataToBuffer(buffer, &ch, 1);
}

GHTTPBool ghiAppendIntToBuffer
(
	GHIBuffer * buffer,
	int i
)
{
	char intValue[16];

	sprintf(intValue, "%d", i);

	return ghiAppendDataToBuffer(buffer, intValue, 0);
}

void ghiResetBuffer
(
	GHIBuffer * buffer
)
{
	assertWithLine(buffer, 364);

	buffer->len = 0;
	buffer->pos = 0;

	// Start with an empty string.
	//////////////////////////////
	*buffer->data = '\0';
}

GHTTPBool ghiSendBufferedData
(
	struct GHIConnection * connection
)
{
	int rcode;
	int writeFlag;
	int exceptFlag;
	char * data;
	int len;

	// Loop while we can send.
	//////////////////////////
	do
	{
		rcode = GSISocketSelect(connection->socket, NULL, &writeFlag, &exceptFlag);
		if((gsiSocketIsError(rcode)) || exceptFlag)
		{
			connection->completed = GHTTPTrue;
			connection->result = GHTTPSocketFailed;
			connection->socketError = GOAGetLastError(connection->socket);
			return GHTTPFalse;
		}
		if(!writeFlag)
		{
			// Can't send anything.
			///////////////////////
			return GHTTPTrue;
		}

		// Figure out what, and how much, to send.
		//////////////////////////////////////////
		data = (connection->sendBuffer.data + connection->sendBuffer.pos);
		len = (connection->sendBuffer.len - connection->sendBuffer.pos);

		// Do the send.
		///////////////
		rcode = ghiDoSend(connection, data, len);
		if(gsiSocketIsError(rcode))
			return GHTTPFalse;

		// Update the position.
		///////////////////////
		connection->sendBuffer.pos += rcode;
	}
	while(connection->sendBuffer.pos < connection->sendBuffer.len);

	return GHTTPTrue;
}


// Read data from a buffer
GHTTPBool ghiReadDataFromBuffer
(
	GHIBuffer * bufferIn,    // the GHIBuffer to read from
	char        bufferOut[], // the raw buffer to write to
	int *       len          // max number of bytes to append, becomes actual length written
)
{
	int bytesAvailable = 0;
	int bytesToCopy    = 0;
	
	
	// Verify parameters
	assertWithLine(bufferIn != NULL, 439);
	assertWithLine(len != NULL, 440);
	if (*len == 0)
		return GHTTPFalse;

	// Make sure the bufferIn isn't emtpy
	bytesAvailable = (int)bufferIn->len - bufferIn->pos;
	if (bytesAvailable <= 0)
		return GHTTPFalse;

	// Calculate the actual number of bytes to copy
	bytesToCopy = min(*len, bytesAvailable);

	// Copy the bytes
	memcpy(bufferOut, bufferIn->data + bufferIn->pos, (size_t)bytesToCopy);
	bufferOut[bytesToCopy] = '\0';
	*len = bytesToCopy;

	// Adjust the bufferIn read position
	bufferIn->pos += bytesToCopy;
	return GHTTPTrue;
}
