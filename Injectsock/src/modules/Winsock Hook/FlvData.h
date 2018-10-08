#pragma once
#include <map>
using namespace std;
#include <winsock2.h>
class CFlvStreamInterface
{
public:
	virtual bool OnVideoHeaderCome(SOCKET s, unsigned int & nTimeStamp, int nDatalen) = 0;
	virtual bool OnAudioHeaderCome(SOCKET s, unsigned int & nTimeStamp, int nDatalen) = 0;
};

class CFlvData
{
public:
	struct DataInfo
	{
		bool bGetHeader;
		int nLessBufLen;
		int nLastPackageLen;
		int nLessHeaderLen;
		int nLessEnd;
		char sHeader[11];
	};

	CFlvData(CFlvStreamInterface*);
	~CFlvData(void);

	bool OnCloseHandle(void *);

	bool OnRecvData(void* s, char * pData, int nLen);
public:
	bool FilterFlvBuf(void* s, char* pData, int nLen, int &);
	int  DoProcHeader(void * s, char *);

protected:
	map<void*, DataInfo *>	m_flvMap;
	CFlvStreamInterface*	m_pFlvNotify;
};

