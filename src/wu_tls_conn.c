#define _WIN32_WINNT 0x0601

#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <Windows.h>
#include <winternl.h>
#include <wincrypt.h>
#include <stdio.h>

#define SCHANNEL_USE_BLACKLISTS

#include <schannel.h>

#define SECURITY_WIN32

#include <sspi.h>

#include "wu_msg.h"
#include "wu_main.h"

extern FILE* g_fphttpslog;
extern SOCKET g_tls_sclt;
extern HANDLE g_hConsoleOutput;
extern SecPkgContext_StreamSizes context_sizes;
extern char* encryptBuffer;

static int tls_recv_start(SOCKET s, SecBuffer secBuffers[4], char* read_buf, int* bytes_read);
static int tls_recv_add_data_to_extra(SOCKET s, SecBuffer secBuffers[4], char* read_buf, int* bytes_read);
static int tls_handshake(CredHandle* credHandle, SOCKET s_clt, SecBufferDesc* secBufferDescInput, SecBuffer secBufferIn[2], unsigned long* fContextAttr,
	CtxtHandle* ctxNewHandle, SecBufferDesc* secBufferDescOutput);
static void acceptSecure_init_schannel_vars(CtxtHandle* ctxNewHandle, char BufferIn1[4096], char BufferIn2[4096],
											SecBuffer secBufferIn[2], SecBuffer secBufferOut[3],
											SecBufferDesc *secBufferDescInput, SecBufferDesc *secBufferDescOutput,
											struct sockaddr_in *sin_clt, int *sinclt_len);


/*
 * Function description:
 * - Encrypt a message and send it to client.
 * - Arguments:
 * - s_clt: Client socket to send message to.
 * - ctxtHandle: Security context handle used in EncryptMessage() function.
 * - message: Message to encrypt and send.
 * - message_size: Size of message in bytes.
 * - cursorPosition: Console cursor position where wu writes schannel error if any.
 * Return value:
 * - 0: Success
 */

int tls_send(SOCKET s_clt, CtxtHandle* ctxtHandle, char* message, unsigned int message_size, COORD cursorPosition) {
	SecBufferDesc bufferDesc;
	SecBuffer secBufferOut[3];
	int ret;

	ZeroMemory(&bufferDesc, sizeof(SecBufferDesc));

	bufferDesc.ulVersion = SECBUFFER_VERSION;
	bufferDesc.cBuffers = 3;
	bufferDesc.pBuffers = secBufferOut;

	ZeroMemory(encryptBuffer, context_sizes.cbHeader + context_sizes.cbMaximumMessage + context_sizes.cbTrailer);

	memcpy(encryptBuffer + context_sizes.cbHeader, message, message_size);

	ZeroMemory(secBufferOut, sizeof(SecBuffer) * 3);
	secBufferOut[0].BufferType = SECBUFFER_STREAM_HEADER;
	secBufferOut[0].pvBuffer = encryptBuffer;
	secBufferOut[0].cbBuffer = context_sizes.cbHeader;
	secBufferOut[1].BufferType = SECBUFFER_DATA;
	secBufferOut[1].pvBuffer = encryptBuffer + context_sizes.cbHeader;
	secBufferOut[1].cbBuffer = message_size;
	secBufferOut[2].BufferType = SECBUFFER_STREAM_TRAILER;
	secBufferOut[2].pvBuffer = encryptBuffer + context_sizes.cbHeader + message_size;
	secBufferOut[2].cbBuffer = context_sizes.cbTrailer;

	ret = EncryptMessage(ctxtHandle, 0, &bufferDesc, 0);
	if (ret != 0)
		show_error_wait_close(&cursorPosition, ERR_MSG_ENCRYPTMESSAGE, NULL, 0);

	if (send(s_clt, encryptBuffer, secBufferOut[0].cbBuffer + secBufferOut[1].cbBuffer + secBufferOut[2].cbBuffer, 0) < 0)
		return -1;

	return 0;
}

