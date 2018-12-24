#include "FlvData.h"
#include <stdio.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <assert.h>


CFlvData::CFlvData(CFlvStreamInterface * p)
{
	m_pFlvNotify = p;
}


CFlvData::~CFlvData(void)
{
}

void SaveData(char* pBuf, int nSize)
{
	char sfile[200] = {0};
	sprintf(sfile, "d:\\data\\flv\\%d_%d.dat",GetTickCount(), nSize);
	FILE * fp = fopen(sfile, "wb");
	if (fp)
	{
		fwrite(pBuf, 1, nSize,fp);
		fclose(fp);
	}
}

char gsFlvSign[] = {
	0x46,0x4c,0x56,0x01,0x01,0x00,0x00,0x00,0x09
};

bool IsMatchFlv(char * pSrcData)
{
	bool bMatch = true;
	for (int i = 0; bMatch && i < 9; i++)
	{
		if (i != 4)
		{
			bMatch = pSrcData[i] == gsFlvSign[i];
		}
		else 
		{
			bMatch = pSrcData[i] == 0x01 || pSrcData[i] == 0x05;	//01：只视频，05：音频&视频
		}
	}
	return bMatch;
}

bool CFlvData::FilterFlvBuf(void* s, char * pData, int nLen, int & nOffset)
{
	char * _data = pData;
	DataInfo * pdi = NULL;
	if (m_flvMap.count(s))
	{
		pdi = m_flvMap[s];
		if (pdi->bGetHeader)
		{
			return true;
		}

		if (IsMatchFlv(_data))
		{
			_data += 9;
			nOffset += (_data - pData);
			pdi->bGetHeader = true; 
			return true;
		}	
		else
		{
			//此情况发生时，应该是需要处理flv头的拆包情况，目前没有考虑
			assert(0);
		}
	}
	
	if ((memcmp(pData, "HTTP/", 5) == 0) && (_data = strstr((char*)pData, "Content-Type: video/x-flv")) != NULL)
	{
		if (!m_flvMap.count(s))
		{	
			OutputDebugStringA(">>> get flv buffer begin~~~~~~~~~");		
			pdi = new DataInfo;
			pdi->bGetHeader = false;
			pdi->nPackageLen = 0;
			memset(&pdi->sHeader[0], 0, 11);
			pdi->sock = s;
			pdi->dataPrivider.SetDataComeNotify(this, pdi);
			pdi->dataPrivider.SetNextNeed(4, false);	//请求4个字节的0，前一个flv包的大小，由于是第一个，所以为0
			m_flvMap[s] = pdi;
		}
		else
		{
			assert(0);
		}	

		_data = strstr(pData, "\r\n\r\n");
		if (_data)
		{
			_data += 4;
			nOffset = _data - pData;
			int ntmplen = nLen - nOffset;
			if (ntmplen >= 9)
			{
				return FilterFlvBuf(s, &pData[nOffset], ntmplen, nOffset);
			}
			else
			{
				if (ntmplen != 0)
				{
					//这种情况需要处理，flv头被拆包了
					assert(false);
				}
			}
		}
	}

	if (!m_flvMap.count(s))
	{
		//非flv流
		return false;
	}

	return pdi->bGetHeader;
}

bool CFlvData::OnCloseHandle(void * s)
{
	if (m_flvMap.count(s))
	{
		delete m_flvMap[s];
		m_flvMap.erase(s);
		return true;
	}
	return false;
}

int CFlvData::DoProcHeader(void * s, char * pHeader, bool & bChange)
{
	if (pHeader[8] == 0 && pHeader[9] == 0 && pHeader[10] == 0)
	{
		unsigned int nDataLen = 0;
		nDataLen |= ((((unsigned int)pHeader[1]) << 16) & 0xFF0000);
		nDataLen |= ((((unsigned int)pHeader[2]) << 8) & 0xFF00);
		nDataLen |= (unsigned char)pHeader[3];

		unsigned int nTime = 0;
		nTime |= ((((unsigned int)pHeader[7]) << 24) & 0xFF000000);
		nTime |= ((((unsigned int)pHeader[4]) << 16) & 0xFF0000);
		nTime |= ((((unsigned int)pHeader[5]) << 8) & 0xFF00);
		nTime |= (unsigned char)pHeader[6];

		if (m_pFlvNotify)
		{
			if (pHeader[0] == 0x09)
			{//video
				if(m_pFlvNotify->OnVideoHeaderCome((SOCKET)s, nTime, nDataLen))
				{
					((char*)pHeader)[4] = (char)((nTime >> 16) & 0xff);
					((char*)pHeader)[5] = (char)((nTime >> 8) & 0xff);
					((char*)pHeader)[6] = (char)(nTime & 0xff);
					((char*)pHeader)[7] = (char)((nTime >> 24) & 0xff);
					bChange = true;
				}
			}	
			else if (pHeader[0] == 0x08)
			{
				if(m_pFlvNotify->OnAudioHeaderCome((SOCKET)s, nTime, nDataLen))
				{
					((char*)pHeader)[4] = (char)((nTime >> 16) & 0xff);
					((char*)pHeader)[5] = (char)((nTime >> 8) & 0xff);
					((char*)pHeader)[6] = (char)(nTime & 0xff);
					((char*)pHeader)[7] = (char)((nTime >> 24) & 0xff);
					bChange = true;
				}
			}
			else if (pHeader[0] == 0x12)
			{
			}
			else
			{
				assert(0);
			}
		}
		return nDataLen;
	}
	return 0;
}


bool CFlvData::OnDataCome(char* pData, int nLen, void * pUser)
{
	DataInfo * pdi = (DataInfo *)pUser;
	bool bChange = false;
	if (nLen == 4)
	{
		//前一个tag的尺寸
		int nLastBufLen = ntohl(*(int*)pData);
		if (nLastBufLen > 0)
		{
			assert(nLastBufLen == pdi->nPackageLen+0x0b);
		}
		pdi->dataPrivider.SetNextNeed(0x0b, true);
	}
	else if (nLen == 0x0b)
	{
		//flv tag头

		pdi->nPackageLen = DoProcHeader(pdi->sock, &pData[0], bChange);
		pdi->dataPrivider.SetNextNeed(pdi->nPackageLen, false);
	}
	else
	{
		//flv数据部分
		pdi->dataPrivider.SetNextNeed(0x4, false);
	}
	return bChange;
}

bool CFlvData::OnRecvData(void* s, char * pData, int nLen)
{
	int nInitOffset = 0;
	if (!FilterFlvBuf(s, pData, nLen, nInitOffset))
	{
		return false;
	}

	if (m_flvMap.count(s))
	{
		DataInfo * pdi = m_flvMap[s];
		if (pdi)
		{
			pdi->dataPrivider.OnRecvData(&pData[nInitOffset], nLen - nInitOffset);
		}
	}
	return true;
}