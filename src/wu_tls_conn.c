#define _WIN32_WINNT 0x0601

#include <WinSock2.h>
#include <Windows.h>
#include <winternl.h>
#include <wincrypt.h>
#include <stdio.h>

#define SCHANNEL_USE_BLACKLISTS

#include <schannel.h>

#define SECURITY_WIN32

#include <sspi.h>

#include "wu_msg.h"

extern FILE* g_fphttpslog;
extern HANDLE g_hConsoleOutput;
extern SecPkgContext_StreamSizes context_sizes;
extern char* encryptBuffer;
extern char* decryptBuffer[10];
extern unsigned int cb_buffer_extra;
extern char* buffer_extra;

int tls_send(int s_clt, CtxtHandle* ctxtHandle, char* message, unsigned int message_size, COORD cursorPosition) {
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
	if (ret != 0) {
		INPUT_RECORD inRec;
		DWORD read;

		cursorPosition.Y++;
		SetConsoleCursorPosition(g_hConsoleOutput, cursorPosition);
		write_info_in_console(ERR_MSG_ENCRYPTMESSAGE, NULL, ret);

		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
	}

	if (send(s_clt, encryptBuffer, secBufferOut[0].cbBuffer + secBufferOut[1].cbBuffer + secBufferOut[2].cbBuffer, 0) < 0)
		return -1;

	return 0;
}


int tls_recv(int s_clt, CtxtHandle* ctxtHandle, char** output, unsigned int* size_output, COORD* cursorPosition) {
	SecBufferDesc secBufferDesc;
	SecBuffer secBufferIn[4];
	int received[10];
	int i, j, ret, missing_size;
	int total_size = 0;

	ZeroMemory(&secBufferDesc, sizeof(SecBufferDesc));
	ZeroMemory(&secBufferIn[0], sizeof(SecBuffer) * 4);

	secBufferDesc.cBuffers = 4;
	secBufferDesc.pBuffers = &secBufferIn[0];
	secBufferDesc.ulVersion = SECBUFFER_VERSION;

	secBufferIn[0].BufferType = SECBUFFER_DATA;
	secBufferIn[1].BufferType = SECBUFFER_EMPTY;
	secBufferIn[2].BufferType = SECBUFFER_EMPTY;
	secBufferIn[3].BufferType = SECBUFFER_EMPTY;

	for (i = 1; i < 10; i++) {
		if (decryptBuffer[i] != NULL) {
			free(decryptBuffer[i]);
			decryptBuffer[i] = NULL;
		}
	}

	for (i = 0; i < 10; i++)
		received[i] = 0;

	ZeroMemory(decryptBuffer[0], 2000);

	if (buffer_extra != NULL) {
		memcpy(decryptBuffer[0], buffer_extra, cb_buffer_extra);
		received[0] = recv(s_clt, decryptBuffer[0] + cb_buffer_extra, 2000 - cb_buffer_extra, 0);

		if (received[0] <= 0)
			return -1;

		secBufferIn[0].pvBuffer = decryptBuffer[0];
		secBufferIn[0].cbBuffer = received[0] + cb_buffer_extra;

		free(buffer_extra);
		buffer_extra = NULL;
	}
	else {
		received[0] = recv(s_clt, decryptBuffer[0], 2000, 0);

		if (received[0] <= 0)
			return -1;

		secBufferIn[0].pvBuffer = decryptBuffer[0];
		secBufferIn[0].cbBuffer = received[0];

		cb_buffer_extra = 0;
	}

	ret = DecryptMessage(ctxtHandle, &secBufferDesc, 0, NULL);
	if (ret == SEC_E_OK) {
		for (i = 0; i < 4; i++) {
			if (secBufferIn[i].BufferType == SECBUFFER_EXTRA) {
				buffer_extra = malloc(secBufferIn[i].cbBuffer);
				memcpy(buffer_extra, secBufferIn[i].pvBuffer, secBufferIn[i].cbBuffer);
				cb_buffer_extra = secBufferIn[i].cbBuffer;
				break;
			}
		}
		if (i == 4) {
			if (buffer_extra != NULL) {
				free(buffer_extra);
				buffer_extra = NULL;
				cb_buffer_extra = 0;
			}
		}

		for (i = 0; i < 4; i++)
			if (secBufferIn[i].BufferType == SECBUFFER_DATA)
				break;

		*output = secBufferIn[i].pvBuffer;
		*size_output = secBufferIn[i].cbBuffer;
	}
	else if (ret == SEC_E_INCOMPLETE_MESSAGE) {
		for (i = 1; i < 10; i++) {
			for (j = 0; j < 4; j++) {
				if (secBufferIn[j].BufferType == SECBUFFER_MISSING)
					break;
			}
			missing_size = secBufferIn[j].cbBuffer;

			total_size = 0;
			total_size += cb_buffer_extra;
			for (j = 0; j < 10 && received[j] != 0; j++)
				total_size += received[j];

			decryptBuffer[i] = malloc(total_size + missing_size);
			memcpy(decryptBuffer[i], decryptBuffer[i - 1], total_size);
			received[i] = recv(s_clt, decryptBuffer[i] + total_size, missing_size, MSG_WAITALL);
			if (received[i] <= 0)
				return -1;

			total_size += missing_size;

			secBufferIn[0].BufferType = SECBUFFER_DATA;
			secBufferIn[0].pvBuffer = decryptBuffer[i];
			secBufferIn[0].cbBuffer = total_size;
			secBufferIn[1].BufferType = SECBUFFER_EMPTY;
			secBufferIn[2].BufferType = SECBUFFER_EMPTY;
			secBufferIn[3].BufferType = SECBUFFER_EMPTY;

			ret = DecryptMessage(ctxtHandle, &secBufferDesc, 0, NULL);
			if (ret == SEC_E_OK) {
				if (buffer_extra != NULL) {
					free(buffer_extra);
					buffer_extra = NULL;
					cb_buffer_extra = 0;
				}
				/* Fix me (i = j) */
				for (i = 0; i < 4; i++)
					if (secBufferIn[i].BufferType == SECBUFFER_DATA)
						break;
				*output = secBufferIn[i].pvBuffer;
				*size_output = secBufferIn[i].cbBuffer;
				break;
			}
			else if (ret = SEC_E_INCOMPLETE_MESSAGE) {
				continue;
			}
			else {

				INPUT_RECORD inRec;
				DWORD read;

				SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
				write_info_in_console(ERR_MSG_DECRYPTMESSAGE, NULL, ret);

				// while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));

				return -1;
			}
		}
		if (i == 10) {
			INPUT_RECORD inRec;
			DWORD read;

			cursorPosition->Y++;
			SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
			write_info_in_console(ERR_MSG_CONGESTED_NETWORK, NULL, 0);

			while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));

			return -1;
		}
	}
	else {
		INPUT_RECORD inRec;
		DWORD read;

		SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
		write_info_in_console(ERR_MSG_DECRYPTMESSAGE, NULL, ret);

		// while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));

		return -1;

	}

	return 0;
}

