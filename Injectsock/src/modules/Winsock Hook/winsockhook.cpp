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

#include "FlvData.h"
#include "WebSocketData.h"

//定义16、32、64位的调位函数。这里就是字节“搬家”而已。
#define SWAP16(s) ((((s)&0xff)<<8)|(((s)>>8)&0xff))

#define SWAP32(l) (((l)>>24)|\
	(((l)&0x00ff0000)>>8)|\
	(((l)&0x0000ff00)<<8)|\
	((l)<<24))

#define SWAP64(ll) (((ll)>>56)|\
	(((ll)&0x00ff000000000000)>>40)|\
	(((ll)&0x0000ff0000000000)>>24)|\
	(((ll)&0x000000ff00000000)>>8)|\
	(((ll)&0x00000000ff000000)<<8)|\
	(((ll)&0x0000000000ff0000)<<24)|\
	(((ll)&0x000000000000ff00)<<40)|\
	((ll)<<56)) 

HINSTANCE g_hMod = NULL;

enum MsgType {
	eNodef,
	eJionRoom,
	eBegin,
	eBegin2,
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

int g_nSleepStep = 0;
int g_msgCnt = 0;
//CHookApi_Jmp g_hj;
map<SOCKET, queue<DataFrame>> g_dataQueueMap;
std::set<SOCKET> g_igCompSock;
std::map<SOCKET, int> g_timerMap;
std::map<SOCKET, queue<pair<char*, int>>> g_lookatStream;
std::map<SOCKET, queue<pair<char*, int>>> g_bufDelayMap;
//std::map<SOCKET, MsgInfo> g_msgTypeMap;
std::map<SOCKET, pair<char*, int>> g_startMap;
std::map<SOCKET, pair<char*, int>> g_endMap;
DWORD g_dwStartTime = 0;
DWORD g_dwEndTime = 0;
DWORD g_dwStartDelay = 0;
DWORD g_dwEndDelay = 0;

set<string> g_curWebSockHandleSet;
//#define _TestMode
SOCKET g_curFlvSock = 0;
map<SOCKET, DWORD> g_dwLastFlvTimeMap;
map<SOCKET, FILE *> g_sMonitorMap;
#ifdef _TestMode
DWORD g_dwTestTick = 0;
#endif


class CFlvNotify : public CFlvStreamInterface
{
public:
	virtual bool OnVideoHeaderCome(SOCKET s, unsigned int & nTimeStamp, int nDatalen){	
		g_curFlvSock = s;
		if (!g_curWebSockHandleSet.size())
		{
			return false;
		}
		int ntime = g_nSleepStep;
		
		if (g_timerMap[s] < 20000)
		{
			if (g_dwLastFlvTimeMap.count(s))
			{
				int nDiff = nTimeStamp - g_dwLastFlvTimeMap[s];
				ntime = nDiff / 5; 
			}

			g_dwLastFlvTimeMap[s] = nTimeStamp;

			Sleep(ntime);
			g_timerMap[s] += ntime;
			nTimeStamp+= g_timerMap[s];

			char sBuf[60] = {0};
			sprintf(sBuf, ">>> video---%d,%d\n", (int)nTimeStamp, g_timerMap[s]);
		//	OutputDebugStringA(sBuf);

// 			Sleep(10);
// 			g_timerMap[s] += ntime;
// 			nTimeStamp+= ntime;		
		}
		else
		{		
			nTimeStamp+= g_timerMap[s];
		}
		return true;
	}

	virtual bool OnAudioHeaderCome(SOCKET s, unsigned int & nTimeStamp, int nDatalen){
		nTimeStamp += g_timerMap[s];	
		return TRUE;
	}
};

class CWSNotify : public CWSStreamInterface
{
public:
	virtual bool OnWSDataCome(SOCKET s, char* pData, int noffset, int nDatalen, DWORD & nDelayTime, int nMsgIdx);
};

CFlvNotify g_fn;
CFlvData g_fd(&g_fn);

CWSNotify g_wsn;
CWebSocketData g_wsd(&g_wsn);
SOCKET g_roomsock = 0;
enum AgGameIdDef{
	eRejectSvrIp	= 0x02008600,		// 00 86 00 02 00 00 00 0c 00 00 00 00
	eJoinRoom		= 0x07200100,		// 0001200700000019
	
