#include "NetDataPrivider.h"
#include "assert.h"
#include "memory.h"


CNetDataPrivider::CNetDataPrivider(void)
{
	m_pCacheBuf = 0;
	m_pDataComeImpl = nullptr;
	m_nNextNeed = 0;
	m_nCounter = 0;
	m_lpUser = 0;
	m_bUseCachePtr = false;
	m_bNeedWholePack = false;
}


CNetDataPrivider::~CNetDataPrivider(void)
{
	m_lockForQuit.Lock();
	m_pDataComeImpl = nullptr;
	m_lpUser = 0;

	if (!m_bUseCachePtr)
	{
		delete [] m_pCacheBuf;
	}
	m_pCacheBuf = nullptr;
	m_lockForQuit.Unlock();
}

void CNetDataPrivider::OnRecvData(char* pBuf, int nDataLen)
{
	m_lockForQuit.Lock();

	if(!m_pDataComeImpl)
	{
		m_lockForQuit.Unlock();
		return ;
	}
	char* _pBuf = pBuf;
	int _nDataLen = nDataLen;
	do 
	{
		if (m_nCounter == 0)
		{
			if(m_nNextNeed > 0)
			{	
				assert(m_pCacheBuf == nullptr);
				if (m_nNextNeed <= _nDataLen)
				{
					m_pCacheBuf = _pBuf;
					m_bUseCachePtr = true;
				}
				else
				{
					if (m_bNeedWholePack)
					{
						m_pCacheBuf = new char[m_nNextNeed];
						m_bUseCachePtr = false;
					}
					else
					{
						m_pCacheBuf = _pBuf;
						m_bUseCachePtr = true;
					}
				}
			}
			else
			{
				if(m_pDataComeImpl->OnDataCome(pBuf, nDataLen, m_lpUser))
				{
					//发生改包时，不需要处理
				}
				break;
			}
		}

		int nLess = m_nNextNeed - m_nCounter;
		if (nLess <= _nDataLen)
		{
			//够用了
			if (!m_bUseCachePtr)
			{
				memcpy(&m_pCacheBuf[m_nCounter], _pBuf, nLess); 
			}
			else
			{
				if (m_bNeedWholePack)
				{
					assert(m_nCounter == 0);
				}	
			}

			if(m_pDataComeImpl->OnDataCome(m_pCacheBuf, m_nNextNeed, m_lpUser))
			{
				assert(m_bUseCachePtr);	//如果发生数据修改，应该直接引用原buffer，否则，需要memcpy源数据包
			}

			if (m_pCacheBuf)
			{
				if (!m_bUseCachePtr)
				{
					delete [] m_pCacheBuf;
				}
				m_pCacheBuf = nullptr;
			}
			m_nCounter = 0;

			_nDataLen -= nLess;
			_pBuf += nLess;

			if (_nDataLen > 0)
			{
				continue;
			}
		}
		else
		{
			//不够用
			if(!m_bUseCachePtr)
			{
				memcpy(&m_pCacheBuf[m_nCounter], _pBuf, _nDataLen);
			}		
			m_nCounter += _nDataLen;
		}
		break;
	} while (true);
	m_lockForQuit.Unlock();
}

void CNetDataPrivider::SetNextNeed(int nNeed, bool b)
{
	m_nNextNeed = nNeed;
	m_bNeedWholePack = b;
}