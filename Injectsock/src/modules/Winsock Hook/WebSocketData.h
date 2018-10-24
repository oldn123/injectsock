#pragma once
#include <map>
#include <set>
#include <winsock2.h>
#include <queue>
#include <string>
using namespace std;

class CWSStreamInterface
{
public:
	virtual bool OnWSDataCome(SOCKET s, char*, int noffset, int nDatalen, DWORD & nDelayTime, int nMsgIdx) = 0;
};

class CWebSocketData
{
public:
	CWebSocketData(CWSStreamInterface *);
	~CWebSocketData(void);

	enum AlignMode{
		eNo = 0,
		eBegin = 1,
		eEnd = 2,
		eBoth = 3
	};

	struct DataInfo
	{
		int nIdx;
		int nPopLen;
		DWORD dwNeedDelay;
		DWORD dwPushTime;
		int nLessBufLen;
		int nPackageLen;
		char * pData;
		int nDataOffset;
		int nDataPos;
	};
	static bool _GetAddressBySocket(SOCKET s, string & sSock);

	bool IsShadowMode(){return m_bShadowMode;}
	void SetShadowMode(bool b){m_bShadowMode = b;}

	bool GetAddressBySocket(SOCKET s, string & sSock);
public:
	DataInfo * FixRemoveMsg(void * s, char * pData, int & nLen, int maxLen);
	int ReadDelayData(void * s, char * pData, int & nLen, int maxLen, int am);
	void OnCloseHandle(void *);
	bool OnRecvData(void * s, char * pData, int & nLen, int maxLen, int & nAlignMode);
	bool IsWSMsg(void * s, char * pData, int nLen);

protected:
	int parse_recv_data(char *p_data, unsigned int ui_recv_data_size
		, int *pi_opcode, int *pi_final, unsigned int *pui_data_size);


protected:
	bool							m_bShadowMode;
	map<string, DataInfo *>			m_removeMap; 
	map<string, queue<DataInfo *>>	m_wsMap;
	map<string, DataInfo *> 		m_curDataInfoMap;
	map<string, queue<DataInfo *>>	m_cacheMap;
	map<string, int>				m_cacheCounter;
	map<SOCKET, string>				m_convMap;
	CWSStreamInterface *			m_pWSNotify;
};

