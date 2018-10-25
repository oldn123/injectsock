#include "WebSocketData.h"
#include <stdio.h>
#include <memory>
#include <stack>
#include <assert.h>
//Opcode����
#define OPCODE_TEXT      1
#define OPCODE_BINARY    2
#define OPCODE_CLOSE     8
#define OPCODE_PING      9
#define OPCODE_PONG      10
//һ֡���ݽ������
#define DATA_CONTINUE    0
#define DATA_FINAL       1

bool TestInsertPackage(char * pInputData, int& nLen, int nMax, char * pInsert, int nInsLen, int am)
{
	if (nLen + nInsLen > nMax || am == 0)
	{
		return false;
	}

	if ((am & CWebSocketData::eEnd) == CWebSocketData::eEnd)
	{
		nLen += nInsLen;
	}
	else
		if ((am & CWebSocketData::eBegin) == CWebSocketData::eBegin)
		{
			nLen += nInsLen;
		}
		else
		{
			assert(0);
			return false;
		}
		return true;
}

bool InsertPackage(char * pInputData, int& nLen, int nMax, char * pInsert, int nInsLen, int am)
{
	if (nLen + nInsLen > nMax || am == 0)
	{
		return false;
	}

	if ((am & CWebSocketData::eEnd) == CWebSocketData::eEnd)
	{
		memcpy(&pInputData[nLen], pInsert, nInsLen);
		nLen += nInsLen;
	}
	else
		if ((am & CWebSocketData::eBegin) == CWebSocketData::eBegin)
		{
			char * pBuf = new char[nMax];
			memcpy(pBuf, pInsert, nInsLen);
			memcpy(&pBuf[nInsLen], pInputData, nLen);
			memcpy(pInputData, pBuf, nLen + nInsLen);
			delete [] pBuf;
			nLen += nInsLen;
		}
		else
		{
			assert(0);
			return false;
		}
		return true;
}


bool CWebSocketData::_GetAddressBySocket(SOCKET s, string & sSock)
{
// 
// 	SOCKADDR_IN addr;
// 	memset(&addr, 0, sizeof(addr));
// 	int nAddrLen = sizeof(addr);
// 
// 	//�����׽��ֻ�ȡ��ַ��Ϣ
// 	if(::getpeername(s, (SOCKADDR*)&addr, &nAddrLen) != 0)
// 	{
// 		assert(0);
// 		return false;
// 	}

	char sBuf[50] = {0};
	sprintf(sBuf, "0x%x", s);
//	sprintf(sBuf, "%d.%d.%d.%d:%d", addr.sin_addr.S_un.S_un_b.s_b1,addr.sin_addr.S_un.S_un_b.s_b2,addr.sin_addr.S_un.S_un_b.s_b3,addr.sin_addr.S_un.S_un_b.s_b4, addr.sin_port);
	sSock = sBuf;
	return true;
}

bool CWebSocketData::GetAddressBySocket(SOCKET s, string & sSock)
{
	if(m_convMap.count(s))
	{
		sSock = m_convMap[s];
		return true;
	}
	bool bok = _GetAddressBySocket(s, sSock);
	if (bok)
	{
		m_convMap[s] = sSock;
	}
	return bok;
}



CWebSocketData::DataInfo * CWebSocketData::FixRemoveMsg( void * s, char * pData, int & nLen, int maxLen )
{
	string sSock;
	if(!GetAddressBySocket((SOCKET)s, sSock))
	{
		assert(0);
	}

	if (m_removeMap.count(sSock))
	{
		DataInfo * pdi = m_removeMap[sSock];
		if(InsertPackage(pData, nLen, maxLen, pdi->pData, pdi->nPackageLen - pdi->nLessBufLen, CWebSocketData::eBegin))
		{
			char sOut[100] = {0};
			sprintf(sOut, ">>> ǿ�лָ�%d���ֽ�-�ɹ�\n", pdi->nPackageLen - pdi->nLessBufLen);
			OutputDebugStringA(sOut);

			pdi->nLessBufLen = pdi->nPackageLen; //����ò�������⣬��ȥ�ٿ���//�޸�ֵ

			m_removeMap.erase(sSock);
			return pdi;
		}
		else
		{
			char sOut[100] = {0};
			sprintf(sOut, ">>> ǿ�лָ�%d���ֽ�-ʧ��\n", pdi->nPackageLen - pdi->nLessBufLen);
			OutputDebugStringA(sOut);

			return nullptr;
		}
	}
	
	return nullptr;
}

