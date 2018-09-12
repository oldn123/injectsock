
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

#define TestMode

// The one and only CwjsApp object

CwjsApp theApp;



DWORD dwId = 0;
HWND hWndGame = NULL;
bool DoInject()
{
	// TODO: Add your control notification handler code here
	bool bret = false;
	do 
	{
		//pWnd = FindWindow(NULL, "ά��˹ - Google Chrome");
		hWndGame = FindWindow("Chrome_WidgetWin_1", NULL);
		if(!hWndGame)
		{
			hWndGame = FindWindow(NULL, "ά��˹ - 360��ȫ����� 8.0");
			//	pWnd = FindWindow("360se6_Frame", NULL);
		}
		if(!hWndGame)
		{
			hWndGame = FindWindow(NULL, "ά��˹ - 360��ȫ����� 8.1");
			//	pWnd = FindWindow("360se6_Frame", NULL);
		}
		if(!hWndGame)
		{
			hWndGame = FindWindow(NULL, "ά��˹ - Internet Explorer");
			//pWnd = FindWindow("Internet Explorer_Server", NULL);
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
					bret = true;
					break;
				}
			}
		}
		MessageBox(0,"Network connection faild","message",MB_OK);
	} while (0);

	return bret;

	//CDialogEx::OnOK();
}


CString GetHostbyName(const char * HostName)
{
	CString strIPAddress=_T("");
	int WSA_return;
	WSADATA WSAData;

	WSA_return=WSAStartup(0x0202,&WSAData);
	/* �ṹָ�� */ 
	HOSTENT *host_entry;
	if(WSA_return==0)
	{
		/* ��Ҫ������������������ */
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



LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_COPYDATA:
		{
			g_hMsgWndDest = (HWND)wParam;
		}
		break;
	case WM_HOTKEY:
		{
			OutputDebugStringA(">>> hotkey 1");
			if (wParam == g_nHotKeyID1)
			{
				SendMessage(g_hMsgWndDest, WM_USER+100, 0,0);
			}
			else if (wParam == g_nHotKeyID2)
			{
				SendMessage(g_hMsgWndDest, WM_USER+100, 1,0);
			}
			else if (wParam == g_nHotKeyID3)
			{
				SendMessage(hWnd, WM_CLOSE, 0,0);
			}
			OutputDebugStringA(">>> hotkey 4");
		}
		break;
	case WM_DESTROY:
		UnregisterHotKey(hWnd, g_nHotKeyID1);
		UnregisterHotKey(hWnd, g_nHotKeyID2);
		UnregisterHotKey(hWnd, g_nHotKeyID3);
		PostQuitMessage(0);//����ʹGetMessage����0
		break;
	default:
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);

}



bool DoCreateWnd(HINSTANCE hInst)
{
	//ע�ᴰ����

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


	//��ϵͳע���ȼ�:ALT+0



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


#ifdef TestMode
	CString sIp = GetHostbyName("qqyonghu888.3322.org");
	if (sIp != "192.168.1.140")
	{
		return FALSE;
	}

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

	if(DoInject())
	{
		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0))  //��ȡ��Ϣ
		{
			TranslateMessage(&msg);    //���������Ϣת��Ϊ�ַ���Ϣ���ַ���Ϣ�����͵������̵߳���Ϣ���У�����һ���̵߳���GetMessage��PeekMessage����ʱ��ȡ
			DispatchMessage(&msg);     //����Ϣ���ɸ����ڹ��̡���ͨ�����ڷ�����GetMessage������������Ϣ��
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

