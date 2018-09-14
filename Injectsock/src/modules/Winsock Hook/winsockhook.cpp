#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <Iphlpapi.h>
#include <Assert.h>
#include <process.h>
#include <assert.h>
//#include "HookApi.h"
#include "mhook.h"
//#include <openssl/rand.h>
//#include <openssl/ssl.h>
//#include <openssl/err.h>
#include <vector>
#include <queue>
#include <map>
#include <set>
using namespace std;
#include "../../eikasia.h"
#include "../../../rtmp/PacketOrderer.h"


enum MsgType {
	eNodef,
	eJionRoom,
	eBegin,
	eResult
};

struct MsgInfo
{
	MsgType mt;
	int nTotalLen;
	int nRecvlen;
};

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
//CHookApi_Jmp g_hj;
map<SOCKET, queue<DataFrame>> g_dataQueueMap;

std::map<SOCKET, int> g_timerMap;


std::map<SOCKET, MsgInfo> g_msgTypeMap;
std::map<SOCKET, pair<char*, int>> g_startMap;
std::map<SOCKET, pair<char*, int>> g_endMap;
DWORD g_dwStartTime = 0;
DWORD g_dwEndTime = 0;
DWORD g_dwStartDelay = 0;
DWORD g_dwEndDelay = 0;
//#define _TestMode

#ifdef _TestMode
DWORD g_dwTestTick = 0;
#endif

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
PWSASEND OrigWSASend;
PWSARECV OrigWSARecv;
PGETHOSTBYNAME OrigGethost;

PRECV OrigRecv  = (PRECV)
	GetProcAddress(GetModuleHandle("Ws2_32.dll"), "recv");

PCLOSESOCKET OrigClosesocket = (PCLOSESOCKET)
	GetProcAddress(GetModuleHandle("Ws2_32.dll"), "closesocket");


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
	int nret = OrigClosesocket(s);
	if (g_timerMap.count(s))
	{
		g_timerMap.erase(s);
	}
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

MsgType GetMsgType(SOCKET s, char * pBuf, int nlen)
{
	MsgType mt = eNodef;
	if (g_msgTypeMap.count(s))
	{
		mt = g_msgTypeMap.at(s).mt;
		g_msgTypeMap[s].nRecvlen += nlen;
		if (g_msgTypeMap[s].nRecvlen >= g_msgTypeMap[s].nTotalLen)
		{
			g_msgTypeMap.erase(s);
		}
		return mt;
	}

	int noffset = 4;
	
	do 
	{
		char strJionRoom[] = "{\"status\":1,\"pack_type\":1,";
		if(memcmp(&pBuf[noffset],strJionRoom,strlen(strJionRoom)) == 0)
		{
			mt = eJionRoom;
			break;
		}

		char strRes[] = "{\"pack_type\":4,\"data\":{";
		if(memcmp((char*)&pBuf[noffset], strRes, strlen(strRes)) == 0)
		{
			mt = eResult;
			break;
		}

		char strStart[] = "{\"pack_type\":5,\"data\":{";
		if(memcmp((char*)&pBuf[noffset], strStart, strlen(strStart)) == 0)
		{
			mt = eBegin;
			break;
		}
	} while (0);
	
	if (mt != eNodef)
	{
		int nMsgLen = (BYTE)pBuf[2];
		nMsgLen <<= 8;
		nMsgLen |= (BYTE)pBuf[3];

		if (nlen < nMsgLen)
		{
			g_msgTypeMap[s].mt = mt;
			g_msgTypeMap[s].nTotalLen = nMsgLen;
			g_msgTypeMap[s].nRecvlen = nlen - 4;
		}
	}

	return mt;
}

bool ReplaceBuf(char * pSrcBuf, char * strFind, char * strRep)
{
	char * pfind = strstr((char*)&pSrcBuf[0], strFind);
	if (pfind)
	{
		memcpy(pfind, strRep, strlen(strRep));
		return true;
	}
	return false;
}