CWebSocketData::CWebSocketData(CWSStreamInterface * p)
{
	m_bShadowMode = false;
	m_pWSNotify = p;
}


CWebSocketData::~CWebSocketData(void)
{
}


bool IsWsHeader(char *p_data, unsigned int ui_recv_data_size)
{
	int i_final = DATA_CONTINUE;
	char *p_tmp = p_data;
	if (ui_recv_data_size == 0) //ʵ�ʲ����ܷ�����
	{
		//���ݸ�ʽ����ȷ
		//syslog(LOG_INFO, "Received information format error!\n");
		return 0;
	}
	//ȡ�ý���λ
	if (*p_tmp & 0x80)
	{
		i_final = DATA_FINAL;
	}

	if (i_final == DATA_CONTINUE)
	{
		return 0;
	}
	int i_opcode = *p_tmp & 0x7F;
	//����֡(����ǰ��ȡ������Opcode�������͵�֡)
	if (i_opcode == 0)
	{
		//���ݸ�ʽ����ȷ
		//syslog(LOG_INFO, "Received information format error!\n");
		return 0;
	}

	switch(i_opcode)
	{
	case OPCODE_TEXT:
	case OPCODE_BINARY :
	case OPCODE_CLOSE:
	case OPCODE_PING:
	case OPCODE_PONG:
		break;
	default:
		return 0; //error fmt
	}

	return true;
}


bool CWebSocketData::IsWSMsg( void * s, char * pData, int nLen )
{
	string sSock;
	GetAddressBySocket((SOCKET)s, sSock);
	if( strstr(pData, "HTTP/1.1 101 Switching Protocols") &&
		strstr(pData, "Upgrade: websocket") &&
		strstr(pData, "Connection: Upgrade") &&
		strstr(pData, "Sec-WebSocket-Accept:"))
	{
		m_curDataInfoMap[sSock] = 0;
	}
	else
	{
		if (m_curDataInfoMap.count(sSock))
		{
			return true;
		}
	}


	return false;
}

void CWebSocketData::OnCloseHandle(void * s)
{
	string sSock;
	if(m_convMap.count((SOCKET)s))
	{
		sSock = m_convMap[(SOCKET)s];
	}
	else
	{
		return;
	}

	if (m_removeMap.count(sSock))
	{
		m_removeMap.erase(sSock);
	}

	if (m_cacheCounter.count(sSock))
	{
		m_cacheCounter.erase(sSock);
	}	

	if (m_cacheMap.count(sSock))
	{
		m_cacheMap.erase(sSock);
	}

	if (m_curDataInfoMap.count(sSock))
	{
		m_curDataInfoMap.erase(sSock);
	}

	if (m_wsMap.count(sSock))
	{
		queue<DataInfo*> & arr = m_wsMap[sSock];
		while(arr.size())
		{
			delete [] arr.front()->pData;
			delete arr.front();
			arr.pop();
		}
		m_wsMap.erase(sSock);
	}

	if (m_convMap.count((SOCKET)s))
	{
		m_convMap.erase((SOCKET)s);
	}

}

