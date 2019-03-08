#pragma once
#include "windows.h"
class RAII_CrtcSec
{
private:
	CRITICAL_SECTION crtc_sec;
public:
	RAII_CrtcSec() { ::InitializeCriticalSection(&crtc_sec); }
	~RAII_CrtcSec() { ::DeleteCriticalSection(&crtc_sec); }

	void Lock() { ::EnterCriticalSection(&crtc_sec); }
	void Unlock() { ::LeaveCriticalSection(&crtc_sec); }
};


class IDataCome
{
public:
	virtual bool OnDataCome(char* pBuf, int nLen, void *) = 0;	//返回值为，是否对包做了修改，如发生修改，则返回true
};

class CNetDataPrivider
{
public:
	CNetDataPrivider(void);
	~CNetDataPrivider(void);

	void SetDataComeNotify(IDataCome * p, void * pUser){m_pDataComeImpl = p;m_lpUser = pUser;}
public:
	void OnRecvData(char* pBuf, int nDataLen);	
	void SetNextNeed(int, bool bNeedWholePack);

	void InputData(char* pBuf, int nDataLen);	
	int GetOutputData(char* pBuf, int nDataLen);

protected:
	IDataCome * m_pDataComeImpl;
	bool		m_bUseCachePtr;
	char *		m_pCacheBuf;
	int			m_nNextNeed;
	int			m_nCounter;
	void *		m_lpUser;
	bool		m_bNeedWholePack;
	RAII_CrtcSec m_lockForQuit;
};