/*
 * Function description:
 * - Receive first TLS message from client and prepare secBuffers for DecryptMessage() function.
 * Arguments:
 * - s: Client socket to receive message from.
 * - secBuffers: Array of 4 SecBuffer to prepare for DecryptMessage() function.
 * - read_buf: Buffer to store received message. It will be used in secBuffers[0].pvBuffer.
 * - bytes_read: Number of bytes received and stored in read_buf.
 * Return value:
 * - 0: Success
 * - -1: Failure. It can be caused by recv() failure or if recv() return 0 (connection closed by client).
 */

static int
tls_recv_start(SOCKET s, SecBuffer secBuffers[4], char* read_buf, int* bytes_read) {
	int ret;

	ret = recv(s, read_buf, 2000, 0);
	if (ret <= 0) {
		free(read_buf);
		return -1;
	}

	secBuffers[0].BufferType = SECBUFFER_DATA;
	secBuffers[0].pvBuffer = read_buf;
	secBuffers[0].cbBuffer = ret;
	secBuffers[1].BufferType = SECBUFFER_EMPTY;
	secBuffers[2].BufferType = SECBUFFER_EMPTY;
	secBuffers[3].BufferType = SECBUFFER_EMPTY;

	*bytes_read = ret;

	return 0;
}

/*
 * Functon description:
 * - If a TLS extra message is pending after DecryptMessage() function, receive more data
 *   from client and add it to secBuffers for next DecryptMessage() call.
 * Arguments:
 * - s: Client socket to receive message from.
 * - secBuffers: Array of 4 SecBuffer to prepare for DecryptMessage() function.
 * - read_buf: Buffer to store received message. It will be used in secBuffers[0].pvBuffer.
 * - bytes_read: Number of bytes received and stored in read_buf. It will be updated with
 *   number of bytes received in this function.
 * Return value:
 * - 0: Success
 * - -1: Failure. It can be caused by recv() failure or if recv() return 0 (connection closed by client).
 */

static int
tls_recv_add_data_to_extra(SOCKET s, SecBuffer secBuffers[4], char* read_buf, int* bytes_read) {
	int ret;
	ret = recv(s, read_buf + *bytes_read, 2000 - *bytes_read, 0);
	if (ret <= 0) {
		free(read_buf);
		return -1;
	}
	secBuffers[0].BufferType = SECBUFFER_DATA;
	secBuffers[0].pvBuffer = read_buf;
	secBuffers[0].cbBuffer = *bytes_read + ret;
	secBuffers[1].BufferType = SECBUFFER_EMPTY;
	secBuffers[2].BufferType = SECBUFFER_EMPTY;
	secBuffers[3].BufferType = SECBUFFER_EMPTY;

	*bytes_read += ret;

	return 0;
}

/*
 * Function description:
 * - Receive a TLS message from client, decrypt it and store decrypted data in output buffer.
 * Arguments:
 * - ctxtHandle: Security context handle used in DecryptMessage() function.
 * - s: Client socket to receive message from.
 * - output: Pointer to buffer to store decrypted data. Buffer will be allocated in this function.
 * - outlen: Pointer to unsigned int to store decrypted data length in bytes.
 * - cursorPosition: Console cursor position where wu writes schannel error if any.
 * Return value:
 * - 0: Success
 * - -1: Failure. It can be caused by recv() failure, if recv() return 0 (connection closed by client)
         or if DecryptMessage() failure.
 */