int CWebSocketData::ReadDelayData(void * s, char * pData, int& nLen, int maxLen, int am)
{
	string sSock;
	if(!GetAddressBySocket((SOCKET)s, sSock))
	{
		assert(0);
	}

	map<string, queue<DataInfo *>> & lmap = m_cacheMap;//m_wsMap;//m_cacheMap;
	char * pDstData = pData;
	int nDstLen = maxLen - nLen;
	int sended = 0;
	if (lmap.count(sSock))
	{
		stack<DataInfo *> tmpDataStack;
		int nTestLen = nLen;
		do 
		{
			if (lmap[sSock].size() > 0)
			{
				auto & iter = lmap[sSock].front();
				if ((int)(GetTickCount() - iter->dwPushTime) >= (int)iter->dwNeedDelay)
				{
					int nsz = (iter->nPackageLen - iter->nLessBufLen) - iter->nPopLen;
					if(TestInsertPackage(pData, nLen, maxLen, &iter->pData[iter->nPopLen], nsz, am))
					{		
						char sOut[100] = {0};
						sprintf(sOut, ">>> [%x]�ָ�+++++++++++++++++++++++++++++++++++++++++++++++++++++++++Idx:%d, ʱ����:%d\n", s, iter->nIdx, (int)iter->dwNeedDelay);
						OutputDebugStringA(sOut);
						sended += nsz;

						if (iter->nLessBufLen < 1)
						{
							tmpDataStack.push(iter);
							lmap[sSock].pop();
							continue;	//�������Ƿ�����һ��
						}
						else
						{
							assert(0);
							//ֻ�������
							iter->nPopLen += sended;
							break;
						}
					}
				}
				else
				{
					if (sended > 0)
					{
						break;
					}
					char sOut[200] = {0};
					sprintf(sOut, ">>> [%x]ʱ��δ����������ȡ��retun -1, --real-len:%d,max-len:%d, data(len:%d, waittime:%d)\n", s, nLen, maxLen, iter->nPackageLen, iter->dwNeedDelay);
					OutputDebugStringA(sOut);
					break;
				}
			}
			else
			{
				break;
			}
		} while (true);		

		if (sended > 0)
		{
			//Ϊ�˱�֤˳��
			int naddsize = 0;
			char * pInsert = new char[sended];
			while(tmpDataStack.size())
			{
				auto iter = tmpDataStack.top();
				tmpDataStack.pop();
				naddsize += iter->nPackageLen;
				memcpy(&pInsert[sended - naddsize], iter->pData, iter->nPackageLen);
				delete [] iter->pData;
				delete iter;
			}
			assert(naddsize == sended);

			if(InsertPackage(pData, nLen, maxLen, pInsert, sended, am))
			{
				
			}
			else
			{
				assert(0);
			}

			delete [] pInsert;
		}	
	}
	return sended;
}

int _FindData(LPBYTE pData, int nDataSize, LPBYTE pFind, int nFindSize)
{
	int nFindIdx = -1;

	for (int i = 0; i <= nDataSize - nFindSize; i++)
	{
		if(memcmp(&pData[i], pFind, nFindSize) == 0)
		{
			nFindIdx = i;
			i += nFindSize;
		}
	}
	return nFindIdx;
}

bool RemoveMsg(char * pSrcBuf, int & nLen, char * sRemove, int nRemLen, int nRemovePos)
{
	if (nRemLen > nLen)
	{
		memset(pSrcBuf, 0, nLen);
	//	assert(0);
		return true;
	}

	int nFind = nRemovePos;
	if (sRemove)
	{
		assert(memcmp(&pSrcBuf[nRemovePos], sRemove, nRemLen) == 0);
		
		int nassert = _FindData((LPBYTE)pSrcBuf, nLen, (LPBYTE)sRemove, nRemLen);
		assert(nassert == nRemovePos);

		if (nassert < 0)
		{
			//assert(0);
			int nFind = _FindData((LPBYTE)pSrcBuf, nLen, (LPBYTE)&sRemove[2], nRemLen - 2);
			if (nFind)
			{
				memset(pSrcBuf, 0, nRemLen - 2);
			}
			else
			{
				assert(false);
			}
			return true;
		}
	}

	if (nLen >= nFind + nRemLen)
	{		
		memset(&pSrcBuf[nFind], 0, nRemLen);
		memcpy(&pSrcBuf[nFind], &pSrcBuf[nFind + nRemLen], nLen - (nFind + nRemLen));
		nLen -= nRemLen;
	}
	else
	{
		assert(0);
		return false;
	}

	return true;
}

