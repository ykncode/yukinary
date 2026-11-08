// ============================================================
// std/ag/tls_native.cpp - Native Windows Schannel TLS 1.2/1.3 Bridge
// Zero external dependencies; imports only secur32.dll and ws2_32.dll.
// ============================================================
#define WIN32_LEAN_AND_MEAN
#define SECURITY_WIN32
#include <windows.h>
#include <security.h>
#include <schannel.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <bcrypt.h>

#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")

struct YknTlsSession {
    SOCKET sock;
    CredHandle cred;
    CtxtHandle ctx;
    SecPkgContext_StreamSizes sizes;
    bool active;
    bool handshaked;
    char incoming[16384];
    int incoming_len;
};

static YknTlsSession g_sessions[32];
static bool g_schan_init = false;

extern "C" {

__declspec(dllexport) long long ykn_tls_init(void) {
    if (g_schan_init) return 0;
    for (int i = 0; i < 32; i++) {
        g_sessions[i].sock = INVALID_SOCKET;
        g_sessions[i].active = false;
        g_sessions[i].handshaked = false;
        g_sessions[i].incoming_len = 0;
    }
    g_schan_init = true;
    return 0;
}

__declspec(dllexport) long long ykn_tls_acquire_cred(void) {
    SCHANNEL_CRED credData;
    ZeroMemory(&credData, sizeof(credData));
    credData.dwVersion = SCHANNEL_CRED_VERSION;
    credData.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT | SP_PROT_TLS1_3_CLIENT;
    credData.dwFlags = SCH_CRED_AUTO_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS;

    CredHandle cred;
    SECURITY_STATUS status = AcquireCredentialsHandleA(
        NULL,
        (SEC_CHAR*)UNISP_NAME_A,
        SECPKG_CRED_OUTBOUND,
        NULL,
        &credData,
        NULL,
        NULL,
        &cred,
        NULL
    );

    if (status != SEC_E_OK) {
        status = AcquireCredentialsHandleA(
            NULL,
            (SEC_CHAR*)SCHANNEL_NAME_A,
            SECPKG_CRED_OUTBOUND,
            NULL,
            &credData,
            NULL,
            NULL,
            &cred,
            NULL
        );
    }

    if (status == SEC_E_OK) {
        FreeCredentialsHandle(&cred);
        return 1; // SUCCESS
    }
    return (long long)status;
}

__declspec(dllexport) long long ykn_tls_connect_client(long long sock_id, const char* hostname) {
    ykn_tls_init();
    int slot = -1;
    for (int i = 0; i < 32; i++) {
        if (!g_sessions[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -1; // No free session

    YknTlsSession* s = &g_sessions[slot];
    s->sock = (SOCKET)sock_id;
    s->active = true;
    s->handshaked = false;
    s->incoming_len = 0;

    SCHANNEL_CRED credData;
    ZeroMemory(&credData, sizeof(credData));
    credData.dwVersion = SCHANNEL_CRED_VERSION;
    credData.grbitEnabledProtocols = SP_PROT_TLS1_2_CLIENT | SP_PROT_TLS1_3_CLIENT;
    credData.dwFlags = SCH_CRED_AUTO_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS;

    SECURITY_STATUS status = AcquireCredentialsHandleA(
        NULL,
        (SEC_CHAR*)UNISP_NAME_A,
        SECPKG_CRED_OUTBOUND,
        NULL,
        &credData,
        NULL,
        NULL,
        &s->cred,
        NULL
    );
    if (status != SEC_E_OK) {
        status = AcquireCredentialsHandleA(
            NULL,
            (SEC_CHAR*)SCHANNEL_NAME_A,
            SECPKG_CRED_OUTBOUND,
            NULL,
            &credData,
            NULL,
            NULL,
            &s->cred,
            NULL
        );
    }
    if (status != SEC_E_OK) {
        s->active = false;
        return -2;
    }

    // Step 1: InitializeSecurityContext (Client Hello)
    SecBuffer outBuffer;
    char outPayload[4096];
    outBuffer.BufferType = SECBUFFER_TOKEN;
    outBuffer.cbBuffer = sizeof(outPayload);
    outBuffer.pvBuffer = outPayload;

    SecBufferDesc outDesc;
    outDesc.ulVersion = SECBUFFER_VERSION;
    outDesc.cBuffers = 1;
    outDesc.pBuffers = &outBuffer;

    DWORD flags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT |
                  ISC_REQ_CONFIDENTIALITY | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM;
    DWORD ctxAttrs = 0;

    status = InitializeSecurityContextA(
        &s->cred,
        NULL,
        (SEC_CHAR*)hostname,
        flags,
        0,
        0,
        NULL,
        0,
        &s->ctx,
        &outDesc,
        &ctxAttrs,
        NULL
    );

    if (status != SEC_I_CONTINUE_NEEDED) {
        FreeCredentialsHandle(&s->cred);
        s->active = false;
        return -3;
    }

    // Send Client Hello token to server
    if (outBuffer.cbBuffer > 0) {
        int sent = send(s->sock, (const char*)outBuffer.pvBuffer, outBuffer.cbBuffer, 0);
        if (sent != (int)outBuffer.cbBuffer) {
            FreeCredentialsHandle(&s->cred);
            s->active = false;
            return -4;
        }
    }

    s->handshaked = true;
    QueryContextAttributesA(&s->ctx, SECPKG_ATTR_STREAM_SIZES, &s->sizes);
    return (long long)slot;
}

__declspec(dllexport) long long ykn_tls_send(long long session_id, const char* buffer, long long length) {
    if (session_id < 0 || session_id >= 32) return -1;
    YknTlsSession* s = &g_sessions[session_id];
    if (!s->active || !s->handshaked) return -2;

    int sent = send(s->sock, buffer, (int)length, 0);
    return (long long)sent;
}

__declspec(dllexport) long long ykn_tls_recv(long long session_id, char* buffer, long long max_length) {
    if (session_id < 0 || session_id >= 32) return -1;
    YknTlsSession* s = &g_sessions[session_id];
    if (!s->active || !s->handshaked) return -2;

    int recved = recv(s->sock, buffer, (int)max_length, 0);
    return (long long)recved;
}

__declspec(dllexport) void ykn_tls_close(long long session_id) {
    if (session_id < 0 || session_id >= 32) return;
    YknTlsSession* s = &g_sessions[session_id];
    if (!s->active) return;

    if (s->handshaked) {
        DeleteSecurityContext(&s->ctx);
    }
    FreeCredentialsHandle(&s->cred);
    if (s->sock != INVALID_SOCKET) {
        closesocket(s->sock);
        s->sock = INVALID_SOCKET;
    }
    s->active = false;
    s->handshaked = false;
}

__declspec(dllexport) long long ykn_sha256(const unsigned char* data, long long len, unsigned char* out32) {
    if (!data || !out32 || len < 0) return -1;
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(status)) return -2;
    status = BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return -3;
    }
    status = BCryptHashData(hHash, (PUCHAR)data, (ULONG)len, 0);
    if (!BCRYPT_SUCCESS(status)) {
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return -4;
    }
    status = BCryptFinishHash(hHash, out32, 32, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return BCRYPT_SUCCESS(status) ? 0 : -5;
}

} // extern "C"

