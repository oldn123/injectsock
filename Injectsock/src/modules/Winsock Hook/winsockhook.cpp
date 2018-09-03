#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <Iphlpapi.h>
#include <Assert.h>
#include <process.h>
#include <assert.h>
#include "HookApi.h"
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <vector>
#include <queue>
#include <map>
using namespace std;
#include "../../eikasia.h"
#include "../../../rtmp/PacketOrderer.h"

struct DataNode{
	int nIdx;
	int nSize;
	char * pBuf;
	DataNode * pNext;
};

struct DataFrame{
	DataNode * pNode;
	int nTotalSize;
	int nLoop;
	int nGetSize;
	DataNode * pPopNode;
};

int g_nSleepStep = 200;
int g_msgCnt = 0;
CHookApi_Jmp g_hj;
map<SOCKET, queue<DataFrame>> g_dataQueueMap;

map<SOCKET, int> g_timerMap;

typedef int (WINAPI *PCONNECT)(SOCKET s, const struct sockaddr *address, int namelen);
typedef int (WINAPI *PGETHOSTBYNAME)(const char *name);
typedef int (WINAPI *PSEND)(SOCKET s, const char* buf, int len, int flags);
typedef int (WINAPI *PRECV)(SOCKET s, const char* buf, int len, int flags);
typedef int (WINAPI *PSENDTO) (SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen);
typedef int (WINAPI *PWSASEND)(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
typedef int (WINAPI *PWSARECV)(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
typedef SOCKET (WINAPI *PSOCKET) (int af, int type, int protocol);
typedef int (WINAPI *PCLOSESOCKET) (SOCKET s);

/*
typedef int (WINAPI *PSPRINTF) (char *_Dest, const char *_Format, va_list ap);
typedef time_t (WINAPI *PTIME)(time_t *);

typedef void (WINAPI *PGRFREADER) (char *grf_file, int grf_num);
typedef void (__fastcall *PHOTKEY) (int key_num);
typedef int (WINAPI *PMakeWindow) (int window);
typedef int (WINAPI *PLoginWindow) (int a, int b);
*/
PSOCKET	OrigSocket = socket;
PCONNECT OrigConnect = connect;
PSEND OrigSend = send;
PSENDTO	OrigSendTo = sendto;
PRECV OrigRecv;
PWSASEND OrigWSASend;
PWSARECV OrigWSARecv;
PGETHOSTBYNAME OrigGethost;
PCLOSESOCKET OrigClosesocket;



/*
PGRFREADER OrigGrfReader;
PHOTKEY OrigHotKey;

PMakeWindow OrigMakeWindow;
int WINAPI MyMakeWindow(int window) {
	return OrigMakeWindow(window);
}


PLoginWindow OrigLoginWindow;
int WINAPI MyLoginWindow(int a, int b) {
	return OrigLoginWindow(a, b);
}

PSPRINTF OrigSprintf;
PTIME OrigTime;*/

SYSTEMTIME st;

//===========================Socket===========================
SOCKET WINAPI MySocket(int af, int type, int protocol) {
	return OrigSocket(af, type, protocol);
}

int WINAPI MyClosesocket (SOCKET s) {
	g_hj.SetHookOff("closesocket");
	int nret = closesocket(s);
	g_hj.SetHookOn("closesocket");
	return nret;
}
/*
int WINAPI MySprintf (char *_Dest, const char *_Format, ...) {

	va_list ap;
	int ret;

	va_start(ap,_Format);
	ret = vsprintf(_Dest, _Format, ap);
	va_end(ap);

	return ret;
}

void test(char *grf_file, int grf_num) {
	char msgbuf[255];
	const char* grf_files[] = { "data.grf", "rdata.grf" };
	int i = 0, e =0;

	for(i=0; i <= ARRAYLENGTH(grf_files); i++) {
		if(strcmp(grf_files[0],grf_file) == 0)
			break;
		else
			e++;
	}
			FILE *fp = fopen("log.txt", "ab");
			sprintf(msgbuf, "%d - %s", grf_num, grf_file);
			fwrite(msgbuf, strlen(msgbuf), 1, fp);
			fputc(0x0d, fp);
			fputc(0x0a, fp);
			fclose(fp);

	if(e > ARRAYLENGTH(grf_files))
		exit(0);
}

void WINAPI MyGrfReader(char *grf_file, int grf_num) {
	OrigGrfReader(grf_file, grf_num);
	//test(grf_file, grf_num);
}

void __fastcall MyHotKey(int key_num) {
	OrigHotKey(key_num);
	//test(grf_file, grf_num);
}
*/

//===========================SEND===========================
int WINAPI __stdcall MySend(SOCKET s, const char* buf, int len, int flags)
{
	//eikasia_process_send(s, buf, &len, flags); // Process the buffer before send

	char stext[80] = {0};
	sprintf(stext, "####! send: %d\n", len);
	OutputDebugStringA(stext);

	int SentBytes = OrigSend(s, buf, len, flags);
	if(SentBytes == SOCKET_ERROR) return SentBytes;

	//SentBytes = eikasia_process_send_after(s, buf, &len, flags, SentBytes); // Process the buffer after send

	return SentBytes;
}

void ReleaseDf(DataFrame & df)
{

}

void hextostr(char *ptr,unsigned char *buf,int len)
{
	if (len > 999)
	{
		len = 999;
	}
	for(int i = 0; i < len; i++)
	{
		sprintf(ptr, "%02x",buf[i]);
		ptr += 2;
	}
}

bool bNeedBreak0 = false;

//===========================RECV===========================
int WINAPI __stdcall MyRecv(SOCKET s, const char* buf, int len, int flags)
{
	int RecvedBytes = 0;
	g_hj.SetHookOff("recv");	
	RecvedBytes = recv(s, (char*)buf, len, flags);
	g_hj.SetHookOn("recv");

	if(RecvedBytes == SOCKET_ERROR) return RecvedBytes;

	if (bNeedBreak0)
	{
		if (RecvedBytes == 6)
		{
			char val[6] = {0};
			if (memcmp(buf, val, 6) == 0)
			{
				bNeedBreak0 = false;
				return -1;
			}	
		}
	}

	//变更时间戳
	bool bVideoMsg = len >= 8 && GetRtmpPacketType((unsigned char*)buf) == RtmpPacket::Video;
	if (bVideoMsg)
	{
		if (!g_timerMap.count(s))
		{
			g_timerMap[s] = 0;
		}

		if (g_timerMap[s] > 15 * 1000)
		{
			return RecvedBytes;
		}

		int ntime = (buf[1] << 16) | (buf[2] << 8) | buf[3];

		ntime += g_nSleepStep;
		Sleep(g_nSleepStep);
		g_timerMap[s] += g_nSleepStep;

		((char*)buf)[1] = (char)((ntime >> 16) & 0xff);
		((char*)buf)[2] = (char)((ntime >> 8) & 0xff);
		((char*)buf)[3] = (char)(ntime & 0xff);
	}
	else
	{
		bool bJson = ((buf[0] & 0xc0) >> 6) == 2 && buf[4] == '{';
		if (bJson)
		{
			char * pfind = strstr((char*)&buf[4], "{\"pack_type\":4,");
			if (pfind)
			{
				char soutput[150] = {0};
				char sTrace[100] = {0};
				int nval = *(int*)buf;
				nval &= 0x3fffffff;
				hextostr(sTrace, (unsigned char*)&nval, 4);

				sprintf(soutput, ">>> 结论  %s\n", sTrace);
				OutputDebugStringA(soutput);
				memset((void*)&buf[0], 0, len);
				bNeedBreak0 = true;
				return -1;
				return RecvedBytes;
			}

			pfind = strstr((char*)&buf[4], "{\"pack_type\":5,");
			if (pfind)
			{
				char soutput[150] = {0};
				char sTrace[100] = {0};
				int nval = *(int*)buf;
				nval &= 0x3fffffff;
				hextostr(sTrace, (unsigned char*)&nval, 4);

				sprintf(soutput, ">>> 已开局  %s\n", sTrace);
				OutputDebugStringA(soutput);
				memset((void*)&buf[0], 0, len);
				bNeedBreak0 = true;
				return -1;
				return RecvedBytes;
			}

			char soutput[150] = {0};
			strcat(soutput, ">>> ");
			memcpy(&soutput[4], &buf[4], RecvedBytes - 4 > 140 ? 140 : RecvedBytes - 4);
			strcat(soutput, "\n");
			OutputDebugStringA(soutput);
		}
// 		if (!g_timerMap.count(s))
// 		{
// 			char soutput[150] = {0};
// 			char sTrace[100] = {0};
// 			hextostr(sTrace, (unsigned char*)buf, 50);
// 
// 			sprintf(soutput, ">>> %s\n", sTrace);
// 			OutputDebugStringA(soutput);
// 		}
	}

	return RecvedBytes;
	
	
	map<SOCKET, queue<DataFrame>> & dqm = g_dataQueueMap;
	queue<DataFrame> & dnQueue = dqm[s];

	int nRecLen = RecvedBytes;
	bool bNeedRecvData = false;
	DataNode * _pdn = NULL;
	if (dnQueue.size() > 0)
	{
		DataFrame & df = dnQueue.back();
		if(df.nGetSize < df.nTotalSize)
		{
			df.nGetSize += nRecLen;
			if (df.nGetSize > df.nTotalSize)
			{
				nRecLen -= df.nGetSize - df.nTotalSize;
				assert(0);
			}
			_pdn = df.pNode;
		}	
	}

	if (_pdn)
	{
		//补充数据节点
		DataNode * pdn = NULL;
		while(1)
		{
			if(_pdn)
			{
				pdn = _pdn;
				_pdn = _pdn->pNext;
			}
			else
			{
				break;
			}
		}
		pdn->pNext = new DataNode;
		pdn->pNext->nIdx = g_msgCnt++;
		pdn->pNext->nSize = nRecLen;
		pdn->pNext->pBuf = new char[nRecLen];
		memcpy(pdn->pNext->pBuf, buf, pdn->pNext->nSize);
		pdn->pNext->pNext = NULL;	
	}
	else
	{
		bool bVideoMsg = len >= 8 && GetRtmpPacketType((unsigned char*)buf) == RtmpPacket::Video;
		if (!bVideoMsg)
		{
			return RecvedBytes;
		}
		else
		{
			DataFrame df;
			df.nLoop = 0;
			df.pNode = new DataNode;
			df.pPopNode = df.pNode;
			df.nGetSize = nRecLen;

			unsigned char totalExpected_byte0 = *(buf + 4);	
			unsigned char totalExpected_byte1 = *(buf + 5);
			unsigned char totalExpected_byte2 = *(buf + 6);

			int totalExpected = (totalExpected_byte0 << 16) | (totalExpected_byte1 << 8) | totalExpected_byte2 + 8;
			// add in extra chunk delimiters
			totalExpected += totalExpected/128;

			df.nTotalSize = totalExpected;


			df.pNode->nIdx = g_msgCnt++;
			df.pNode->nSize = nRecLen;
			df.pNode->pBuf = new char[nRecLen];
			memcpy(df.pNode->pBuf, buf, df.pNode->nSize);
			df.pNode->pNext = NULL;

			dnQueue.push(df);
		}
	}

	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

	DataNode *pdn = NULL;
	if (dnQueue.size() && g_msgCnt > 1)
	{
		DataFrame & df = dnQueue.front();
		pdn = df.pPopNode;
		df.pPopNode = df.pPopNode->pNext;
		if (df.pPopNode == NULL)
		{
			df.pPopNode = df.pNode;
			df.nLoop++;
		}
		memset((char*)buf, 0, len);
		memcpy((char*)buf, pdn->pBuf, pdn->nSize);
		RecvedBytes = pdn->nSize;

		if (df.nLoop > 1)
		{
			ReleaseDf(df);
			dnQueue.pop();
		}
	}

	char stext[80] = {0};
	sprintf(stext, "####! recv: %d\n", RecvedBytes);
	OutputDebugStringA(stext);

	//eikasia_process_recv(s, buf, &RecvedBytes, flags); // Process the recived buffer
	return RecvedBytes;
}

//===========================CONNECT===========================
int WINAPI __stdcall MyConnect(SOCKET s, const struct sockaddr *address, int namelen)
{
	char stext[80] = {0};
	sprintf(stext, "####! connect: %d\n", address->sa_data);
	OutputDebugStringA(stext);

	int errors = OrigConnect(s, address, namelen);
	if(errors == SOCKET_ERROR) return errors;
	return errors;
}

//===========================WSASEND===========================
int WINAPI __stdcall MyWSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	char stext[80] = {0};
	sprintf(stext, "####! wsaSend: %d\n", dwBufferCount );
	OutputDebugStringA(stext);

	int Errors = OrigWSASend(s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, lpFlags, lpOverlapped, lpCompletionRoutine);
	if(Errors == SOCKET_ERROR) return Errors;
	return Errors;
}

//===========================WSARECV===========================
int WINAPI __stdcall MyWSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
	int Errors = OrigWSARecv(s, lpBuffers, dwBufferCount, lpNumberOfBytesRecvd, lpFlags, lpOverlapped, lpCompletionRoutine);
	if(Errors == SOCKET_ERROR) return Errors;

	char stext[80] = {0};
	sprintf(stext, "####! wsaRecv: %d\n", *lpNumberOfBytesRecvd );
	OutputDebugStringA(stext);

	return Errors;
}

