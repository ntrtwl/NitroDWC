///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#include "gs/ghttp/ghttpCommon.h"
#if defined(MATRIXSSL)
#include "../matrixssl/matrixssl.h"
#endif


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GHTTPBool ghttpSetRequestEncryptionEngine(GHTTPRequest request, GHTTPEncryptionEngine engine)
{
	GHIConnection * connection = ghiRequestToConnection(request);
	if(!connection)
		return GHTTPFalse;

	// If the same engine has previously been set then we're done
	if (connection->encryptor.mEngine == engine)
		return GHTTPTrue; 

	// If a different engine has previously been set then we're screwed
	if (connection->encryptor.mInterface != NULL &&
		connection->encryptor.mEngine != engine)
	{
		return GHTTPFalse; 
	}

	// If the URL is HTTPS but the engine is specific as NONE then we can't connect
	if((engine == GHTTPEncryptionEngine_None) && (strncmp(connection->URL, "https://", 8) == 0))
		return GHTTPFalse;

	// Initialize the engine
	connection->encryptor.mEngine = engine;

	if (engine == GHTTPEncryptionEngine_None)
	{
		connection->encryptor.mInterface = NULL;
		return GHTTPTrue; // this is the default, just return
	}
	else
	{
		connection->encryptor.mInterface   = NULL;
#ifdef MATRIXSSL
		connection->encryptor.mInitFunc    = ghiEncryptorSslInitFunc;
		connection->encryptor.mCleanupFunc = ghiEncryptorSslCleanupFunc;
		connection->encryptor.mEncryptFunc = ghiEncryptorSslEncryptFunc;
		connection->encryptor.mDecryptFunc = ghiEncryptorSslDecryptFunc;
#endif
		connection->encryptor.mInitialized = GHTTPFalse;
		connection->encryptor.mSessionEstablished = GHTTPFalse;
		return GHTTPTrue;
	}
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// *********************  MATRIXSSL ENCRYPTION ENGINE  ********************* //
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#ifdef MATRIXSSL

// SSL requires a certificate validator
static int ghiSslCertValidator(struct sslCertInfo* theCertInfo, void* theUserData)
{
	// Taken from matrisSslExample
	sslCertInfo_t	*next;
/*
	Make sure we are checking the last cert in the chain
*/
	next = theCertInfo;
	while (next->next != NULL) {
		next = next->next;
	}
	return next->verified;
}

// Init the engine
GHIEncryptionResult ghiEncryptorSslInitFunc(struct GHIConnection * connection,
											  struct GHIEncryptor  * theEncryptor)
{
	sslKeys_t *keys = NULL;
	sslSessionId_t *id = NULL;

	int ecodeResult;

	if (matrixSslOpen() < 0)
		return GHIEncryptionResult_Error;

	if (matrixSslReadKeys(&keys, NULL, NULL, NULL, NULL) < 0)
		return GHIEncryptionResult_Error;

	if (matrixSslNewSession((ssl_t**)&theEncryptor->mInterface, keys, id, 0) < 0)
		return GHIEncryptionResult_Error;

	matrixSslSetCertValidator((ssl_t*)theEncryptor->mInterface, ghiSslCertValidator, NULL);

	theEncryptor->mInitialized = GHTTPTrue;
	return GHIEncryptionResult_Success;
}

// Start the handshake process
GHIEncryptionResult ghiEncryptorSslInitFunc(struct GHIConnection * connection,
											  struct GHIEncryptor  * theEncryptor)
{
	sslBuf_t helloWrapper;
	
	// Prepare the hello message
	helloWrapper.buf   = connection->sendBuffer.data;
	helloWrapper.size  = connection->sendBuffer.size;
	helloWrapper.start = connection->sendBuffer.data + connection->sendBuffer.pos;
	helloWrapper.end   = helloWrapper.start; // start writing here
	
	ecodeResult = matrixSslEncodeClientHello((ssl_t*)theEncryptor->mInterface, &helloWrapper, 0); // 0 = cipher
	if (ecodeResult != 0) 
		return GHIEncryptionResult_Error; // error!

	// Adjust the sendBuffer to account for the new data
	connection->sendBuffer.len += (int)(helloWrapper.end - helloWrapper.start);
	connection->sendBuffer.encrypted = GHTTPTrue;
	theEncryptor->mSessionStarted = GHTTPTrue;
}

// Destroy the engine
GHIEncryptionResult ghiEncryptorSslCleanupFunc(struct GHIConnection * connection,
												 struct GHIEncryptor  * theEncryptor)
{
	matrixSslClose();
	return GHIEncryptionResult_Success;
}

// Encrypt some data
//    -  theEncryptedLength is reduced by the length of data written to theEncryptedBuffer
GHIEncryptionResult ghiEncryptorSslEncryptFunc(struct GHIConnection * connection,
												 struct GHIEncryptor  * theEncryptor,
												 const char * thePlainTextBuffer,
												 int          thePlainTextLength,
												 char *       theEncryptedBuffer,
												 int *        theEncryptedLength)
{
	int encodeResult = 0;

	// SSL buffer wrapper
	// Append to theDecryptedBuffer
	sslBuf_t encryptedBuf;
	encryptedBuf.buf   = theEncryptedBuffer;  // buf starts here
	encryptedBuf.start = theEncryptedBuffer;  // readpos,  set to start
	encryptedBuf.end   = theEncryptedBuffer;  // writepos, set to start
	encryptedBuf.size  = *theEncryptedLength; // total size of buf
	
	// perform the encryption
	encodeResult = matrixSslEncode(connection->encryptor.mInterface, 
		(unsigned char*)thePlainTextBuffer, *thePlainTextLength, &encryptedBuf);

	if (encodeResult == SSL_ERROR)
		return GHIEncryptionResult_Error;
	else if (encodeResult == SSL_FULL)
		return GHIEncryptionResult_BufferTooSmall;
	else
	{
		//*thePlainTextLength = *thePlainTextLength; // we always use the entire buffer
		*theEncryptedLength -= (int)(encryptedBuf.end - encryptedBuf.start);
		return GHIEncryptionResult_Success;
	}
}

// Decrypt some data
//    -  During the handshaking process, this may result in data being appended to the send buffer
//    -  Data may be left in the encrypted buffer
//    -  theEncryptedLength becomes the length of data read from theEncryptedBuffer
//    -  theDecryptedLength becomes the length of data written to theDecryptedBuffer
GHIEncryptionResult ghiEncryptorSslDecryptFunc(struct GHIConnection * connection,
												 struct GHIEncryptor  * theEncryptor,
												 const char * theEncryptedBuffer,
												 int *        theEncryptedLength,
												 char *       theDecryptedBuffer,
												 int *        theDecryptedLength)
{
	GHTTPBool decryptMore = GHTTPTrue;
	int decodeResult = 0;

	// SSL buffer wrappers
	sslBuf_t inBuf;
	sslBuf_t decryptedBuf;
	int encryptedStartSize = *theEncryptedLength;

	// Read from theEncryptedBuffer - Have to cast away the "const"
	inBuf.buf   = (unsigned char*)theEncryptedBuffer;  
	inBuf.start = (unsigned char*)theEncryptedBuffer;
	inBuf.end   = (unsigned char*)theEncryptedBuffer + *theEncryptedLength;
	inBuf.size  = *theEncryptedLength;

	// Append to theDecryptedBuffer
	decryptedBuf.buf   = theDecryptedBuffer;  // buf starts here
	decryptedBuf.start = theDecryptedBuffer;  // readpos,  set to start
	decryptedBuf.end   = theDecryptedBuffer;  // writepos, set to start
	decryptedBuf.size  = *theDecryptedLength; // total size of buf
	
	// Perform the decode operation
	//     - may require multiple tries
	while(decryptMore != GHTTPFalse && ((inBuf.end-inBuf.start) > 0))
	{
		unsigned char error = 0;
		unsigned char alertlevel = 0;
		unsigned char alertdescription = 0;

		// perform the decode, this will decode a single SSL message at a time
		decodeResult = matrixSslDecode(theEncryptor->mInterface, &inBuf, &decryptedBuf, 
										&error, &alertlevel, &alertdescription);
		switch(decodeResult)
		{
		case SSL_SUCCESS:          
			// a message was handled internally by matrixssl
			// No data is appeneded to the decrypted buffer
			if (matrixSslHandshakeIsComplete(theEncryptor->mInterface))
				theEncryptor->mSessionEstablished = GHTTPTrue;
			break;

		case SSL_PROCESS_DATA:
			// We've received app data, continue on.  
			// App data was appended to the decrypted buffer
			break;

		case SSL_SEND_RESPONSE:
			{
			// we must send an SSL response which has been written to decryptedBuf
			// transfer this response to the connection's sendBuffer
			int responseSize = decryptedBuf.end - decryptedBuf.start;

			// force disable-encryption
			//   this may seem like a hack, but it's the best way to avoid
			//   unnecessary data copies without modifying matrixSSL
			theEncryptor->mSessionEstablished = GHTTPFalse;
			ghiTrySendThenBuffer(connection, decryptedBuf.start, responseSize);
			theEncryptor->mSessionEstablished = GHTTPTrue;

			// Remove the bytes from the decrypted buffer (we don't want to return them to the app)
			decryptedBuf.end = decryptedBuf.start; // bug?
			break;
			}

		case SSL_ERROR:            
			// error decoding the data
			decryptMore = GHTTPFalse;
			break;

		case SSL_ALERT:            
			// server sent an alert
			if (alertdescription == SSL_ALERT_CLOSE_NOTIFY)
			decryptMore = GHTTPFalse;
			break;

		case SSL_PARTIAL:          
			// need to read more data from the socket(inbuf incomplete)
			decryptMore = GHTTPFalse;
			break;

		case SSL_FULL:             
			{
				// decodeBuffer is too small, need to increase size and try again
				decryptMore = GHTTPFalse;
				break;
			}
		};
	}

	// Store off the lengths
	*theEncryptedLength = encryptedStartSize - (inBuf.end - inBuf.start);
	*theDecryptedLength = decryptedBuf.end - decryptedBuf.start;

	// Return status to app
	if (decodeResult == SSL_FULL)
		return GHIEncryptionResult_BufferTooSmall;
	else if (decodeResult == SSL_ERROR || decodeResult == SSL_ALERT)
		return GHIEncryptionResult_Error;

	//if ((int)(decryptedBuf.end - decryptedBuf.start) > 0)
	//	printf ("Decrypted: %d bytes\r\n", *theDecryptedLength);
	return GHIEncryptionResult_Success;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#endif // encryption engine switch