void tls_shutdown(CtxtHandle* ctxtHandle, CredHandle* credHandle, int s_clt) {
	SecBufferDesc bufferDesc;
	SecBuffer secBuffer[1];
	SecBuffer secBufferInput[4];
	SecBuffer secBufferOutput[4];
	DWORD shutdownToken = SCHANNEL_SHUTDOWN;
	SecBufferDesc secBufferDescInput;
	SecBufferDesc secBufferDescOutput;
	ULONG fContextAttr = ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM | ASC_REQ_EXTENDED_ERROR | ASC_REQ_REPLAY_DETECT | ASC_REQ_CONFIDENTIALITY;
	ULONG contextAttr = 0;
	int i;

	ZeroMemory(&bufferDesc, sizeof(SecBufferDesc));
	ZeroMemory(&secBuffer, sizeof(SecBuffer) * 1);

	secBuffer[0].BufferType = SECBUFFER_TOKEN;
	secBuffer[0].cbBuffer = sizeof(shutdownToken);
	secBuffer[0].pvBuffer = &shutdownToken;

	bufferDesc.ulVersion = SECBUFFER_VERSION;
	bufferDesc.cBuffers = 1;
	bufferDesc.pBuffers = &secBuffer[0];

	ApplyControlToken(ctxtHandle, &bufferDesc);

	ZeroMemory(&secBufferDescInput, sizeof(SecBufferDesc));
	secBufferDescInput.ulVersion = SECBUFFER_VERSION;
	secBufferDescInput.cBuffers = 4;
	secBufferDescInput.pBuffers = secBufferInput;

	ZeroMemory(&secBufferInput, sizeof(SecBuffer) * 4);
	secBufferInput[0].BufferType = SECBUFFER_EMPTY;
	secBufferInput[1].BufferType = SECBUFFER_EMPTY;
	secBufferInput[2].BufferType = SECBUFFER_EMPTY;
	secBufferInput[3].BufferType = SECBUFFER_EMPTY;

	ZeroMemory(&secBufferDescOutput, sizeof(SecBufferDesc));
	secBufferDescOutput.ulVersion = SECBUFFER_VERSION;
	secBufferDescOutput.cBuffers = 4;
	secBufferDescOutput.pBuffers = secBufferOutput;

	ZeroMemory(&secBufferOutput, sizeof(SecBuffer) * 4);
	secBufferOutput[0].BufferType = SECBUFFER_EMPTY;
	secBufferOutput[1].BufferType = SECBUFFER_EMPTY;
	secBufferOutput[2].BufferType = SECBUFFER_EMPTY;
	secBufferOutput[3].BufferType = SECBUFFER_EMPTY;

	AcceptSecurityContext(credHandle, ctxtHandle, &secBufferDescInput, fContextAttr, 0,
		ctxtHandle, &secBufferDescOutput, &contextAttr, NULL);
	for (i = 4; i < 4; i++)
		if (secBufferOutput[i].BufferType == SECBUFFER_DATA)
			break;

	if (i < 4) {
		send(s_clt, secBufferOutput[i].pvBuffer, secBufferOutput[i].cbBuffer, 0);
		FreeContextBuffer(secBufferOutput[i].pvBuffer);
	}

	DeleteSecurityContext(ctxtHandle);
	closesocket(s_clt);

	return;
}