int tls_recv(CtxtHandle* ctxtHandle, SOCKET s, char** output, unsigned int* outlen, COORD* cursorPosition) {
	SECURITY_STATUS secStatus;
	SecBufferDesc secBufferDesc;
	SecBuffer secBuffers[4];

	static char* extra_buf = NULL;
	static int extra_len = 0;

	char* read_buf = NULL;
	int bytes_read = 0;
	int try_count = 0;
	int ret = 0;

	read_buf = (char*)malloc(2000);
	if (read_buf == NULL)
		show_error_wait_close(cursorPosition, ERR_MSG_MEMORY_ALLOC, NULL, 0);

	ZeroMemory(&secBufferDesc, sizeof(SecBufferDesc));
	ZeroMemory(secBuffers, sizeof(SecBuffer) * 4);

	secBufferDesc.ulVersion = SECBUFFER_VERSION;
	secBufferDesc.cBuffers = 4;
	secBufferDesc.pBuffers = secBuffers;

	if (extra_buf != NULL && extra_len > 0 && extra_len < 2000) {
		memcpy(read_buf, extra_buf, extra_len);
		bytes_read = extra_len;
		tls_recv_add_data_to_extra(s, secBuffers, read_buf, &bytes_read);
		free(extra_buf);
		extra_buf = NULL;
		extra_len = 0;
	}
	else if (tls_recv_start(s, secBuffers, read_buf, &bytes_read) < 0)
		return -1;


	secStatus = DecryptMessage(ctxtHandle, &secBufferDesc, 0, NULL);
	if (secStatus == SEC_E_OK) {
		int i;
		int data_index = -1;
		int extra_index = -1;

		for (i = 0; i < 4; i++) {
			if (secBuffers[i].BufferType == SECBUFFER_DATA) {
				data_index = i;
			}
			else if (secBuffers[i].BufferType == SECBUFFER_EXTRA) {
				extra_index = i;
			}
		}

		if (data_index == -1) {
			free(read_buf);
			return -1;
		}

		*outlen = secBuffers[data_index].cbBuffer;
		*output = (char*)malloc(*outlen);
		if (*output == NULL)
			show_error_wait_close(cursorPosition, ERR_MSG_MEMORY_ALLOC, NULL, 0);

		memcpy(*output, secBuffers[data_index].pvBuffer, *outlen);

		if (extra_index >= 0) {
			extra_buf = (char*)malloc(secBuffers[extra_index].cbBuffer);
			extra_len = secBuffers[extra_index].cbBuffer;
			memcpy(extra_buf, secBuffers[extra_index].pvBuffer, extra_len);
		}

		free(read_buf);
	}
	else if (secStatus == SEC_E_INCOMPLETE_MESSAGE) {
		int i;
		int missing_req;
		int missing_index;
		int data_index;
		char* tmp;

	retry_decrypt:
		missing_index = -1;
		data_index = -1;

		for (i = 0; i < 4; i++) {
			if (secBuffers[i].BufferType == SECBUFFER_MISSING) {
				missing_index = i;
			}
		}

		if (missing_index == -1) {
			free(read_buf);
			return -1;
		}

		missing_req = secBuffers[missing_index].cbBuffer;
		tmp = (char*)realloc(read_buf, bytes_read + missing_req);
		if (tmp == NULL) {
			free(read_buf);
			show_error_wait_close(cursorPosition, ERR_MSG_MEMORY_ALLOC, NULL, 0);
		}

		read_buf = tmp;

		ret = recv(s, read_buf + bytes_read, missing_req, MSG_WAITALL);
		if (ret <= 0) {
			free(read_buf);
			return -1;
		}

		bytes_read += ret;

		ZeroMemory(&secBufferDesc, sizeof(SecBufferDesc));
		ZeroMemory(secBuffers, sizeof(SecBuffer) * 4);

		secBufferDesc.ulVersion = SECBUFFER_VERSION;
		secBufferDesc.cBuffers = 4;
		secBufferDesc.pBuffers = secBuffers;

		secBuffers[0].BufferType = SECBUFFER_DATA;
		secBuffers[0].pvBuffer = read_buf;
		secBuffers[0].cbBuffer = bytes_read;
		secBuffers[1].BufferType = SECBUFFER_EMPTY;
		secBuffers[2].BufferType = SECBUFFER_EMPTY;
		secBuffers[3].BufferType = SECBUFFER_EMPTY;

		secStatus = DecryptMessage(ctxtHandle, &secBufferDesc, 0, NULL);
		if (secStatus == SEC_E_OK) {
			for (i = 0; i < 4; i++) {
				if (secBuffers[i].BufferType == SECBUFFER_DATA) {
					data_index = i;
				}
			}

			if (data_index == -1) {
				free(read_buf);
				return -1;
			}

			*outlen = secBuffers[data_index].cbBuffer;
			*output = (char*)malloc(*outlen);
			if (*output == NULL)
				show_error_wait_close(cursorPosition, ERR_MSG_MEMORY_ALLOC, NULL, 0);

			memcpy(*output, secBuffers[data_index].pvBuffer, *outlen);
			free(read_buf);
		}
		else if (secStatus == SEC_E_INCOMPLETE_MESSAGE) {
			if (++try_count > 16) {
				free(read_buf);
				return -1;
			}
			goto retry_decrypt;
		}
		else
			show_error_wait_close(cursorPosition, ERR_MSG_DECRYPTMESSAGE, NULL, 0);
	}
	else
		show_error_wait_close(cursorPosition, ERR_MSG_DECRYPTMESSAGE, NULL, 0);

	return 0;
}


