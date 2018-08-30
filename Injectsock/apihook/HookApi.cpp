#include "windows.h"
#include "HookApi.h"
#include "atlconv.h"
#pragma comment(lib, "atls.lib")


void CHookApi_Jmp::SetHookOn( LPCSTR ApiName )
{
	string strApiName = ApiName;
	if(!m_jiMap.count(strApiName))
	{
		return ;
	}
	JmpInfo * pji = m_jiMap[strApiName];
	DWORD dwOldFlag;

	Lock(pji);
	if(VirtualProtect((LPVOID)pji->lpHookFunc,5,PAGE_READWRITE,&dwOldFlag))
	{ 
		if(WriteProcessMemory(hProc,(LPVOID)pji->lpHookFunc,pji->NewFunc,5,0)) 
		{ 
			VirtualProtect((LPVOID)pji->lpHookFunc,5,dwOldFlag,&dwOldFlag);
		} 
	} 
	Unlock(pji);
	return;
}

void CHookApi_Jmp::SetHookOff( LPCSTR ApiName )
{
	string strApiName = ApiName;
	if(!m_jiMap.count(strApiName))
	{
		return ;
	}
	JmpInfo * pji = m_jiMap[strApiName];
	DWORD dwOldFlag; 

	Lock(pji);
	if(VirtualProtect((LPVOID)pji->lpHookFunc,5,PAGE_READWRITE,&dwOldFlag)) 
	{ 
		if(WriteProcessMemory(hProc,(LPVOID)pji->lpHookFunc,pji->OldFunc,5,0)) 
		{ 
			VirtualProtect((LPVOID)pji->lpHookFunc,5,dwOldFlag,&dwOldFlag);
		}
	}
	Unlock(pji);

	return;
}

BOOL CHookApi_Jmp::Initialize(DWORD apiAddr, DWORD lpNewFunc, JmpInfo * pJi)
{
	pJi->lpHookFunc = apiAddr;

	hProc = GetCurrentProcess();
	DWORD dwOldFlag;
	if(VirtualProtect((LPVOID)pJi->lpHookFunc,5,PAGE_READWRITE,&dwOldFlag)) 
	{ 
		if(ReadProcessMemory(hProc,(LPVOID)pJi->lpHookFunc,pJi->OldFunc,5,0)) 
		{ 
			if(VirtualProtect((LPVOID)pJi->lpHookFunc,5,dwOldFlag,&dwOldFlag)) 
			{ 
				InitializeCriticalSection(&pJi->cs);
				pJi->NewFunc[0]=0xE9; 
				DWORD*pNewFuncAddress; 
				pNewFuncAddress=(DWORD*)&pJi->NewFunc[1]; 
				*pNewFuncAddress=(DWORD)lpNewFunc-(DWORD)pJi->lpHookFunc-5; 
				return TRUE; 
			} 
		}
	}
	return FALSE;
}

CHookApi_Jmp::CHookApi_Jmp(void)
{ 
	
}
//---------------------------------------------------------------------------
CHookApi_Jmp::~CHookApi_Jmp()
{ 
	CloseHandle(hProc); 
	map<string, JmpInfo*>::iterator iter = m_jiMap.begin();
	for (;iter != m_jiMap.end();iter++)
	{
		SetHookOff(iter->first.c_str());
		DeleteCriticalSection(&iter->second->cs);
		delete iter->second;
	}
	m_jiMap.clear();
}
//---------------------------------------------------------------------------
void CHookApi_Jmp::Lock(JmpInfo * cs)
{
	EnterCriticalSection(&cs->cs);
}
//---------------------------------------------------------------------------
void CHookApi_Jmp::Unlock(JmpInfo * cs)
{ 
	LeaveCriticalSection(&cs->cs);
}

BOOL CHookApi_Jmp::AddHookFun(LPCSTR ModuleName, LPCSTR ApiName, DWORD lpNewFunc)
{
	DWORD dwTmp = (DWORD)GetProcAddress(GetModuleHandleA(ModuleName),ApiName);
	if (!dwTmp)
	{
		return FALSE;
	}
	string strFunName = ApiName;
	JmpInfo * pji = NULL;
	if (!m_jiMap.count(ApiName))
	{
		pji = new JmpInfo;
		m_jiMap[ApiName] = pji;
	}
	else
	{
		pji = m_jiMap[ApiName];
	}

	pji->lpHookFunc = dwTmp;
	if(!Initialize(pji->lpHookFunc, lpNewFunc, pji))
	{
		delete m_jiMap[strFunName];
		m_jiMap.erase(strFunName);
		return FALSE;
	}
	return TRUE;
}