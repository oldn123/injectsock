#pragma once

#include <map>
#include <string>
#include <Psapi.h>   

#include <tlhelp32.h>   

#pragma comment(lib,"Psapi.lib")   

using namespace std;
struct JmpInfo
{
	BYTE OldFunc[8]; 
	BYTE NewFunc[8]; 
	DWORD lpHookFunc; 
	CRITICAL_SECTION cs;
};

class CHookApi_Jmp 
{
public:
	CHookApi_Jmp(void); 
	virtual ~CHookApi_Jmp();

	BOOL AddHookFun(LPCSTR ModuleName, LPCSTR ApiName, DWORD lpNewFunc);
	void SetHookOn(LPCSTR); 
	void SetHookOff(LPCSTR); 
protected:
	BOOL Initialize(DWORD apiAddr, DWORD lpNewFunc, JmpInfo*);
	void Unlock(JmpInfo * cs);
	void Lock(JmpInfo * cs); 

protected: 
	map<string, JmpInfo*>	m_jiMap;
	HANDLE					hProc; 
	//CRITICAL_SECTION		m_cs;
};