/*
 * Function decription:
 * - Initialize Schannel shutdown variables for AcceptSecurityContext() function.
 * Arguments:
 * - secBufferDescInput: Pointer to SecBufferDesc to initialize for AcceptSecurityContext() function input.
 * - secBufferInput: Array of 4 SecBuffer to initialize for AcceptSecurityContext() function input.
 * - secBufferDescOutput: Pointer to SecBufferDesc to initialize for AcceptSecurityContext() function output.
 * - secBufferOutput: Array of 4 SecBuffer to initialize for AcceptSecurityContext() function output.
 */

static void
tls_shutdown_init_schannel_vars(SecBufferDesc * secBufferDescInput, SecBuffer secBufferInput[4], SecBufferDesc * secBufferDescOutput, SecBuffer secBufferOutput[4]) {
	ZeroMemory(secBufferDescInput, sizeof(SecBufferDesc));
	secBufferDescInput->ulVersion = SECBUFFER_VERSION;
	secBufferDescInput->cBuffers = 4;
	secBufferDescInput->pBuffers = secBufferInput;

	ZeroMemory(secBufferInput, sizeof(SecBuffer) * 4);
	secBufferInput[0].BufferType = SECBUFFER_EMPTY;
	secBufferInput[1].BufferType = SECBUFFER_EMPTY;
	secBufferInput[2].BufferType = SECBUFFER_EMPTY;
	secBufferInput[3].BufferType = SECBUFFER_EMPTY;

	ZeroMemory(secBufferDescOutput, sizeof(SecBufferDesc));
	secBufferDescOutput->ulVersion = SECBUFFER_VERSION;
	secBufferDescOutput->cBuffers = 4;
	secBufferDescOutput->pBuffers = secBufferOutput;

	ZeroMemory(secBufferOutput, sizeof(SecBuffer) * 4);
	secBufferOutput[0].BufferType = SECBUFFER_EMPTY;
	secBufferOutput[1].BufferType = SECBUFFER_EMPTY;
	secBufferOutput[2].BufferType = SECBUFFER_EMPTY;
	secBufferOutput[3].BufferType = SECBUFFER_EMPTY;

	return;
}


/*
 * Function description:
 * - Shutdown Schannel connection with client and close client socket.
 * Arguments:
 * - ctxtHandle: Security context handle used in ApplyControlToken() and AcceptSecurityContext() functions.
 * - credHandle: Credential handle used in AcceptSecurityContext() function.
 * - s_clt: Client socket to close at the end of function.
 */

void tls_shutdown(CtxtHandle* ctxtHandle, CredHandle* credHandle, SOCKET s_clt) {
	SecBufferDesc bufferDesc;
	SecBuffer secBuffer[1];
	SecBuffer secBufferInput[4];
	SecBuffer secBufferOutput[4];
	DWORD shutdownToken = SCHANNEL_SHUTDOWN;
	SecBufferDesc secBufferDescInput;
	SecBufferDesc secBufferDescOutput;
	ULONG fContextAttr = ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM | ASC_REQ_EXTENDED_ERROR | ASC_REQ_REPLAY_DETECT | ASC_REQ_CONFIDENTIALITY;
	ULONG contextAttr = 0;
	SECURITY_STATUS ret;
	int i;

	ZeroMemory(&bufferDesc, sizeof(SecBufferDesc));
	ZeroMemory(&secBuffer, sizeof(SecBuffer) * 1);

	secBuffer[0].BufferType = SECBUFFER_TOKEN;
	secBuffer[0].cbBuffer = sizeof(shutdownToken);
	secBuffer[0].pvBuffer = &shutdownToken;

	bufferDesc.ulVersion = SECBUFFER_VERSION;
	bufferDesc.cBuffers = 1;
	bufferDesc.pBuffers = &secBuffer[0];

	ret = ApplyControlToken(ctxtHandle, &bufferDesc);
	if (ret != SEC_E_OK) {
		closesocket(s_clt);
		return;
	}

	tls_shutdown_init_schannel_vars(&secBufferDescInput, secBufferInput, &secBufferDescOutput, secBufferOutput);

	ret = AcceptSecurityContext(credHandle, ctxtHandle, &secBufferDescInput, fContextAttr, 0,
		ctxtHandle, &secBufferDescOutput, &contextAttr, NULL);
	if (ret != SEC_E_OK) {
		closesocket(s_clt);
		return;
	}

	for (i = 0; i < 4; i++)
		if (secBufferOutput[i].BufferType == SECBUFFER_DATA)
			break;

	if (i < 4) {
		send(s_clt, secBufferOutput[i].pvBuffer, secBufferOutput[i].cbBuffer, 0);
		FreeContextBuffer(secBufferOutput[i].pvBuffer);
	}

	/* DeleteSecurityContext(ctxtHandle); FreeCredentialHandle(credHandle); */
	closesocket(s_clt);

	return;
}

