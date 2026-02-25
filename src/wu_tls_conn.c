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

int tls_recv_start(int s, SecBuffer secBuffers[4], char *read_buf, int *bytes_read) {
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

int tls_recv_add_data_to_extra(int s, SecBuffer secBuffers[4], char *read_buf, int* bytes_read) {
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

int tls_recv(CtxtHandle* ctxtHandle, int s, char** output, unsigned int* outlen, COORD* cursorPosition) {
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
	if (read_buf == NULL) {
		INPUT_RECORD inRec;
		DWORD read;
		cursorPosition->Y++;
		SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
		write_info_in_console(ERR_MSG_MEMORY_ALLOC, NULL, 0);
		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
		return -1;
	}
	
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
	} else if(tls_recv_start(s, secBuffers, read_buf, &bytes_read) < 0)
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
		if (*output == NULL) {
			INPUT_RECORD inRec;
			DWORD read;
			cursorPosition->Y++;
			SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
			write_info_in_console(ERR_MSG_MEMORY_ALLOC, NULL, 0);
			while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
			return -1;
		}

		memcpy(*output, secBuffers[data_index].pvBuffer, *outlen);

		if (extra_index >= 0) {
			extra_buf = (char*)malloc(secBuffers[extra_index].cbBuffer);
			extra_len = secBuffers[extra_index].cbBuffer;
			memcpy(extra_buf, secBuffers[extra_index].pvBuffer, extra_len);
		}

		free(read_buf);
	} else if (secStatus == SEC_E_INCOMPLETE_MESSAGE) {
		int i;
		int missing_req;
		int missing_index = -1;
		int data_index = -1;
		char* tmp;


retry_decrypt:
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
			INPUT_RECORD inRec;
			DWORD read;
			cursorPosition->Y++;
			SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
			write_info_in_console(ERR_MSG_MEMORY_ALLOC, NULL, 0);
			while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
			return -1;
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
			if (*output == NULL) {
				INPUT_RECORD inRec;
				DWORD read;
				cursorPosition->Y++;
				SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
				write_info_in_console(ERR_MSG_MEMORY_ALLOC, NULL, 0);
				while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
				return -1;
			}

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
		else {
			INPUT_RECORD inRec;
			DWORD read;

			SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
			write_info_in_console(ERR_MSG_DECRYPTMESSAGE, NULL, ret);
			while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));
			return -1;
		}
	}
	else {
		INPUT_RECORD inRec;
		DWORD read;

		SetConsoleCursorPosition(g_hConsoleOutput, *cursorPosition);
		write_info_in_console(ERR_MSG_DECRYPTMESSAGE, NULL, ret);
		while (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &inRec, sizeof(INPUT_RECORD), &read));

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
	for (i = 0; i < 4; i++)
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
	char BufferIn1[4096];
	char BufferIn2[4096];
	char BufferOut1[4096];
	SecBufferDesc secBufferDescInput;
	SecBufferDesc secBufferDescInput2;
	SecBufferDesc secBufferDescOutput;
	SecBuffer secBufferIn[2];
	SecBuffer secBufferIn2[4];
	SecBuffer secBufferOut[3];

	ZeroMemory(&ctxNewHandle, sizeof(CtxtHandle));
	ZeroMemory(&ctxNewHandle2, sizeof(CtxtHandle));

	ZeroMemory(BufferIn1, 4096);
	ZeroMemory(BufferIn2, 4096);

	ZeroMemory(secBufferIn, sizeof(SecBuffer) * 2);
	secBufferDescInput.ulVersion = SECBUFFER_VERSION;
	secBufferDescInput.cBuffers = 2;
	secBufferDescInput.pBuffers = secBufferIn;

	secBufferIn[0].BufferType = SECBUFFER_TOKEN;
	secBufferIn[0].pvBuffer = BufferIn1;
	secBufferIn[1].cbBuffer = 4096;
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
		ZeroMemory(BufferIn1, 4096);
		ZeroMemory(BufferIn2, 4096);
		ZeroMemory(&sin_clt, sizeof(struct sockaddr_in));

		sinclt_len = sizeof(struct sockaddr_in);
		s_clt = accept(s, (struct sockaddr*)&sin_clt, &sinclt_len);

		strcpy_s(ipaddr_httpsclt, 16, inet_ntoa(sin_clt.sin_addr));

		secBufferIn[0].cbBuffer = recv(s_clt, BufferIn1, 4096, 0);

		if (secBufferIn[0].cbBuffer <= 0)
			continue;

		err = AcceptSecurityContext(credHandle, NULL, &secBufferDescInput, fContextAttr, 0,
			&ctxNewHandle, &secBufferDescOutput, &contextAttr, NULL);

		if (err != SEC_E_OK && err != SEC_I_CONTINUE_NEEDED)
			continue;

		send(s_clt, (char*)secBufferDescOutput.pBuffers[0].pvBuffer, secBufferDescOutput.pBuffers[0].cbBuffer, 0);
		FreeContextBuffer(secBufferDescOutput.pBuffers[0].pvBuffer);

		secBufferIn[0].cbBuffer = recv(s_clt, BufferIn1, 4096, 0);
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