					//0x2f000200,
	eBeginGame		= 0x0b000200,		// 821C0002000B0000001C00000000
	eBeginGame2		= 0x35000200,
	
	ePing3S_1		= 0x37000200,		// 每3秒一个
	ePing3S_2		= 0x37000300,		// 每3秒一个
	eEndGame		= 0x3A000200,
	eEndGame1		= 0x19000300,
	eDispchWiner	= 0x39000200,		//分配输赢

	eUserTalk		= 0x05110600,		//玩家发言
	eUserTalk2		= 0x07110600,		//固定发言，应该是快捷方式
	eGameResultOutRoom	= 0x11000200,		// 8223000200110000002300000000	//房间外的开局信息
	eGameResult		= 0x109a0400,
	ePay			= 0x36000200,		//买入

	eIgnoreMsg1		= 0x1c000200,
	eIgnoreMsg2		= 0x14000200,
	eIgnoreMsg3		= 0x36a00200,		//
			//0x0fb00000
			//0x10b10000
			//0x26800000
			//0x3a000200
			//0x63000400
};

DWORD GetDelayTime()
{
	return g_curFlvSock ? g_timerMap[g_curFlvSock] : 0;
}

map<SOCKET, map<int ,int>> cntmap;
char g_curRoomName[10] = {0};
bool CWSNotify::OnWSDataCome(SOCKET s, char* pData, int noffset, int nDatalen, DWORD & nDelayTime, int nMsgIdx){
	int nCode = *(int*)&pData[noffset];
	
	char sText[100] = {0};
	switch(nCode)
	{
	case eRejectSvrIp:
		{
			assert(nDatalen == 0xe);
			sprintf(sText, ">>> [%x], 拒绝连接服务端\n", s);	
		}
		break;
	case eJoinRoom:
		{
			memcpy(g_curRoomName, &pData[18], 4);
			
			sprintf(sText, ">>> [%x], 进入房间[%s]\n", s, g_curRoomName);	
			g_roomsock = s;
			string sSock;
			g_wsd.GetAddressBySocket(s, sSock);
			g_curWebSockHandleSet.insert(sSock);

			nDelayTime = GetDelayTime();
		}
		break;
	case ePay:
		{
			sprintf(sText, ">>> [%x], 玩家买入[%x]\n", s, nDatalen);	
		}
		break;
	case eGameResult:
		{
			sprintf(sText, ">>> [%x], 游戏出结论[%x]\n", s, nDatalen);
		}
		break;
	case eGameResultOutRoom:
		{
			char sRoom[5] = {0};
			memset(sRoom, 0, 5);
			memcpy(sRoom, &pData[14], 4);
			char sRound[20]  = {0};
			memset(sRound, 0, 20);
			memcpy(sRound, &pData[19], 13);
			sprintf(sText, ">>> [%x], 游戏出结论[%s]-[%s],庄[%d],闲[%d]\n", s, sRoom, sRound, (int)(pData[33]&0xff),(int)(pData[34]&0xff));
		}
		break;
	case eBeginGame:
		{
			cntmap.clear();
			cntmap[s][nCode] = 0;
			char sRoundName[20] = {0};
			memcpy(sRoundName, &pData[14], 14);
			nDelayTime = GetDelayTime();

			if (g_sMonitorMap.count(s))
			{
				char ss[16] = {0};
				memset(ss, 0x98, 16);
				fwrite(ss, 1, 16, g_sMonitorMap[s]);
			}
			sprintf(sText, ">>> [%x], 已开局[%s]------------------------------------延时[%d]\n", s, sRoundName, nDelayTime);	
		}
		break;
	case eBeginGame2:
		{
			char sName[10] = {0};
			memcpy(sName, &pData[14], 4);
		//	if (strcmp(g_curRoomName, sName) == 0/* && *(LPWORD)pData[0x13] == 0x1900*/)
			{
				if (g_sMonitorMap.count(s))
				{
					char ss[16] = {0};
					memset(ss, 0x99, 16);
					fwrite(ss, 1, 16, g_sMonitorMap[s]);
				}

				nDelayTime = GetDelayTime();
				sprintf(sText, ">>> [%x], 开始计时[%s][%x]------------------------------延时[%d]\n", s, sName, nDatalen,nDelayTime);		
			}
		}
		break;
	case eDispchWiner:
		{
			char sText1[20] = {0};
			memcpy(sText1, &pData[14], 0x1e);
			sprintf(sText, ">>> [%x], 分配输赢:[%s]\n", s, sText1);
			//nDelayTime += 0x80000;
		}
		break;
	case eEndGame:
		{
			sprintf(sText, ">>> [%x], 出结论--------size:%d\n", s, nDatalen);
			//nDelayTime += 0x80000;
		}
		break;
	case eUserTalk:
	case eUserTalk2:
		{
			sprintf(sText, ">>> [%x], 玩家发言--------size:%d\n", s, nDatalen);
			nDelayTime += GetDelayTime();
		}
		break;
	case eEndGame1:
		{
			sprintf(sText, ">>> [%x], 出结论1-------size:%d\n", s, nDatalen);	
			//nDelayTime += 0x80000;
		}
		break;
	case ePing3S_1:
		{
			sprintf(sText, ">>> [%x], PING--1[%x]\n", s, nDatalen);	
			//nDelayTime = GetDelayTime();
		}
		break;
	case ePing3S_2:
		{
			sprintf(sText, ">>> [%x], PING--2[%x]\n", s, nDatalen);
			//nDelayTime = GetDelayTime();
		}
		break;
	case eIgnoreMsg1:
	case eIgnoreMsg2:
	case eIgnoreMsg3:
		{		
			strcpy(sText, "");
			//sprintf(sText, ">>> [%x], 忽略-------size:%d\n", s, nDatalen);
			//nDelayTime += 3000;
			//nDelayTime += 0x80000;
		}
		break;
	default:
		{
	
		}
	}


	//if (eBeginGame2 != nCode)
	{	
		nDelayTime = 0;
	}

//	if (cntmap.size())
	if (g_roomsock && g_roomsock != s)
	{
	//	nDelayTime += 0x80000;
	}
//	if (g_roomsock == s)
	{
		{
			cntmap[s][nCode]++;
			if (strlen(sText) == 0)
			{
				sprintf(sText, ">>> [%x] 0x%x--[%x]--[%d]\n", s, nCode, nDatalen, cntmap[s][nCode]);
			}		
		}


		{
			if (strlen(sText) > 0)
			{
				OutputDebugStringA(sText);
			}
		}		
	}
	
	return true;
}

typedef int (WINAPI *PCONNECT)(SOCKET s, const struct sockaddr *address, int namelen);
typedef int (WINAPI *PGETHOSTBYNAME)(const char *name);
typedef int (WINAPI *PSEND)(SOCKET s, const char* buf, int len, int flags);
typedef int (WINAPI *PRECV)(SOCKET s, const char* buf, int len, int flags);
typedef int (WINAPI *PSENDTO) (SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen);
typedef int (WINAPI *PWSASEND)(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
typedef int (WINAPI *PWSARECV)(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
typedef SOCKET (WINAPI *PSOCKET) (int af, int type, int protocol);
typedef int (WINAPI *PCLOSESOCKET) (SOCKET s);
typedef int (WSAAPI *PSETSOCKOPT)(__in SOCKET s,__in int level,__in int optname,const char FAR * optval,__in int optlen);
typedef int (WSAAPI *PIOCTRLSOCKET)(__in SOCKET s,__in long cmd,__inout u_long FAR * argp);
typedef int (WSAAPI *PSELECT)(__in int nfds,__inout_opt fd_set FAR * readfds,__inout_opt fd_set FAR * writefds,__inout_opt fd_set FAR * exceptfds,__in_opt const struct timeval FAR * timeout);
/*
typedef int (WINAPI *PSPRINTF) (char *_Dest, const char *_Format, va_list ap);
typedef time_t (WINAPI *PTIME)(time_t *);

typedef void (WINAPI *PGRFREADER) (char *grf_file, int grf_num);
typedef void (__fastcall *PHOTKEY) (int key_num);
typedef int (WINAPI *PMakeWindow) (int window);
typedef int (WINAPI *PLoginWindow) (int a, int b);
*/
PSOCKET	OrigSocket = socket;
PSEND OrigSend = send;
PSENDTO	OrigSendTo = sendto;
PWSASEND OrigWSASend;
PWSARECV OrigWSARecv;
PGETHOSTBYNAME OrigGethost;

PIOCTRLSOCKET OrigIoctrlsocket = (PIOCTRLSOCKET)
	GetProcAddress(GetModuleHandle("Ws2_32.dll"), "ioctlsocket");

PSELECT OrigSelect = (PSELECT)
	GetProcAddress(GetModuleHandle("Ws2_32.dll"), "select");

PSETSOCKOPT OrigSetsockopt = (PSETSOCKOPT)
	GetProcAddress(GetModuleHandle("Ws2_32.dll"), "setsockopt");

PCONNECT OrigConnect = (PCONNECT)
	GetProcAddress(GetModuleHandle("Ws2_32.dll"), "connect");

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
	SOCKET s = OrigSocket(af, type, protocol);
	char sOut[100] = {0};
	sprintf(sOut, ">>> +++++++++++ sock-creat:0x%x\n", s);
	OutputDebugStringA(sOut);
	return s;
}

int WSAAPI MyIoctrlsocket(__in SOCKET s,__in long cmd,__inout u_long FAR * argp)
{
	int ret = OrigIoctrlsocket(s, cmd, argp);
	char sOut[100] = {0};
	sprintf(sOut, ">>> ioctrlsocket:0x%x, 0x%x, ret:%d\n", s, cmd, ret);
	OutputDebugStringA(sOut);
	return ret;
}

int WSAAPI MySelect(__in int nfds,__inout_opt fd_set FAR * readfds,__inout_opt fd_set FAR * writefds,__inout_opt fd_set FAR * exceptfds,__in_opt const struct timeval FAR * timeout)
{
	int ret = OrigSelect(nfds, readfds, writefds, exceptfds, timeout);
	char sOut[100] = {0};
	sprintf(sOut, ">>> select:%d, 0x%x, 0x%x, 0x%x, %d,%d,ret:%d\n", nfds, readfds, writefds, exceptfds, timeout ? timeout->tv_sec : 0, timeout ? timeout->tv_usec : 0, ret);
	OutputDebugStringA(sOut);
	return ret;
}

int WSAAPI MySetsockopt(__in SOCKET s,__in int level,__in int optname,const char FAR * optval,__in int optlen)
{
	int ret = OrigSetsockopt(s, level, optname, optval, optlen);
	char sOut[100] = {0};
	sprintf(sOut, ">>> setsocketopt:0x%x, %d, %d, ret:%d\n", s, level, optname, ret);
	OutputDebugStringA(sOut);
	return ret;
}


int WINAPI MyClosesocket (SOCKET s) {
	char sOut[100] = {0};
	sprintf(sOut, ">>> ----------- sock-close:0x%x\n", s);
	OutputDebugStringA(sOut);
	assert(s != g_roomsock);
	if (s == g_roomsock)
	{
		MessageBeep(0);
		g_roomsock = 0;
		//SetLastError(0x9999);
	}

	string sSock;
	g_wsd.GetAddressBySocket(s, sSock);

	int nret = OrigClosesocket(s);
	if (g_timerMap.count(s))
	{
		g_timerMap.erase(s);
	}

	if (g_sMonitorMap.count(s))
	{
		fclose(g_sMonitorMap[s]);
		g_sMonitorMap.erase(s);
	}

	if (g_curWebSockHandleSet.count(sSock))
	{
		g_curWebSockHandleSet.erase(sSock);
	}

	if (g_curFlvSock == s)
	{
		g_curFlvSock = 0;
	}

	if (g_igCompSock.count(s))
	{
		g_igCompSock.erase(s);
	}

	if (g_dwLastFlvTimeMap.count(s))
	{
		g_dwLastFlvTimeMap.erase(s);
	}
	g_wsd.OnCloseHandle((void*)s);
	g_fd.OnCloseHandle((void*)s);

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
	int noffset = 0;
	
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

		{
			char sRoomName[10] = {0};
			char sRoundName[20] = {0};
			BYTE sbegin[14] = {0x82,0x20,0x00,0x02,0x10,0x1B,0x00,0x00,0x00,0x20,0,0,0,0};

			if(memcmp((char*)&pBuf[noffset], sbegin, 14) == 0)
			{
				memcpy(sRoomName, &pBuf[14], 4);
				memcpy(sRoundName, &pBuf[18], 14);
				mt = eBegin;
				break;
			}
		}

		{
			//821C0002000B0000001C00000000
			char sRoundName[20] = {0};
			BYTE sbegin[14] = {0x82,0x1C,0x00,0x02,0x00,0x0B,0x00,0x00,0x00,0x1C,0,0,0,0};
			if(memcmp((char*)&pBuf[noffset], sbegin, 14) == 0)
			{
				memcpy(sRoundName, &pBuf[14], 14);
				mt = eBegin2;
				break;
			}
		}
	} while (0);


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

int FindData(LPBYTE pData, int nDataSize, LPBYTE pFind, int nFindSize)
{
	int nFindIdx = -1;
	for (int i = 0; i < nDataSize - nFindSize; i++)
	{
		if(memcmp(&pData[i], pFind, nFindSize) == 0)
		{
			nFindIdx = i;
			i += nFindSize;
		}
	}
	return nFindIdx >= 0;
}

int WINAPI __stdcall FakeRecv(SOCKET s, const char* buf, int len, int flags)
{
	int RecvedBytes = 0;

	RecvedBytes = OrigRecv(s, (char*)buf, len, flags);

	if(RecvedBytes == SOCKET_ERROR)
	{
// 		char __buf[0x8000] = {0};
// 		int nRet = g_wsd.ReadDelayData((void*)s, (char*)buf, len);
// 
// 		if (nRet >= 0)
// 		{
// 			memcpy((void*)&buf[0], __buf, nRet);
// 			char sBuf[120] = {0};
// 			sprintf(sBuf, ">>> fake ret1: %d\n", nRet);
// 			OutputDebugStringA(sBuf);
// 			return nRet;
// 		}
		return RecvedBytes;
	}
	else
	{
		char sFile[100] = {0};
		sprintf(sFile, "d:\\data\\s_%x.dat", s);
		FILE * fp = fopen(sFile, "ab");
		fwrite(&buf[0], 1, RecvedBytes, fp);
		fclose(fp);
	}

	if(!g_fd.OnRecvData((void*)s, (char*)buf, RecvedBytes))
	{
		//非flv
		DWORD dwDiff = 0;
		string sSock;
		g_wsd.GetAddressBySocket(s, sSock);
		int am;	//CWebSocketData::AlignMode 

		if(g_wsd.OnRecvData((void*)s, (char*)buf, RecvedBytes, len, am))
		{	
		}

		if (am != 0)
		{
			g_wsd.ReadDelayData((void*)s, (char*)buf, RecvedBytes, len, am);
		}
	}

	return RecvedBytes;
}

//===========================RECV===========================
int WINAPI __stdcall MyRecv(SOCKET s, const char* buf, int len, int flags)
{	
	if(flags == 2)
	{
		int retTest = OrigRecv(s, buf, len, flags);
		char stext[80] = {0};
		sprintf(stext, ">>> [0x%x] 测试是否有数据: ret =%d, inputLen=%d\n",s, retTest, len);
		OutputDebugStringA(stext);
		//测试是否有新数据到来
		return retTest;
	}
	assert(flags == 0);
	int RecvedBytes = FakeRecv(s, buf, len, flags);


//	if(0)
	{
		
		if (RecvedBytes > 0)
		{
			char sFile[100] = {0};
			sprintf(sFile, "d:\\data\\changed\\s_%x.dat", s);
			FILE * fp = fopen(sFile, "ab");
			fwrite(&buf[0], 1, RecvedBytes, fp);
			fclose(fp);
		}
		else
		{
			char stext[80] = {0};
			sprintf(stext, ">>> [0x%x] ret %d\n",s, RecvedBytes);
			OutputDebugStringA(stext);
		}	
	}


// 	if (g_sMonitorMap.count(s) && RecvedBytes > 0)
// 	{
// 		fwrite(buf, 1, RecvedBytes, g_sMonitorMap[s]);
// 		char sline[16] = {0};
// 		memset(sline, 0x88, 16);
// 		fwrite(sline, 1, 16, g_sMonitorMap[s]);
// 	}

	return RecvedBytes;
}

//===========================CONNECT===========================
int WINAPI __stdcall MyConnect(SOCKET s, const struct sockaddr *address, int namelen)
{
	sockaddr_in sin;
	memcpy(&sin, address, sizeof(sin));

	char stext[80] = {0};
	sprintf(stext, ">>> [0x%x] connect: %s:%d\n",s, inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
	OutputDebugStringA(stext);

	if (ntohs(sin.sin_port) == 3431)
	{
		char sFile[128] = {0};
		sprintf(sFile, "d:\\data\\ag\\joinroom\\%s.dat", inet_ntoa(sin.sin_addr));
		g_sMonitorMap[s] = fopen(sFile, "ab");
	}

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

bool g_bquit = false;


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
	while(!g_bquit){
		addrLen = sizeof(toAddr);
		memset(recvBuffer, 0, 128);
		if((recvLen = recvfrom(sock,recvBuffer,128,0,(struct sockaddr*)&toAddr,&addrLen)) > 0)
		{
			if (strncmp(recvBuffer, "****on", 6) == 0)
			{
				EnableHook(true);
			}
			else if (strncmp(recvBuffer, "****off", 7) == 0)
			{
				EnableHook(false);
			}
			else if (strncmp(recvBuffer, "****quit", 8) == 0)
			{
				EnableHook(false);
				g_bquit = false;
				break;
			}
		}

		Sleep(500);
	}

// 	OutputDebugStringA(">>> free ok");
// 	if(FreeLibrary(g_hMod))
// 	{
// 		OutputDebugStringA(">>> free ok");
// 	}
// 	else
// 	{
// 		OutputDebugStringA(">>> free faild");
// 	}
}


//#ifdef _TestMode
unsigned __stdcall ThreadStaticEntryPoint(void*)
{
// 	do 
// 	{	
// 		if (g_dwTestTick == 0)
// 		{
// 			g_dwTestTick = GetTickCount();
// 		}
// 		else
// 		{
// 			if(GetTickCount() - g_dwTestTick > 1000 * 60)
// 			{
// 
// 			}
// 		}
// 		Sleep(1000*60);
// 	} while (true);
	InitSvrSocket();


	return 1;// the thread exit code
}
//#endif


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
	OutputDebugStringA(">>> EnableHook in");
	int n = 0;
	if (bHook)
	{
		if (Mhook_SetHook((PVOID*)&OrigRecv, MyRecv)) {		
			n++;
		}

		if (Mhook_SetHook((PVOID*)&OrigClosesocket, MyClosesocket)) {		
			n++;
		}

		if (Mhook_SetHook((PVOID*)&OrigConnect, MyConnect)) {		
			n++;
		}

		if (Mhook_SetHook((PVOID*)&OrigIoctrlsocket, MyIoctrlsocket)) {		
			n++;
		}

		if (Mhook_SetHook((PVOID*)&OrigSelect, MySelect)) {		
			n++;
		}

		if (Mhook_SetHook((PVOID*)&OrigSetsockopt, MySetsockopt)) {		
			n++;
		}	

		if (n == 3)
		{
			OutputDebugStringA(">>> EnableHook(1) ret true");
		}
		else
		{
			OutputDebugStringA(">>> EnableHook(1) ret false");
		}
	}
	else
	{
		if(Mhook_Unhook((PVOID*)&OrigRecv)){
			n++;
		}

		if (Mhook_Unhook((PVOID*)&OrigClosesocket)){
			n++;
		}

		if (Mhook_Unhook((PVOID*)&OrigConnect)) {		
			n++;
		}

		OrigRecv  = (PRECV)
			GetProcAddress(GetModuleHandle("Ws2_32.dll"), "recv");

		OrigClosesocket = (PCLOSESOCKET)
			GetProcAddress(GetModuleHandle("Ws2_32.dll"), "closesocket");

		OrigConnect = (PCONNECT)
			GetProcAddress(GetModuleHandle("Ws2_32.dll"), "connect");
		
		if (n == 3)
		{
			OutputDebugStringA(">>> EnableHook(0) ret true");
		}
		else
		{
			OutputDebugStringA(">>> EnableHook(0) ret false");
		}
	}

	return n == 2;
}


void WINAPI WinsockHook(HINSTANCE hInst)
{

	g_hMod = hInst;
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

//#ifdef _TestMode
	unsigned int dwThread = 0;
	HANDLE hth1 = (HANDLE)_beginthreadex(NULL,0,ThreadStaticEntryPoint,0,CREATE_SUSPENDED,&dwThread);
	ResumeThread(hth1);
//#endif	// Set the hook
	
	OutputDebugStringA(">>> 123");

	EnableHook(TRUE);

	OutputDebugStringA(">>> 456");
}