/*
 * Function description:
 * - Perform TLS handshake with client and establish Schannel connection.
 * Arguments:
 * - credHandle: Credential handle used in AcceptSecurityContext() function.
 * - s_clt: Client socket to perform handshake with.
 * - secBufferDescInput: Pointer to SecBufferDesc to initialize for AcceptSecurityContext() function input.
 * - secBufferIn: Array of 2 SecBuffer to initialize for AcceptSecurityContext() function input.
 * - fContextAttr: Pointer to unsigned long to initialize for AcceptSecurityContext() function input and updated with output context attributes.
 * - ctxNewHandle: Pointer to CtxtHandle to initialize for AcceptSecurityContext() function output.
 * - secBufferDescOutput: Pointer to SecBufferDesc to initialize for AcceptSecurityContext() function output.
 * Return value:
 * - 0: Success
 * - -1: Failure. It can be caused by AcceptSecurityContext() failure, recv() failure or if recv() return 0 (connection closed by client).
 * 
 */
static int
tls_handshake(CredHandle* credHandle, SOCKET s_clt, SecBufferDesc* secBufferDescInput, SecBuffer secBufferIn[2], unsigned long* fContextAttr,
	CtxtHandle* ctxNewHandle, SecBufferDesc* secBufferDescOutput) {
	SECURITY_STATUS err;

	err = AcceptSecurityContext(credHandle, NULL, secBufferDescInput, *fContextAttr, 0,
		ctxNewHandle, secBufferDescOutput, fContextAttr, NULL);

	if (err != SEC_E_OK && err != SEC_I_CONTINUE_NEEDED)
		return -1;

	send(s_clt, (char*)secBufferDescOutput->pBuffers[0].pvBuffer, secBufferDescOutput->pBuffers[0].cbBuffer, 0);
	FreeContextBuffer(secBufferDescOutput->pBuffers[0].pvBuffer);

	secBufferIn[0].cbBuffer = recv(s_clt, secBufferIn[0].pvBuffer, 4096, 0);
	if (secBufferIn[0].cbBuffer <= 0)
		return -1;

	err = AcceptSecurityContext(credHandle, ctxNewHandle, secBufferDescInput, *fContextAttr, 0, ctxNewHandle, secBufferDescOutput, fContextAttr, NULL);
	if (err == SEC_E_OK) {
		send(s_clt, (char*)secBufferDescOutput->pBuffers[0].pvBuffer, secBufferDescOutput->pBuffers[0].cbBuffer, 0);
		FreeContextBuffer(secBufferDescOutput->pBuffers[0].pvBuffer);
	}
	else {
		return -1;
	}

	return 0;
}