bool CWebSocketData::OnRecvData( void * s, char * pData, int & nLen, int maxLen, int & nAlignMode)
{
	int opcode = 0;
	int i_ret = 0;
	int i_final = 0;
	char * pRecvData = pData;
	int nLenBak = nLen;
	unsigned int ui_nbytes = nLen;
	unsigned int ui_data_size = 0;
	unsigned int ui_data_total_size = 0;
	nAlignMode = eNo;

	DataInfo * pDataInfoFix = FixRemoveMsg(s, pData, nLen, maxLen);
	if (pDataInfoFix)
	{
		ui_nbytes = nLen;
	}

	if (!IsWSMsg(s, pData, nLen))
	{
		if (!IsWsHeader(pData, nLen))
		{
			return false;
		}	
	}

	string sSock;
	if(!GetAddressBySocket((SOCKET)s, sSock))
	{
		assert(0);
	}

	DataInfo *& pCurDataInfo = m_curDataInfoMap[sSock];
	do 
	{
		if (pCurDataInfo)
		{
			if (pCurDataInfo->nLessBufLen > 0)
			{
				if (ui_nbytes >= pCurDataInfo->nLessBufLen)
				{
					pCurDataInfo->nDataPos = pRecvData - pData;
					int nMemPos = pCurDataInfo->nPackageLen - pCurDataInfo->nLessBufLen;
					memcpy(&pCurDataInfo->pData[nMemPos], pRecvData, pCurDataInfo->nLessBufLen); 
					if (pCurDataInfo->dwPushTime == 0)
					{
						pCurDataInfo->dwPushTime = GetTickCount();
						assert(m_wsMap[sSock].size() == 0);
						m_wsMap[sSock].push(pCurDataInfo);
					}
					bool bCrossDel = false;
					bool sm = m_bShadowMode;
					if (m_pWSNotify)
					{
						m_pWSNotify->OnWSDataCome((SOCKET)s, pCurDataInfo->pData, pCurDataInfo->nDataOffset, pCurDataInfo->nPackageLen, pCurDataInfo->dwNeedDelay, pCurDataInfo->nIdx);

						if (pCurDataInfo->dwNeedDelay)
						{
							bCrossDel = true;
						
							m_cacheMap[sSock].push(pCurDataInfo);
							if(RemoveMsg(pData, nLen, &pCurDataInfo->pData[nMemPos], pCurDataInfo->nLessBufLen, pCurDataInfo->nDataPos))
							{
								char sOut[100] = {0};
								sprintf(sOut, ">>> �ݴ�ɹ�----------------------Idx:%d, len:%d, timeDelay:%d\n", pCurDataInfo->nIdx, pCurDataInfo->nPackageLen, pCurDataInfo->dwNeedDelay);
								OutputDebugStringA(sOut);
							}
							else
							{
								char sOut[100] = {0};
								sprintf(sOut, ">>> �ݴ�ʧ��XXXXXXXXXXXXXXXXXXXXIdx:%d, len:%d, timeDelay:%d\n", pCurDataInfo->nIdx, pCurDataInfo->nPackageLen, pCurDataInfo->dwNeedDelay);
								OutputDebugStringA(sOut);
							}
						}

						//pDi->dwNeedDelay = pCurDataInfo->dwNeedDelay;
					}				

					ui_nbytes -= pCurDataInfo->nLessBufLen;
					pRecvData += pCurDataInfo->nLessBufLen;

					pCurDataInfo->nLessBufLen = 0;
					pCurDataInfo = 0;

					if(!sm)
					{
						//�����洢��ֱ���ͷ�
						if (!bCrossDel)
						{
							delete [] m_wsMap[sSock].front()->pData;
							delete m_wsMap[sSock].front();
						}
						m_wsMap[sSock].pop();
						assert(m_wsMap[sSock].size() == 0);
					}

					if (ui_nbytes <= 0)
					{
						if (ui_nbytes == 0)
						{
							nAlignMode |= (int)eEnd;
						}
						else
						{
							assert(0);	//��������������
							continue;
						}
						break;
					}
				}
				else
				{
					//�������Ҳ���ȥ
					if (pCurDataInfo->dwPushTime == 0)
					{
						pCurDataInfo->dwPushTime = GetTickCount();
						assert(m_wsMap[sSock].size() == 0);
						m_wsMap[sSock].push(pCurDataInfo);
					}
					
					if (ui_nbytes)
					{
						memcpy(&pCurDataInfo->pData[pCurDataInfo->nPackageLen - pCurDataInfo->nLessBufLen], pRecvData, ui_nbytes);
						//assert(*(LPWORD)&pCurDataInfo->pData[0] != 0xc82);
						pCurDataInfo->nLessBufLen -= ui_nbytes;
						//assert(pCurDataInfo->nPackageLen - pCurDataInfo->nLessBufLen == pCurDataInfo->nDataOffset);	//һ��nDataOffsetΪ2, �����ϢͷӦ��Ϊ2������Ҫע��ȥ�����߼���δ���
					}

					if (ui_nbytes > pCurDataInfo->nDataOffset)
					{
						//�������msg id ������ǰһ��Ϣ����ȥ�������ڴ˴�����ǿ���Ƴ�
						RemoveMsg(pData, nLen, 0, ui_nbytes, nLen - ui_nbytes);
					//	nLen -= ui_nbytes;
						m_removeMap[sSock] = pCurDataInfo;

						char sOut[100] = {0};
						sprintf(sOut, ">>> ǿ���Ƴ�%d���ֽ�\n", ui_nbytes);
						OutputDebugStringA(sOut);
					}	
					else
					{
						if (pCurDataInfo->nDataOffset == 2)
						{
							pData[nLen - ui_nbytes + 1] = 0;
							m_removeMap[sSock] = pCurDataInfo;
							char sOut[100] = {0};
							sprintf(sOut, ">>> ǿ�б��Ϊ0���ֽ�\n");
							OutputDebugStringA(sOut);
						}
						else
						{
							assert(0);
						}
					}
					break;
				}
			}	
		}

		//�����յ��ĵ�һ����Ӧ���õ�ʵ�ʸ�֡�����ݳ���
		i_ret = parse_recv_data(pRecvData, ui_nbytes, &opcode, &i_final, &ui_data_size);
		if (i_ret == 0) //ʵ�ʲ��ᷢ����
		{
			break;
		}

		if (pRecvData == pData)
		{
			nAlignMode |= (int)eBegin;
		}

		//���õ�ǰ����
		if (!pCurDataInfo)
		{
			m_cacheCounter[sSock]++;
			assert(m_wsMap[sSock].size() == 0);
			pCurDataInfo = new DataInfo;
			pCurDataInfo->nDataOffset = i_ret;
			pCurDataInfo->nPopLen = 0;
			pCurDataInfo->dwNeedDelay = 0;
			pCurDataInfo->dwPushTime  = 0;
			pCurDataInfo->nLessBufLen = ui_data_size;
			pCurDataInfo->nPackageLen = ui_data_size;
			pCurDataInfo->pData = new char[ui_data_size];
			pCurDataInfo->nIdx = m_cacheCounter[sSock];
			pCurDataInfo->nDataPos = pRecvData - pData;
			assert(ui_data_size);
			continue;
		}
		
		if (pCurDataInfo->nPackageLen == 0)
		{
			break;
		}	
	} while (1);


	return m_bShadowMode;
}