//===========================RECV===========================
int WINAPI __stdcall MyRecv(SOCKET s, const char* buf, int len, int flags)
{
	int RecvedBytes = 0;
	//g_hj.SetHookOff("recv");	
	RecvedBytes = OrigRecv(s, (char*)buf, len, flags);
	//g_hj.SetHookOn("recv");

	if(RecvedBytes == SOCKET_ERROR)
	{
		if (g_timerMap.size())
		{
			if (g_startMap.count(s) && g_dwStartTime != 0 && g_dwStartDelay != 0)
			{
				if (GetTickCount() - g_dwStartTime >= g_dwStartDelay - g_dwStartDelay / 4)
				{
					int realret = g_startMap[s].second;
					memcpy((void*)buf, g_startMap[s].first, realret);
					delete [] g_startMap[s].first;
					g_startMap.erase(s);
					return realret;
				}
			}

			if (g_endMap.count(s) && g_dwEndTime != 0 && g_dwEndDelay != 0)
			{
				if (GetTickCount() - g_dwEndTime >= g_dwEndDelay - g_dwEndDelay / 4)
				{
					int realret = g_endMap[s].second;
					memcpy((void*)buf, g_endMap[s].first, realret);
					delete [] g_endMap[s].first;
					g_endMap.erase(s);
					return realret;
				}
			}
		}
	}

	if(RecvedBytes == SOCKET_ERROR) return RecvedBytes;

	bool bVideoMsg = len >= 8 && GetRtmpPacketType((unsigned char*)buf) == RtmpPacket::Video;
	if (bVideoMsg)
	{
		if (!g_timerMap.count(s))
		{
			g_timerMap[s] = 0;
		}

		if (g_timerMap[s] >= 15 * 1000)
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
		MsgType mt = GetMsgType(s, (char*)buf, RecvedBytes);
		switch(mt)
		{
		case eBegin:
			{
				if (g_timerMap.size())
				{
					char * pdata = new char[RecvedBytes];
					memcpy(pdata, buf, RecvedBytes);
					g_startMap[s] = make_pair(pdata, RecvedBytes);
					g_dwStartTime = GetTickCount();
					g_dwStartDelay = g_timerMap.begin()->second;
					memset((void*)&buf[0], 0, len);
					return 6;
				}
				return RecvedBytes;
			}
			break;
		case eResult:
			{
				if (g_timerMap.size())
				{
					char * pdata = new char[RecvedBytes];
					memcpy(pdata, buf, RecvedBytes);
					g_endMap[s] = make_pair(pdata, RecvedBytes);
					g_dwEndTime = GetTickCount();
					g_dwEndDelay = g_timerMap.begin()->second;
					memset((void*)&buf[0], 0, len);
					return 6;
				}
				return RecvedBytes;
			}
			break;
		case eJionRoom:
			{

			}
			break;
		}


		bool bJson = ((buf[0] & 0xc0) >> 6) == 2 && buf[4] == '{';
		if (bJson)
		{
			char soutput[150] = {0};
			strcat(soutput, ">>> ");
			memcpy(&soutput[4], &buf[4], RecvedBytes - 4 > 140 ? 140 : RecvedBytes - 4);
			strcat(soutput, "\n");
			OutputDebugStringA(soutput);
		}
	}

	return RecvedBytes;
	
	
// 	map<SOCKET, queue<DataFrame>> & dqm = g_dataQueueMap;
// 	queue<DataFrame> & dnQueue = dqm[s];
// 
// 	int nRecLen = RecvedBytes;
// 	bool bNeedRecvData = false;
// 	DataNode * _pdn = NULL;
// 	if (dnQueue.size() > 0)
// 	{
// 		DataFrame & df = dnQueue.back();
// 		if(df.nGetSize < df.nTotalSize)
// 		{
// 			df.nGetSize += nRecLen;
// 			if (df.nGetSize > df.nTotalSize)
// 			{
// 				nRecLen -= df.nGetSize - df.nTotalSize;
// 				assert(0);
// 			}
// 			_pdn = df.pNode;
// 		}	
// 	}
// 
// 	if (_pdn)
// 	{
// 		//补充数据节点
// 		DataNode * pdn = NULL;
// 		while(1)
// 		{
// 			if(_pdn)
// 			{
// 				pdn = _pdn;
// 				_pdn = _pdn->pNext;
// 			}
// 			else
// 			{
// 				break;
// 			}
// 		}
// 		pdn->pNext = new DataNode;
// 		pdn->pNext->nIdx = g_msgCnt++;
// 		pdn->pNext->nSize = nRecLen;
// 		pdn->pNext->pBuf = new char[nRecLen];
// 		memcpy(pdn->pNext->pBuf, buf, pdn->pNext->nSize);
// 		pdn->pNext->pNext = NULL;	
// 	}
// 	else
// 	{
// 		bool bVideoMsg = len >= 8 && GetRtmpPacketType((unsigned char*)buf) == RtmpPacket::Video;
// 		if (!bVideoMsg)
// 		{
// 			return RecvedBytes;
// 		}
// 		else
// 		{
// 			DataFrame df;
// 			df.nLoop = 0;
// 			df.pNode = new DataNode;
// 			df.pPopNode = df.pNode;
// 			df.nGetSize = nRecLen;
// 
// 			unsigned char totalExpected_byte0 = *(buf + 4);	
// 			unsigned char totalExpected_byte1 = *(buf + 5);
// 			unsigned char totalExpected_byte2 = *(buf + 6);
// 
// 			int totalExpected = (totalExpected_byte0 << 16) | (totalExpected_byte1 << 8) | totalExpected_byte2 + 8;
// 			// add in extra chunk delimiters
// 			totalExpected += totalExpected/128;
// 
// 			df.nTotalSize = totalExpected;
// 
// 
// 			df.pNode->nIdx = g_msgCnt++;
// 			df.pNode->nSize = nRecLen;
// 			df.pNode->pBuf = new char[nRecLen];
// 			memcpy(df.pNode->pBuf, buf, df.pNode->nSize);
// 			df.pNode->pNext = NULL;
// 
// 			dnQueue.push(df);
// 		}
// 	}
// 
// 	//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
// 
// 	DataNode *pdn = NULL;
// 	if (dnQueue.size() && g_msgCnt > 1)
// 	{
// 		DataFrame & df = dnQueue.front();
// 		pdn = df.pPopNode;
// 		df.pPopNode = df.pPopNode->pNext;
// 		if (df.pPopNode == NULL)
// 		{
// 			df.pPopNode = df.pNode;
// 			df.nLoop++;
// 		}
// 		memset((char*)buf, 0, len);
// 		memcpy((char*)buf, pdn->pBuf, pdn->nSize);
// 		RecvedBytes = pdn->nSize;
// 
// 		if (df.nLoop > 1)
// 		{
// 			ReleaseDf(df);
// 			dnQueue.pop();
// 		}
// 	}
// 
// 	char stext[80] = {0};
// 	sprintf(stext, "####! recv: %d\n", RecvedBytes);
// 	OutputDebugStringA(stext);

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

#ifdef _TestMode
unsigned __stdcall ThreadStaticEntryPoint(void*)
{
	do 
	{	
		if (g_dwTestTick == 0)
		{
			g_dwTestTick = GetTickCount();
		}
		else
		{
			if(GetTickCount() - g_dwTestTick > 1000 * 60)
			{

			}
		}
		Sleep(1000*60);
	} while (true);
	return 1;// the thread exit code
}
#endif


HWND g_hMsgWnd = 0;

//窗口处理函数

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_USER+100:
		{
			char stext[100] = {0};
			sprintf(stext, ">>> %d--%d\n", wParam, lParam);
			OutputDebugStringA(stext);
			OutputDebugStringA(">>> hotkey 1");
			if (wParam == 0)
			{
				OutputDebugStringA(">>> hotkey 2");
				EnableHook(TRUE);
			}
			else if (wParam == 1)
			{
				OutputDebugStringA(">>> hotkey 3");
				EnableHook(FALSE);
			}
			OutputDebugStringA(">>> hotkey 4");
		}
		break;
	case WM_DESTROY:

	//	PostQuitMessage(0);//可以使GetMessage返回0
		break;
	default:
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);

}