int acceptSecure(int s, CredHandle* credHandle, CtxtHandle* ctxtHandle, char ipaddr_httpsclt[16]) {
	int s_clt;
	ULONG err;
	struct sockaddr_in sin_clt;
	int sinclt_len = sizeof(struct sockaddr_in);
	CtxtHandle ctxNewHandle, ctxNewHandle2;
	ULONG fContextAttr = ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM | ASC_REQ_EXTENDED_ERROR | ASC_REQ_REPLAY_DETECT | ASC_REQ_CONFIDENTIALITY;
	ULONG contextAttr = 0;
	char BufferIn1[2048];
	char BufferIn2[2048];
	char BufferOut1[2048];
	SecBufferDesc secBufferDescInput;
	SecBufferDesc secBufferDescInput2;
	SecBufferDesc secBufferDescOutput;
	SecBuffer secBufferIn[2];
	SecBuffer secBufferIn2[4];
	SecBuffer secBufferOut[3];
	int timeout = 2000;

	ZeroMemory(&ctxNewHandle, sizeof(CtxtHandle));
	ZeroMemory(&ctxNewHandle2, sizeof(CtxtHandle));

	ZeroMemory(BufferIn1, 2048);
	ZeroMemory(BufferIn2, 2048);

	ZeroMemory(secBufferIn, sizeof(SecBuffer) * 2);
	secBufferDescInput.ulVersion = SECBUFFER_VERSION;
	secBufferDescInput.cBuffers = 2;
	secBufferDescInput.pBuffers = secBufferIn;

	secBufferIn[0].BufferType = SECBUFFER_TOKEN;
	secBufferIn[0].pvBuffer = BufferIn1;
	secBufferIn[1].cbBuffer = 2048;
	secBufferIn[1].BufferType = SECBUFFER_EMPTY;
	secBufferIn[1].pvBuffer = BufferIn2;

	ZeroMemory(secBufferIn2, sizeof(SecBuffer) * 4);
	secBufferDescInput2.ulVersion = SECBUFFER_VERSION;
	secBufferDescInput2.cBuffers = 4;
	secBufferDescInput2.pBuffers = secBufferIn2;

	secBufferDescOutput.ulVersion = SECBUFFER_VERSION;
	secBufferDescOutput.cBuffers = 3;
	secBufferDescOutput.pBuffers = secBufferOut;
	ZeroMemory(&secBufferOut, sizeof(SecBuffer) * 3);

	for (;;) {
		ZeroMemory(BufferIn1, 2048);
		ZeroMemory(BufferIn2, 2048);
		ZeroMemory(&sin_clt, sizeof(struct sockaddr_in));

		sinclt_len = sizeof(struct sockaddr_in);
		s_clt = accept(s, (struct sockaddr*)&sin_clt, &sinclt_len);

		setsockopt(s_clt, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

		strcpy_s(ipaddr_httpsclt, 16, inet_ntoa(sin_clt.sin_addr));

		secBufferIn[0].cbBuffer = recv(s_clt, BufferIn1, 2048, 0);

		if (secBufferIn[0].cbBuffer <= 0)
			continue;

		err = AcceptSecurityContext(credHandle, NULL, &secBufferDescInput, fContextAttr, 0,
			&ctxNewHandle, &secBufferDescOutput, &contextAttr, NULL);

		if (err != SEC_E_OK && err != SEC_I_CONTINUE_NEEDED)
			continue;
		// printf("AcceptSecurityContext failed: %x\n", err);


		send(s_clt, (char*)secBufferDescOutput.pBuffers[0].pvBuffer, secBufferDescOutput.pBuffers[0].cbBuffer, 0);
		FreeContextBuffer(secBufferDescOutput.pBuffers[0].pvBuffer);

		secBufferIn[0].cbBuffer = recv(s_clt, BufferIn1, 2048, 0);
		if (secBufferIn[0].cbBuffer <= 0)
			continue;

		err = AcceptSecurityContext(credHandle, &ctxNewHandle, &secBufferDescInput, fContextAttr, 0, &ctxNewHandle, &secBufferDescOutput, &contextAttr, NULL);
		if (err == SEC_E_OK) {
			send(s_clt, (char*)secBufferDescOutput.pBuffers[0].pvBuffer, secBufferDescOutput.pBuffers[0].cbBuffer, 0);
			FreeContextBuffer(secBufferDescOutput.pBuffers[0].pvBuffer);
			break;
		}
		else {
			continue;
		}


	}

	memcpy(ctxtHandle, &ctxNewHandle, sizeof(CtxtHandle));

	return s_clt;
}
