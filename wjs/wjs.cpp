﻿
// wjs.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "wjs.h"
#include "wjsDlg.h"

#include "..\DllInjector\dllinjector.h"
#ifdef _DEBUG
#pragma comment(lib, "..\\debug\\DllInjector.lib")
#else
#pragma comment(lib, "..\\release\\finder.lib")
#endif


DWORD dwId = 0;
HWND hWndGame = NULL;

bool sendbuf(char * strData)
{
	int sock;
	//sendto中使用的对方地址
	struct sockaddr_in toAddr;
	//在recvfrom中使用的对方主机地址
	struct sockaddr_in fromAddr;
	int fromLen = 0;
	char recvBuffer[128];
	bool bok = false;
	do 
	{
		sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
		if(sock < 0)
		{
			break;
		}
		
		memset(&toAddr,0,sizeof(toAddr));
		toAddr.sin_family=AF_INET;
		toAddr.sin_addr.s_addr=inet_addr("127.0.0.1");
		toAddr.sin_port = htons(4011);

		char buf[128] = {0};
		strcpy(buf, strData);
		if(sendto(sock,buf,strlen(buf),0,(struct sockaddr*)&toAddr,sizeof(toAddr)) < 1)
		{
			break;
		}
		bok = true;
// 		fromLen = sizeof(fromAddr);
// 		if(recvfrom(sock,recvBuffer,128,0,(struct sockaddr*)&fromAddr,&fromLen)<0)
// 		{
// 			break;
// 		}
	} while (0);


	printf("recvfrom() result:%s\r\n",recvBuffer);
	closesocket(sock);

	return bok;
}



#ifdef _DEBUG
#define new DEBUG_NEW
#endif
HWND g_hMsgWndDest = NULL;
HWND g_hMsgWnd = NULL;
int g_nHotKeyID1 = 100;
int g_nHotKeyID2 = 101;
int g_nHotKeyID3 = 102;
// CwjsApp

BEGIN_MESSAGE_MAP(CwjsApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CwjsApp construction

CwjsApp::CwjsApp()
{
	// support Restart Manager
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}



//#define TestMode

// The one and only CwjsApp object

CwjsApp theApp;



BOOL RemoteEject(DWORD dwProcessID, HMODULE hModule)
{
	BOOL bRetCode = FALSE;
	HANDLE hProcess = NULL;
	HANDLE hThread = NULL;

	PTHREAD_START_ROUTINE pfnThreadRoutine;

	do
	{
		//获得想要注入代码的进程的句柄
		hProcess = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessID);
		if (hProcess == NULL)
			break;

		//获得LoadLibraryA在Kernel32.dll中的真正地址
		pfnThreadRoutine = (PTHREAD_START_ROUTINE)::GetProcAddress(GetModuleHandle(TEXT("Kernel32")), "FreeLibraryA");
		if (pfnThreadRoutine == NULL)
			break;

		//创建远程线程，并通过远程线程调用用户的DLL文件
		hThread = ::CreateRemoteThread(hProcess, NULL, 0, pfnThreadRoutine, (LPVOID)hModule, 0, NULL);
		if (hThread == NULL)
			break;

		//等待远程线程终止
		::WaitForSingleObject(hThread, INFINITE);
	}while(FALSE);

	if (hThread != NULL)
	{
		DWORD dwExitCode;
		::GetExitCodeThread(hThread, &dwExitCode);
		bRetCode = (BOOL)dwExitCode;
		::CloseHandle(hThread);
	}
	if (hProcess != NULL)
	{
		::CloseHandle(hProcess);
	}
	return bRetCode;
}



DWORD DoInject()
{
	// TODO: Add your control notification handler code here
	do 
	{
		if (hWndGame && !::IsWindow(hWndGame))
		{
			hWndGame = NULL;
		}

		//hWndGame = FindWindow(NULL, "维加斯 - Google Chrome");
		if (!hWndGame)
		{
			hWndGame = FindWindow("Chrome_WidgetWin_1", NULL);
		}
		if(!hWndGame)
		{
			hWndGame = FindWindow(NULL, "维加斯 - 360安全浏览器 8.0");
			//	pWnd = FindWindow("360se6_Frame", NULL);
		}
		if(!hWndGame)
		{
			hWndGame = FindWindow(NULL, "维加斯 - 360安全浏览器 8.1");
			//	pWnd = FindWindow("360se6_Frame", NULL);
		}

		if (hWndGame)
		{
			dwId = 0;
			GetWindowThreadProcessId(hWndGame, &dwId);
			if (dwId != 0)
			{
				TCHAR strFile[MAX_PATH] = {0};
				GetModuleFileName(NULL, strFile, MAX_PATH);
				CString sFile = strFile;
				CString sName = sFile.Right(sFile.GetLength() - sFile.ReverseFind('\\'));
				sFile.Replace((LPCTSTR)sName, "\\eikn.dll");

				if(InjectDll(dwId, sFile, LoadLib, ThreadHijacking))
				{
					break;
				}
				dwId = 0;
			}
		}
		OutputDebugStringA(">>> 注入失败!");
		//MessageBox(0,"Network connection faild","message",MB_OK);
	} while (0);

	return dwId;

	//CDialogEx::OnOK();
}