//===========================GETHOSTBYNAME===========================
int WINAPI __stdcall Mygethostbyname(const char *name)
{
	return OrigGethost(name);

}

//===========================SENDTO===========================
int WINAPI __stdcall MySendTo(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen)
{
	char stext[80] = {0};
	sprintf(stext, "####! sendTo: %d\n", len );
	OutputDebugStringA(stext);


	return OrigSendTo(s, buf, len, flags, to, tolen);
}

void WINAPI WinsockHook(void)
{
	//WSADATA wsaData;
	//char msgbuf[255];
	//WSAStartup(MAKEWORD(1,1), &wsaData);

//	FILE *fp = fopen("log.txt", "ab");
//	sprintf(msgbuf, "%x", GetProcAddress(GetModuleHandle("Ws2_32.dll"), "send"));
//	fwrite(msgbuf, strlen(msgbuf), 1, fp);
//	fputc(0x0d, fp);
//	fputc(0x0a, fp);
//	fclose(fp);


	g_hj.AddHookFun("Ws2_32.dll","closesocket", (DWORD)MyClosesocket);
	g_hj.AddHookFun("Ws2_32.dll","recv", (DWORD)MyRecv);

	g_hj.SetHookOn("closesocket");
	g_hj.SetHookOn("recv");


// 	*(PDWORD)&OrigSend = APIHook((DWORD)GetProcAddress(GetModuleHandle("Ws2_32.dll"), "send"), (DWORD)MySend, (DWORD)OrigSend);
// 	*(PDWORD)&OrigRecv = APIHook((DWORD)GetProcAddress(GetModuleHandle("Ws2_32.dll"), "recv"), (DWORD)MyRecv, (DWORD)OrigRecv);
// 	*(PDWORD)&OrigSendTo = APIHook((DWORD)GetProcAddress(GetModuleHandle("Ws2_32.dll"), "sendto"), (DWORD)MySendTo, (DWORD)OrigSendTo);
// 	*(PDWORD)&OrigGethost = APIHook((DWORD)GetProcAddress(GetModuleHandle("Ws2_32.dll"), "gethostbyname"), (DWORD)Mygethostbyname, (DWORD)OrigGethost);
// 	*(PDWORD)&OrigClosesocket = APIHook((DWORD)GetProcAddress(GetModuleHandle("Ws2_32.dll"), "closesocket"), (DWORD)MyClosesocket, (DWORD)OrigClosesocket);
// 	*(PDWORD)&OrigConnect = APIHook((DWORD)GetProcAddress(GetModuleHandle("Ws2_32.dll"), "connect"), (DWORD)MyConnect, (DWORD)OrigConnect);
// 	*(PDWORD)&OrigSocket = APIHook((DWORD)GetProcAddress(GetModuleHandle("Ws2_32.dll"), "socket"), (DWORD)MySocket, (DWORD)OrigSocket);
// 	*(PDWORD)&OrigWSASend = APIHook((DWORD)GetProcAddress(GetModuleHandle("Ws2_32.dll"), "WSASend"), (DWORD)MyWSASend, (DWORD)OrigWSASend);
// 	*(PDWORD)&OrigWSARecv = APIHook((DWORD)GetProcAddress(GetModuleHandle("Ws2_32.dll"), "WSARecv"), (DWORD)MyWSARecv, (DWORD)OrigWSARecv);

//	*(PDWORD)&OrigGrfReader = APIHook(0x005426E0, (DWORD)MyGrfReader, (DWORD)OrigGrfReader);
//	*(PDWORD)&OrigGrfReader = APIHook(0x00735D4A, (DWORD)MyGrfReader, (DWORD)OrigGrfReader);
//	*(PDWORD)&OrigGrfReader = APIHook(0x00540560, (DWORD)MyGrfReader, (DWORD)OrigGrfReader);
//	*(PDWORD)&OrigMakeWindow = APIHook(0x00507540, (DWORD)MyMakeWindow, (DWORD)OrigMakeWindow);
//	*(PDWORD)&OrigLoginWindow = APIHook(0x0045A970, (DWORD)MyLoginWindow, (DWORD)OrigLoginWindow);
//	*(PDWORD)&OrigSprintf = APIHook(FUNC_SPRINTF, (DWORD)MySprintf, (DWORD)OrigSprintf);
//	*(PDWORD)&OrigHotKey = APIHook(FUNC_PROCESS_HOTKEY, (DWORD)MyHotKey, (DWORD)OrigHotKey);
//	*(PDWORD)&OrigTime = APIHook(0x0070E028, (DWORD)MyTime, (DWORD)OrigTime);
}