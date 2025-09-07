#define _WIN32_WINNT 0x0601

#include <Windows.h>
#include <winternl.h>
#include <wincrypt.h>
#include <stdio.h>

#define SCHANNEL_USE_BLACKLISTS

#include <schannel.h>

#define SECURITY_WIN32

#include <sspi.h>

extern FILE *fp_httpslog;


int tls_recv(int s_clt, CtxtHandle *ctxtHandle, char BufferIn[2048], int *bytereceived) {
	SecBufferDesc secBufferDescInput;	
	int i;
	SecBuffer secBufferIn[4];
	char *p;

	ZeroMemory(&secBufferDescInput, sizeof(SecBufferDesc));
	secBufferDescInput.ulVersion = SECBUFFER_VERSION;
	secBufferDescInput.cBuffers = 4;
	secBufferDescInput.pBuffers = secBufferIn;

	ZeroMemory(&secBufferIn, sizeof(SecBuffer) * 4);

	ZeroMemory(BufferIn, 2048);
	secBufferIn[0].cbBuffer = recv(s_clt, BufferIn, 2047, 0);
	if (secBufferIn[0].cbBuffer <= 0)
		return 1;

	secBufferIn[0].BufferType = SECBUFFER_DATA;
	secBufferIn[0].pvBuffer = BufferIn;
	secBufferIn[1].BufferType = SECBUFFER_EMPTY;
	secBufferIn[2].BufferType = SECBUFFER_EMPTY;
	secBufferIn[3].BufferType = SECBUFFER_EMPTY;

	ret = DecryptMessage(&ctxtHandle, &secBufferDescInput, 0 , 0);
	fprintf(g_fphttpslog, "DecryptMessage: %i\n", ret);
	fflush(g_fphttpslog);

	for (i = 0; i < 4; i++)
		if (secBufferIn[i].BufferType == SECBUFFER_DATA)
			break;

	p = secBufferIn[i].pvBuffer;
	*(p + secBufferIn[i].cbBuffer) = '\0';

	*bytereceived = strlen(p);

	return 0;
}


int acceptSecure(int s, CredHandle *credHandle, CtxtHandle *ctxtHandle) {
	int s_clt;
	int received;
	ULONG err;
	struct sockaddr_in sin_clt;
	int sinclt_len = sizeof(struct sockaddr_in);
	CtxtHandle ctxNewHandle, ctxNewHandle2;
	int i;
	ULONG fContextAttr = ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_STREAM | ASC_REQ_EXTENDED_ERROR | ASC_REQ_REPLAY_DETECT | ASC_REQ_CONFIDENTIALITY;
	ULONG contextAttr = 0;
	TimeStamp tsExpiry;
	char BufferIn1[2048];
	char BufferIn2[2048];
	char BufferOut1[2048];
	char *p;
	SecBufferDesc secBufferDescInput;
	SecBufferDesc secBufferDescInput2;
	SecBufferDesc secBufferDescOutput;
	SecBuffer secBufferIn[2];
	SecBuffer secBufferIn2[4];
	SecBuffer secBufferOut[3];

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
		} else {
			continue;
		}


	}

	*ctxtHandle = ctxNewHandle;

	return s_clt;
}