CString GetHostbyName(const char * HostName)
{
	CString strIPAddress=_T("");
	int WSA_return;
	WSADATA WSAData;

	WSA_return=WSAStartup(0x0202,&WSAData);
	/* 结构指针 */ 
	HOSTENT *host_entry;
	if(WSA_return==0)
	{
		/* 即要解析的域名或主机名 */
		host_entry=gethostbyname(HostName);
		if(host_entry!=0)
		{
			strIPAddress.Format(_T("%d.%d.%d.%d"),
				(host_entry->h_addr_list[0][0]&0x00ff),
				(host_entry->h_addr_list[0][1]&0x00ff),
				(host_entry->h_addr_list[0][2]&0x00ff),
				(host_entry->h_addr_list[0][3]&0x00ff));
		}
	}
	return strIPAddress;
}

DWORD g_dwPid = 0;


LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_HOTKEY:
		{
			OutputDebugStringA(">>> hotkey 1");
			if (wParam == g_nHotKeyID1)
			{
				if (!hWndGame || !IsWindow(hWndGame))
				{
					g_dwPid = DoInject();
				}
				else
				{
					OutputDebugStringA(">>> 窗口仍有效");
				}
				
				OutputDebugStringA(">>> hotkey on");
				if(sendbuf("****on"))
				{
					OutputDebugStringA(">>> sendbuf on ok");
				}
				else
				{
					OutputDebugStringA(">>> sendbuf on faild");
				}
			}
			else if (wParam == g_nHotKeyID2)
			{
				OutputDebugStringA(">>> hotkey off");
				if(sendbuf("****off"))
				{
					OutputDebugStringA(">>> sendbuf off ok");
				}
				else
				{
					OutputDebugStringA(">>> sendbuf off faild");
				}
			}
			else if (wParam == g_nHotKeyID3)
			{
				sendbuf("****quit");

				//RemoteEject(g_dwPid, );

				DestroyWindow(hWnd);
				//SendMessage(hWnd, WM_CLOSE, 0,0);
			}
			OutputDebugStringA(">>> hotkey 4");
		}
		break;
	case WM_DESTROY:
		UnregisterHotKey(hWnd, g_nHotKeyID1);
		UnregisterHotKey(hWnd, g_nHotKeyID2);
		UnregisterHotKey(hWnd, g_nHotKeyID3);
		PostQuitMessage(0);//可以使GetMessage返回0
		break;
	default:
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);

}



bool DoCreateWnd(HINSTANCE hInst)
{
	//注册窗口类

	WNDCLASSEX wce = { 0 };
	wce.cbSize = sizeof(wce);
	wce.cbClsExtra = 0;
	wce.cbWndExtra = 0;
	wce.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wce.hCursor = NULL;
	wce.hIcon = NULL;
	wce.hIconSm = NULL;
	wce.hInstance = hInst;
	wce.lpfnWndProc = WndProc;
	wce.lpszClassName = "EikMsgWndA";
	wce.lpszMenuName = NULL;
	wce.style = CS_HREDRAW | CS_VREDRAW;
	ATOM nAtom = RegisterClassEx(&wce);
	if (!nAtom )
	{
		return false;
	}

	g_hMsgWnd = CreateWindowEx(0, "EikMsgWndA", "", WS_POPUPWINDOW, 0, 0, 0, 0, NULL, NULL, hInst, NULL);
	if (!g_hMsgWnd)
	{
		return false;
	}


	//向系统注册热键:ALT+0



	BOOL bKeyRegistered1 =RegisterHotKey(g_hMsgWnd, g_nHotKeyID1, MOD_CONTROL,VK_F1);
	BOOL bKeyRegistered2 =RegisterHotKey(g_hMsgWnd, g_nHotKeyID2, MOD_CONTROL,VK_F2);
	BOOL bKeyRegistered3 =RegisterHotKey(g_hMsgWnd, g_nHotKeyID3, MOD_CONTROL,VK_F3);
	if (bKeyRegistered1 && bKeyRegistered2)
	{
		OutputDebugStringA(">>> hotkey reg ok");
	}
	else
	{
		OutputDebugStringA(">>> hotkey reg faild");
	}

	return true;
}



// CwjsApp initialization

BOOL CwjsApp::InitInstance()
{
	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();


	AfxEnableControlContainer();

	// Create the shell manager, in case the dialog contains
	// any shell tree view or shell list view controls.
	CShellManager *pShellManager = new CShellManager;

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T("Local AppWizard-Generated Applications"));

	CString sIp = GetHostbyName("qqyonghu888.3322.org");
	if (sIp != "192.168.1.140")
	{
		return FALSE;
	}

#ifdef TestMode

	CwjsDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}
#else
	DoCreateWnd(AfxGetInstanceHandle());

	g_dwPid = DoInject();
	//if(g_dwPid)
	{
		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0))  //获取消息
		{
			TranslateMessage(&msg);    //将虚拟键消息转换为字符消息。字符消息被发送到调用线程的消息队列，在下一次线程调用GetMessage或PeekMessage函数时读取
			DispatchMessage(&msg);     //将消息分派给窗口过程。它通常用于分派由GetMessage函数检索的消息。
		}
	}
#endif



// 
// 	// Delete the shell manager created above.
// 	if (pShellManager != NULL)
// 	{
// 		delete pShellManager;
// 	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