/*
 * Functon description:
 * - Initialize Schannel variables for AcceptSecurityContext() function in acceptSecure() function.
 * Arguments:
 * - ctxNewHandle: Pointer to CtxtHandle to initialize for AcceptSecurityContext() function output.
 * - ctxNewHandle2: Pointer to CtxtHandle to initialize for AcceptSecurityContext() function output in case
                    of second AcceptSecurityContext() call in acceptSecure() function.
 * - BufferIn1: Buffer to initialize for AcceptSecurityContext() function input. It will be used in secBufferIn[0].pvBuffer.
 * - BufferIn2: Buffer to initialize for AcceptSecurityContext() function input in case of second AcceptSecurityContext()
 *              call in acceptSecure() function. It will be used in secBufferIn[1].pvBuffer.
 * - SecBufferIn: SecBuffer buffers to initialize for AcceptSecurityContext().
 * - SecBufferIn2: SecBuffer
 * 
 */
void acceptSecure_init_schannel_vars(CtxtHandle* ctxNewHandle, char BufferIn1[4096], char BufferIn2[4096],
							SecBuffer secBufferIn[2], SecBuffer secBufferOut[3],
							SecBufferDesc *secBufferDescInput, SecBufferDesc *secBufferDescOutput,
							struct sockaddr_in *sin_clt, int *sinclt_len) {
	ZeroMemory(ctxNewHandle, sizeof(CtxtHandle));

	ZeroMemory(BufferIn1, 4096);
	ZeroMemory(BufferIn2, 4096);

	ZeroMemory(secBufferIn, sizeof(SecBuffer) * 2);
	secBufferDescInput->ulVersion = SECBUFFER_VERSION;
	secBufferDescInput->cBuffers = 2;
	secBufferDescInput->pBuffers = secBufferIn;

	secBufferIn[0].BufferType = SECBUFFER_TOKEN;
	secBufferIn[0].pvBuffer = BufferIn1;
	secBufferIn[1].cbBuffer = 4096;
	secBufferIn[1].BufferType = SECBUFFER_EMPTY;
	secBufferIn[1].pvBuffer = BufferIn2;

	secBufferDescOutput->ulVersion = SECBUFFER_VERSION;
	secBufferDescOutput->cBuffers = 3;
	secBufferDescOutput->pBuffers = secBufferOut;
	ZeroMemory(secBufferOut, sizeof(SecBuffer) * 3);

	ZeroMemory(sin_clt, sizeof(struct sockaddr_in));
	*sinclt_len = sizeof(struct sockaddr_in);

	return;
}

SOCKET acceptSecure(SOCKET s, CredHandle* credHandle, CtxtHandle* ctxtHandle, char ipaddr_httpsclt[16]) {
	SOCKET s_clt;
	struct sockaddr_in sin_clt;
	int sinclt_len = sizeof(struct sockaddr_in);
	CtxtHandle ctxNewHandle;
	ULONG fContextAttr = ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM | ASC_REQ_EXTENDED_ERROR | ASC_REQ_REPLAY_DETECT | ASC_REQ_CONFIDENTIALITY;
	char BufferIn1[4096];
	char BufferIn2[4096];
	SecBufferDesc secBufferDescInput;
	SecBufferDesc secBufferDescOutput;
	SecBuffer secBufferIn[2];
	SecBuffer secBufferOut[3];
	int ret;

	for (;;) {
		acceptSecure_init_schannel_vars(&ctxNewHandle, BufferIn1, BufferIn2, secBufferIn,
										secBufferOut, &secBufferDescInput, &secBufferDescOutput,
										&sin_clt, &sinclt_len);

		s_clt = accept(s, (struct sockaddr*)&sin_clt, &sinclt_len);

		if (s_clt == INVALID_SOCKET)
			continue;

		g_tls_sclt = s_clt;

		ZeroMemory(ipaddr_httpsclt, 16);
		inet_ntop(AF_INET, &sin_clt.sin_addr, ipaddr_httpsclt, 16);

		ret = recv(s_clt, BufferIn1, 4096, 0);
		if (ret <= 0) {
			closesocket(s_clt);
			continue;
		}

		secBufferIn[0].cbBuffer = (unsigned long)ret;

		if (tls_handshake(credHandle, s_clt, &secBufferDescInput, secBufferIn, &fContextAttr, &ctxNewHandle, &secBufferDescOutput) < 0) {
			closesocket(s_clt);
			continue;
		}
		else
			break;
	}

	memcpy(ctxtHandle, &ctxNewHandle, sizeof(CtxtHandle));

	return s_clt;
}
