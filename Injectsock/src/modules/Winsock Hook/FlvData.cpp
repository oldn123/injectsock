#include "FlvData.h"
#include <stdio.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <assert.h>

CFlvData::CFlvData(CFlvStreamInterface * p)
{
	m_pFlvNotify = p;
// 	assert(0);
// 	FILE * fp = fopen("D:\\data\\flv\\948242296_266.dat", "rb");
// 	char * pbuf = new char[0x8000];
// 	int ns = fread(pbuf, 1, 0x8000, fp);
// 	OnRecvData(pbuf, pbuf, ns);
// 	fclose(fp);
// 
// 	 fp = fopen("D:\\data\\flv\\948251265_32768.dat", "rb");
// 	 ns = fread(pbuf, 1, 0x8000, fp);
// 	 OnRecvData(pbuf, pbuf, ns);
// 	 fclose(fp);
// 
// 	 fp = fopen("D:\\data\\flv\\948254750_32768.dat", "rb");
// 	 ns = fread(pbuf, 1, 0x8000, fp);
// 	 OnRecvData(pbuf, pbuf, ns);
// 	 fclose(fp);
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
	0x46,0x4c,0x56,0x01,0x05,0x00,0x00,0x00,0x09,0x00,0,0,0
};

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

		if (memcmp(_data, gsFlvSign, 13) == 0)
		{
			_data += 13;
			nOffset = _data - pData;
			pdi->bGetHeader = true; 
		}			
	}
	
	if ((memcmp(pData, "HTTP/", 5) == 0) && (_data = strstr((char*)pData, "Content-Type: video/x-flv")) != NULL)
	{
		OutputDebugStringA(">>> get flv buffer begin~~~~~~~~~");		
		if (!m_flvMap.count(s))
		{
			pdi = new DataInfo;
			m_flvMap[s] = pdi;
		}
		else
		{
			pdi = m_flvMap[s];
		}
		memset(pdi, 0, sizeof(DataInfo));

		_data = strstr(pData, "\r\n\r\n");
		if (_data)
		{
			_data += 4;
		
			if (_data - pData < nLen)
			{
				if (memcmp(_data, gsFlvSign, 13) == 0)
				{
					_data += 13;
					nOffset = _data - pData;
					pdi->bGetHeader = true; 
				}			
			}
			else
			{
				nOffset = _data - pData;
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

int CFlvData::DoProcHeader(void * s, char * pHeader)
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

bool CFlvData::OnRecvData(void* s, char * pData, int nLen)
{
	int nInitOffset = 0;
	if (!FilterFlvBuf(s, pData, nLen, nInitOffset))
	{
		return false;
	}

	//SaveData(pData, nLen);

	DataInfo * pdi = m_flvMap[s];
	char * _strBuf = pData + nInitOffset;
	int nRecvBytes = nLen - nInitOffset;
	if (nRecvBytes == 0)
	{
		return true;
	}
	assert(nRecvBytes > 0);
	int _noffset = 0;

	if (pdi->nLessHeaderLen)
	{
		if (nLen >= pdi->nLessHeaderLen)
		{
			memcpy(&pdi->sHeader[11 - pdi->nLessHeaderLen], _strBuf, pdi->nLessHeaderLen);

			pdi->nLastPackageLen = DoProcHeader(s, &pdi->sHeader[0]);

			if (pdi->nLastPackageLen > 0)
			{
				nRecvBytes -= pdi->nLessHeaderLen;
				_strBuf += pdi->nLessHeaderLen;

				pdi->nLessBufLen = pdi->nLastPackageLen;
				pdi->nLessHeaderLen = 0;
			}
			else
			{
				assert(false);
			}
		}
		else
		{
			memcpy(&pdi->sHeader[11 - pdi->nLessHeaderLen], _strBuf, nLen);
			pdi->nLessHeaderLen -= nLen;
		}	
	}

	if (pdi->nLessEnd)
	{
		nRecvBytes -= pdi->nLessEnd;
		_strBuf += pdi->nLessEnd;
	}

	do 
	{
		if (pdi->nLessHeaderLen)
		{
			break;
		}

		if (pdi->nLessBufLen > 0 && nRecvBytes)
		{
			if (pdi->nLessBufLen - nRecvBytes >= 0)
			{
				//来的数据还不够，还要继续补充
				pdi->nLessBufLen -= nRecvBytes;
			}
			else
			{
				//够用了
				if (pdi->nLessEnd == 0)
				{
					nRecvBytes -= (pdi->nLessBufLen + 4);	
				
					if(nRecvBytes < 0)
					{
						pdi->nLessEnd = -nRecvBytes;
						nRecvBytes = 0;
					}
					else
					{
						_strBuf = (char*)&_strBuf[pdi->nLessBufLen];	
						int nLastBufLen = ntohl(*(int*)_strBuf);	//用于验证前一个buf的尺寸
						if (pdi->nLastPackageLen+11 != nLastBufLen)
						{
							assert(0);		//尺寸校验失败
						}
						_strBuf += 4;
					}			
				}	
				pdi->nLessBufLen = 0;
				assert(nRecvBytes >= 0);
			}
		}

		if (pdi->nLessBufLen > 0 || nRecvBytes < 1)
		{
			break;
		}
		pdi->nLessEnd = 0;
		if (nRecvBytes >= 11)
		{//完整头的情况
			pdi->nLastPackageLen = DoProcHeader(s, &_strBuf[0]);
			if (pdi->nLastPackageLen > 0)
			{
				assert(pdi->nLessBufLen == 0);
				if (pdi->nLastPackageLen + 11 > nRecvBytes)
				{
					pdi->nLessBufLen = pdi->nLastPackageLen + 11 - nRecvBytes;
					nRecvBytes = 0;
				}
				else
				{
					nRecvBytes -= (pdi->nLastPackageLen + 11 + 4);
					if(nRecvBytes < 0)
					{
						pdi->nLessEnd = -nRecvBytes;
						nRecvBytes = 0;
					}
					else
					{
						_strBuf += (pdi->nLastPackageLen + 11);
						_strBuf += 4;
					}			
					continue;
				}
			}
			else
			{
				assert(pdi->nLessBufLen == 0);
				assert(pdi->nLastPackageLen == 0);
				break;
			}
		}
		else
		{
			OutputDebugStringA("****** header need fix *******\n");
			pdi->nLessHeaderLen = 11 - nRecvBytes;
			memcpy(pdi->sHeader, _strBuf, nRecvBytes); 
			assert(pdi->sHeader[0] == 0x08 || pdi->sHeader[0] == 0x09);
			nRecvBytes = 0;
		}		
	} while (TRUE);

	return true;
}