int InitSvrSocket()
{
	int sock;
	//sendto中使用的对方地址
	struct sockaddr_in toAddr;
	//在recvfrom中使用的对方主机地址
	struct sockaddr_in fromAddr;
	int recvLen;
	int addrLen = 0;
	char recvBuffer[128] = {0};
	sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	if(sock < 0)
	{
		return 0;
	}
	memset(&fromAddr,0,sizeof(fromAddr));
	fromAddr.sin_family=AF_INET;
	fromAddr.sin_addr.s_addr=htonl(INADDR_ANY);
	fromAddr.sin_port = htons(4011);
	if(bind(sock,(struct sockaddr*)&fromAddr,sizeof(fromAddr))<0)
	{
		closesocket(sock);
		return 0;
	}
	while(1){
		addrLen = sizeof(toAddr);
		if((recvLen = recvfrom(sock,recvBuffer,128,0,(struct sockaddr*)&toAddr,&addrLen)) > 0)
		{
			
		}
		
		Sleep(500);
		return 0;
	}
}



bool DoCreateWnd(HINSTANCE hInst)
{
	//注册窗口类

	WNDCLASSEX wce = { 0 };
	wce.cbSize = sizeof(wce);
	wce.cbClsExtra = 0;
	wce.cbWndExtra = 0;
	wce.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wce.hCursor = NULL;
	wce.hIcon = NULL;
	wce.hIconSm = NULL;
	wce.hInstance = hInst;
	wce.lpfnWndProc = WndProc;
	wce.lpszClassName = "EikMsgWnd";
	wce.lpszMenuName = NULL;
	wce.style = CS_HREDRAW | CS_VREDRAW;
	ATOM nAtom = RegisterClassEx(&wce);
	if (!nAtom )
	{
		return false;
	}

	g_hMsgWnd = CreateWindowEx(0, "EikMsgWnd", "", WS_POPUPWINDOW, 0, 0, 0, 0, NULL, NULL, hInst, NULL);
	if (!g_hMsgWnd)
	{
		return false;
	}


	HWND hDest = FindWindow("EikMsgWndA",NULL);
	if (hDest)
	{
		char sbuf[100] = {0};

		OutputDebugStringA(">>> EikMsgWndA found");
		COPYDATASTRUCT copyData = { 0 };
		copyData.lpData = 0;
		copyData.cbData = 0;
		copyData.dwData = (DWORD)hDest;
		LRESULT lr = SendMessage(hDest, WM_COPYDATA, (WPARAM)hDest, (LPARAM)&copyData);
		sprintf(sbuf, ">>> 0x%x, %x, %d", hDest, lr, GetLastError());
		OutputDebugStringA(sbuf);
	}
	else
	{
		OutputDebugStringA(">>> EikMsgWndA not found");
	}
	
	return true;
}

