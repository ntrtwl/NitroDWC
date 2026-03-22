#ifndef __GHTTPENCRYPTION_H__
#define __GHTTPENCRYPTION_H__

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Encryption method
typedef enum {
    GHIEncryptionMethod_None,
    GHIEncryptionMethod_Encrypt,  // encrypt raw data written to buffer
    GHIEncryptionMethod_Decrypt   // decrypt raw data written to buffer
} GHIEncryptionMethod;

// Encryption results
typedef enum {
    GHIEncryptionResult_None,
    GHIEncryptionResult_Success,        // successfully encrypted/decrypted
    GHIEncryptionResult_BufferTooSmall, // buffer was too small to hold converted data
    GHIEncryptionResult_Error           // some other kind of error
} GHIEncryptionResult;


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
struct GHIEncryptor; // forward declare for callbacks
struct GHIConnection;

// Called to init the encryption engine
typedef GHIEncryptionResult (*GHTTPEncryptorInitFunc) (
    struct GHIConnection *theConnection,
    struct GHIEncryptor *theEncryptor
);

// Called to connect the socket (some engines do this internally)
typedef GHIEncryptionResult (*GHTTPEncryptorCleanupFunc)(
    struct GHIConnection *theConnection,
    struct GHIEncryptor *theEncryptor
);

// Called to start the handshake process engine
typedef GHIEncryptionResult (*GHTTPEncryptorEncryptFunc)(
    struct GHIConnection *theConnection,
    struct GHIEncryptor *theEncryptor,
    const char *thePlainTextBuffer,
    int *thePlainTextLength,
    char *theEncryptedBuffer,
    int *theEncryptedLength
);

// Called to destroy the encryption engine
typedef GHIEncryptionResult (*GHTTPEncryptorDecryptFunc)(
    struct GHIConnection *theConnection,
    struct GHIEncryptor *theEncryptor,
    const char *theEncryptedBuffer,
    int *theEncryptedLength,
    char *theDecryptedBuffer,
    int *theDecryptedLength
);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
typedef struct GHIEncryptor {
    void *mInterface;   // only SSL is currently supported
    GHTTPEncryptionEngine mEngine;
    GHTTPBool mInitialized;
    GHTTPBool mSessionEstablished;  // handshake completed?
    GHTTPEncryptorInitFunc mInitFunc;
    GHTTPEncryptorCleanupFunc mCleanupFunc;
    GHTTPEncryptorEncryptFunc mEncryptFunc;
    GHTTPEncryptorDecryptFunc mDecryptFunc;
} GHIEncryptor;

#ifdef MATRIXSSL
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// ssl encryption
    GHIEncryptionResult ghttpEncryptorSslInitFunc(
        struct GHIConnection *connection,
        struct GHIEncryptor *theEncryptor
    );

    GHIEncryptionResult ghttpEncryptorSslCleanupFunc(
        struct GHIConnection *connection,
        struct GHIEncryptor *theEncryptor
    );

    GHIEncryptionResult ghttpEncryptorSslEncryptFunc(
        struct GHIConnection *connection,
        struct GHIEncryptor *theEncryptor,
        const char *thePlainTextBuffer,
        int *thePlainTextLength,
        char *theEncryptedBuffer,
        int *theEncryptedLength
    );

    GHIEncryptionResult ghttpEncryptorSslDecryptFunc(
        struct GHIConnection *connection,
        struct GHIEncryptor *theEncryptor,
        const char *theEncryptedBuffer,
        int *theEncryptedLength,
        char *theDecryptedBuffer,
        int *theDecryptedLength
    );
#endif

#endif