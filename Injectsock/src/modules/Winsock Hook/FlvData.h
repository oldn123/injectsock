#pragma once
#include <map>
using namespace std;
#include <winsock2.h>
#include "NetDataPrivider.h"

class CFlvStreamInterface
{
public:
	virtual bool OnVideoHeaderCome(SOCKET s, unsigned int & nTimeStamp, int nDatalen) = 0;
	virtual bool OnAudioHeaderCome(SOCKET s, unsigned int & nTimeStamp, int nDatalen) = 0;
};

class CFlvData : public IDataCome
{
public:
	struct DataInfo
	{
		void* sock;
		bool bGetHeader;
		int nPackageLen;
		char sHeader[11];
		CNetDataPrivider dataPrivider;
	};

	CFlvData(CFlvStreamInterface*);
	~CFlvData(void);

	bool OnCloseHandle(void *);
	bool OnRecvData(void* s, char * pData, int nLen);
public:
	bool FilterFlvBuf(void* s, char* pData, int nLen, int &);
	int  DoProcHeader(void * s, char *, bool & bChange);
	virtual bool OnDataCome(char* pBuf, int nLen, void *) override;

protected:
	map<void*, DataInfo *>	m_flvMap;
	CFlvStreamInterface*	m_pFlvNotify;
};