void WINAPI EndHook()
{
	if (g_hMsgWnd)
	{
		DestroyWindow(g_hMsgWnd);
		g_hMsgWnd = NULL;
	}

	EnableHook(FALSE);
}

bool WINAPI EnableHook(BOOL bHook)
{
	int n = 0;
	if (bHook)
	{
		if (Mhook_SetHook((PVOID*)&OrigRecv, MyRecv)) {		
			n++;
		}

		if (Mhook_SetHook((PVOID*)&OrigClosesocket, MyClosesocket)) {		
			n++;
		}
	}
	else
	{
		if(Mhook_Unhook((PVOID*)&OrigRecv))
		{
			n++;
		}

		if (Mhook_Unhook((PVOID*)&OrigClosesocket))
		{
			n++;
		}
	}

	return n == 2;
}


void WINAPI WinsockHook(HINSTANCE hInst)
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


// 	if(DoCreateWnd(hInst))
// 	{
// 		OutputDebugStringA(">>> 窗口创建ok");
// 	}
// 	else
// 	{
// 		OutputDebugStringA(">>> 窗faild");
// 	}

// 	g_hj.AddHookFun("Ws2_32.dll","closesocket", (DWORD)MyClosesocket);
// 	g_hj.AddHookFun("Ws2_32.dll","recv", (DWORD)MyRecv);
// 
// 	g_hj.SetHookOn("closesocket");
// 	g_hj.SetHookOn("recv");

	HANDLE hProc = NULL;

#ifdef _TestMode
	unsigned int dwThread = 0;
	HANDLE hth1 = (HANDLE)_beginthreadex(NULL,0,ThreadStaticEntryPoint,0,CREATE_SUSPENDED,&dwThread);
	ResumeThread(hth1);
#endif	// Set the hook
	
	OutputDebugStringA(">>> 123");

	EnableHook(TRUE);

	OutputDebugStringA(">>> 456");
}