/**************************************************************
���ܣ�ȡ������ʵ�ʳ���
����ֵ
0:ʧ��  1:�ɹ�
***************************************************************/
int get_data_len(char *p_data, unsigned int ui_data_len_len
	, unsigned int ui_data_size, unsigned int *pui_data_size)
{
	int i_loop = 0;
	unsigned int ui_size = 0;

	if (ui_data_len_len >= ui_data_size)
	{
		return 0;
	}

	for (i_loop = 0; i_loop < ui_data_len_len; i_loop++)
	{
		ui_size = ((unsigned char)ui_size) << 8;
		ui_size += (unsigned char)*p_data;
		p_data++;
	}

	*pui_data_size = ui_size;
	return 1;
}

/**************************************************************
���ܣ������յ�������
����ֵ
0:ʧ��  1:�ɹ�
***************************************************************/
int CWebSocketData::parse_recv_data(char *p_data, unsigned int ui_recv_data_size
	, int *pi_opcode, int *pi_final, unsigned int *pui_data_size)
{
	int i_ret = 0;
	int i_final = DATA_CONTINUE;
	int i_opcode = 0;
	unsigned int ui_data_len = 0;
	unsigned int ui_data_size = 0;
	char *p_tmp = p_data;

	if (ui_recv_data_size == 0) //ʵ�ʲ����ܷ�����
	{
		//���ݸ�ʽ����ȷ
		//syslog(LOG_INFO, "Received information format error!\n");
		return 0;
	}
	//ȡ�ý���λ
	if (*p_tmp & 0x80)
	{
		i_final = DATA_FINAL;
	}

	if (i_final == DATA_CONTINUE)
	{
		return 0;
	}

	//ȡ��Opcode����
	i_opcode = *p_tmp & 0x7F;
	//����֡(����ǰ��ȡ������Opcode�������͵�֡)
	if (i_opcode == 0)
	{
		if (*pi_opcode == 0)
		{
			//���ݸ�ʽ����ȷ
			//syslog(LOG_INFO, "Received information format error!\n");
			return 0;
		}
	}
	else
	{
		*pi_opcode = i_opcode;
	}

	switch(i_opcode)
	{
	case OPCODE_TEXT:
	case OPCODE_BINARY :
	case OPCODE_CLOSE:
	case OPCODE_PING:
	case OPCODE_PONG:
		break;
	default:
		return 0; //error fmt
	}

	ui_recv_data_size--;
	if (ui_recv_data_size == 0) //ʵ�ʲ����ܷ�����
	{
		//���ݸ�ʽ����ȷ
		//syslog(LOG_INFO, "Received information format error!\n");
		return 0;
	}
	p_tmp++;

	int payloadoffset = 0;
	//�жϸ������Ƿ�������룬������λ���ݳ��ȼ�4������
	if (*p_tmp & 0x80)
	{
		ui_data_size += 4;
		payloadoffset += 4;
	}
	//ȡ�����ݳ���λ
	ui_data_len = *p_tmp & 0x7f;

	//����������ֽ������洢�����ݳ���
	ui_recv_data_size--;
	p_tmp++;
	
	if (ui_data_len == 126)
	{
		i_ret = get_data_len(p_tmp, 2, ui_recv_data_size, &ui_data_len);
		if (i_ret == 0) //ʵ�ʲ����ܷ�����
		{
			return 0;
		}
		//ǰ������1�ֱ���֡��ʽλ�ͳ�������λ������ָ2�������ݳ���λ
		ui_data_size += ui_data_len + 1 + 1 + 2;
		payloadoffset += 1 + 1 + 2;
	}
	//�����8���ֽ������洢�����ݳ���
	else if (ui_data_size == 127)
	{
		i_ret = get_data_len(p_tmp, 8, ui_recv_data_size, &ui_data_len);
		if (i_ret == 0) //ʵ�ʲ����ܷ�����
		{
			return 0;
		}
		//ǰ������1�ֱ���֡��ʽλ�ͳ�������λ������8ָ�������ݳ���λ
		ui_data_size += ui_data_len + 1 + 1 + 8;
		payloadoffset += 1 + 1 + 8;
	}
	//�䱾��ʹ���ʵ�����ݳ���
	else
	{
		//ǰ������1�ֱ���֡��ʽλ�ͳ���λ
		ui_data_size += ui_data_len + 1 + 1;
		payloadoffset += 2;
	}

	*pi_final = i_final;
	*pui_data_size = ui_data_size;
	return payloadoffset;
}