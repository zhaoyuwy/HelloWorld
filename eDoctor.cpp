#include "stdafx.h"
#include"eDoctor.h"
#pragma push_macro("new")
#undef new
#include "module/utility/Crypto.h"
#include "module/deamon/CheckUpdate.h"
#include "module/deamon/CheckUpdate.h"
#pragma pop_macro("new")
#include "module/deamon/ConfigManager.h"
#include "module/web/Browser.h"
#include "module/lua/LuaMainApp.h"
#include <Strsafe.h>
Json::Value UpdateInfo;
//����Ǻ�̨ģ���ȫ�ֶ���ָ��
eDoctorDeamon *g_pEDD=NULL;
//�����ڵ�hwnd
HWND MainHwnd = NULL;
//Ӧ�ó���ʵ���ľ��
HINSTANCE MainInstance;
//��Ϣ���Ӿ��
HHOOK MainShellHook;
MyVedioWindow *g_MyVedioWindow;
OtherVedioWindow *g_OtherVedioWindow;
EDoctorMessageList g_EDMessageList;
HANDLE g_hConsole;
std::string MainPath;

NOTIFYICONDATA eDoctor::m_trayIcon;

eDoctor::eDoctor()
{
}
eDoctor::~eDoctor()
{
}
eDoctor* eDoctor::m_eDoctor=NULL;

//�����򴴽�������ֻ�ܷ���һ��ʵ��
eDoctor* eDoctor::CreateEDoctor()
{
	if(m_eDoctor==NULL)
	{
		m_eDoctor=new eDoctor();
	}
	return m_eDoctor;
}
//����������
void eDoctor::ReleaseEDoctor()
{
	if(m_eDoctor!=NULL)
	{
		delete m_eDoctor;
		m_eDoctor=NULL;
	}
}
//����Ӧ�ó��������Ϣ
void eDoctor::SetInstanceInfo(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPTSTR lpCmdLine,int nCmdShow)
{
	m_hInstance=hInstance;
	m_hPrevInstance=hPrevInstance;
	m_lpCmdLine=lpCmdLine;
	m_nCmdShow=nCmdShow;
}

//������
int eDoctor::CheckUpdate()
{
	//ֱ���ȱȽ�
	//int cmp=CheckUpdate::CompareVersions();
	std::string sPath=VersionConfig::GetInstance()->GetNewVersionPath();

	EDLog("��ʼ������");
	UpdateInfo.clear();
	CheckUpdate::CreateCheckUpdate();
	CheckUpdate::GetInstance()->CheckNewVersion();
	Sleep(500);

	if(UpdateInfo.size()>=0)
	{
		if(UpdateInfo["isConstraintUpdate"].asString()=="1" && VersionConfig::GetInstance()->GetCurVersion()!=UpdateInfo["versionNo"].asString())
		{
			return -2;
		}
		if(UpdateInfo["isNeedUpdate"].asString()=="1" && UpdateInfo["updateUrl"].asString()!="")
		{
			return -1;
		}
	}
	return 0;
	//return -1;
}
//�ƶ�updater��ɾ����ʱĿ¼
void MoveUpdater()
{
	struct _stat st;
	int re=_stat("./dtemp", &st);
	if(re!=0)
	{
		return;
	}
	CopyFile(L"dtemp\\bin\\updater.exe", L"updater.exe", false);
	//RemoveDirectory(L"./dtemp");
	//system("rd /s /q .\\dtemp"); 
	WinExec("cmd /c rd /s /q .\\dtemp",SW_HIDE);
}
//�ж������Ƿ�������ֻ�����û�����һ������
BOOL FindProcess()  
{  
	int i=0;  
	PROCESSENTRY32 pe32;  
	pe32.dwSize = sizeof(pe32);   
	HANDLE hProcessSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);  
	if(hProcessSnap == INVALID_HANDLE_VALUE)  
	{  
		i+=0;  
	}  
	BOOL bMore = ::Process32First(hProcessSnap, &pe32);  
	while(bMore)  
	{  
		//printf(" �������ƣ�%s \n", pe32.szExeFile);  
		if(L"eDoctor.exe"==std::wstring(pe32.szExeFile))  
		{  
			//printf("����������");  
			i+=1;  
		}  
		bMore = ::Process32Next(hProcessSnap, &pe32);  
	}  
	if(i>1){           //����1���ų�����  
		return true;  
	}else{  
		return false;  
	}  
}  



int eDoctor::Run(LPTSTR lpCmdLine)
{
	::OleInitialize(NULL);
	::DefWindowProc(NULL, 0, 0, 0L);
	MainInstance=m_hInstance;
	//static BOOL run_flag = FALSE;
	BOOL isRun = FALSE;
	isRun=FindProcess();;
	std::wstring t = lpCmdLine;
	//MessageBoxW(NULL,t.c_str(),L"��ʾ",MB_OK);
	if (isRun > 0 && (t != std::wstring(L"IsStartUpAgain")))
	{
		MessageBoxW(NULL,L"e����������",L"��ʾ",MB_OK);
		isRun = FALSE;
		return 1;
	}
	if(!InitXLUE())
    {
        MessageBoxW(NULL,L"��ʼ��XLUE ʧ��!",L"����",MB_OK);
        return 1;
    }
	EDLog("��ʼ��XLUE---ok");

	MoveUpdater();
	int nCheckUpdate=CheckUpdate();
	if(nCheckUpdate==-2)
	{
		MessageBox(NULL,L"��ǰ�汾���ͣ��������������°�ͻ��ˣ�",L"��ʾ",0);
		UninitXLUE();
		::OleUninitialize();
		return 0;
	}
	else if(nCheckUpdate==-1)
	{
		EDLog("��ǰ�汾���ͣ���Ҫ����");
		std::string sPath=UpdateInfo["updateUrl"].asString();
		if(sPath!="")
		{
			::ShellExecute(NULL,L"open",L"updater.exe",STR_2_WSTR(sPath).c_str(),L"", SW_SHOW );
			UninitXLUE();
			::OleUninitialize();
			return 0;
		}
	}
	
    if(!LoadMainXAR())
    {
        MessageBoxW(NULL,L"Load XARʧ��!",L"����",MB_OK);
        return 1;
    }
	EDLog("��ʼ��XAR---ok");

	MSG msg;

	// ����Ϣѭ��-----------------------------------------
	//����ʹ��PeekMessage������ߴ���EDoctorMessageList��Ч��
	while(true)
	{
		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if(msg.message==WM_QUIT)
			{
				g_pEDD->m_bReadyToQuit=1;
				g_pEDD->EDEndAV(0);
				
				g_pEDD->EDUnbindCef();
				g_pEDD->ReleaseBrowser();
				g_pEDD->LogoutOnlineState();
				//g_EDMessageList.PostEDTMessage(EM_QUIT, 0, 0, 0);
				continue;
			}
			if(msg.message==WM_CLOSE)
			{
				//EDLog("123");
				/*CefBrowser *broswer=Browser::GetInstance()->GetApp()->GetHandler()->GetFirstBrowser();
				broswer->GetHost()->CloseBrowser(false);
				continue;*/
				if(Browser::GetInstance()->GetApp()!=NULL && Browser::GetInstance()->GetApp()->GetHandler()->IsClosing())
				{
					CefBrowser *broswer=Browser::GetInstance()->GetApp()->GetHandler()->GetFirstBrowser();
					broswer->GetHost()->CloseBrowser(true);
					continue;
				}
			}
			if(msg.message==WM_SYSCOMMAND)
			{
				if(msg.wParam==SC_CLOSE)
				{
					LuaMainApp::LuaEvent_OnBeforeClose();
					continue;
				}
				
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			//����Ϣ��ȥ������
			if(g_EDMessageList.Size()>0)
			{
				EDoctorMessage avMsg;
				g_EDMessageList.GetEDTMessage(avMsg,1);
				if(avMsg.msg==EM_QUIT)//�������˳�������
				{
					EDLog("�յ��˳���Ϣ");;
					break;
				}
				g_pEDD->HandleAVMessage(avMsg);
			}
		}
	}
	UninitXLUE();
	EDLog("����ʼ��XLUE");;

	delete g_MyVedioWindow;
	delete g_OtherVedioWindow;
	::OleUninitialize();
	return (int) msg.wParam;
}
//lua�����Ļص�����
int __stdcall eDoctor::LuaErrorHandle(lua_State* luaState,const wchar_t* pExtInfo,const wchar_t* wcszLuaErrorString,PXL_LRT_ERROR_STACK pStackInfo)
{
    static bool s_bEnter = false;
    if (!s_bEnter)
    {
        s_bEnter = true;
        if(pExtInfo != NULL)
        {
			std::wstring str = wcszLuaErrorString ? wcszLuaErrorString : L"";
            luaState;
            pExtInfo;
            wcszLuaErrorString;
            str += L" @ ";
            str += pExtInfo;

            MessageBoxW(0,str.c_str(),L"Ϊ�˰������ǸĽ�����,�뷴���˽ű�����",MB_ICONERROR | MB_OK);

        }
        else
        {
			MessageBoxW(0,wcszLuaErrorString ? wcszLuaErrorString : L"" ,L"Ϊ�˰������ǸĽ�����,�뷴���˽ű�����",MB_ICONERROR | MB_OK);
        }
        s_bEnter = false;
    }
    return 0;
}

//ע�����lua�����Ķ���
bool eDoctor::InitLuaObjects()
{
	XL_LRT_ENV_HANDLE hEnv = XLLRT_GetEnv(NULL);
	//ע�����ȫ�ֶ���
	if(!LuaMainApp::RegisterGlobalObj(hEnv))
	{
		return false;
	}
	XLLRT_ReleaseEnv(hEnv);
	return true;
}
//��ʼ��bolt
bool eDoctor::InitXLUE()
{
    //��ʼ��ͼ�ο�
    XLGraphicParam param;
    XL_PrepareGraphicParam(&param);
	long result = XL_InitGraphicLib(&param);
    //��ʼ��XLUE,�⺯����һ�����ϳ�ʼ������
    //����˳�ʼ��Lua����,��׼����,XLUELoader�Ĺ���
    result = XLUE_InitLoader(NULL);
	XLGraphicPlusParam plusParam;
	XLGP_PrepareGraphicPlusParam(&plusParam);
	XLGP_InitGraphicPlus(&plusParam);

	//����һ���򵥵Ľű�������ʾ
    XLLRT_ErrorHandle(LuaErrorHandle);

	//��ʼ������ȫ�ֶ���
	if(!InitLuaObjects())
	{
		return false;
	}

	//��ʼ���Լ�����ĸ���ȫ�ֱ���
	g_IM.Initialize(Crypto::GetInstance()->MD5_encrypt("eDoctor"));
	g_pEDD=eDoctorDeamon::CreateeDoctorDeamon();
	MainPath=WSTR_2_STR(GetResDir());
#ifdef _DEBUG
	AllocConsole();
	g_hConsole = GetStdHandle( STD_OUTPUT_HANDLE );
	std::wstring sDbgTitle=L"<������Ϣ����>\n";
	WriteConsole( g_hConsole, sDbgTitle.c_str(), sDbgTitle.size(),NULL, NULL );
	SetConsoleTitle(L"Debug Window");
	std::ofstream fOut("DbgLog.log");
	fOut.close();
#endif
	
    return true; 
}

//����bolt
void eDoctor::UninitXLUE()
{
    //�˳�����
    XLUE_Uninit(NULL);
	//XLUE_UninitLuaHost(NULL);
	XLGP_UnInitGraphicPlus();
    XL_UnInitGraphicLib();
    XLUE_UninitHandleMap(NULL);
	

	//���ٸ����Զ����ȫ�ֱ���
	
	g_pEDD=NULL;
	g_IM.Uninitialize();
	FreeConsole();
}
//������Դ�ļ�
bool eDoctor::LoadMainXAR()
{
    long result = 0;
    //����XAR������·��
    result = XLUE_AddXARSearchPath(GetResDir());
    //������XAR,��ʱ��ִ�и�XAR�������ű�onload.lua
    result = XLUE_LoadXAR(ED_XAR_PATH);
    if(result != 0)
    {
        return false;
    }
    return true;
}
//��ʾ����
bool eDoctor::CreateInTray(HWND hWnd)
{
	::ZeroMemory(&m_trayIcon, sizeof(m_trayIcon));
	m_trayIcon.cbSize=NOTIFYICONDATA_V1_SIZE;
	m_trayIcon.hWnd=hWnd;
	m_trayIcon.uID=1;
	m_trayIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	m_trayIcon.hIcon=LoadIcon(MainInstance, MAKEINTRESOURCE(IDI_ICON1));

	//WCHAR *wMsg=L"e����";
	//StringCchCopy(m_trayIcon.szTip, ARRAYSIZE(m_trayIcon.szTip), wMsg);

	Shell_NotifyIconW(NIM_ADD, &m_trayIcon);

	return true;
}
//��ʾ������ʾ��Ϣ
void eDoctor::ShowTrayTips(const std::string &sTips)
{
	const WCHAR *wMsg=STR_2_WSTR(sTips).c_str();
	wMsg=L"123";
	m_trayIcon.uTimeout=5000;
	m_trayIcon.dwInfoFlags = NIIF_INFO;
	m_trayIcon.uCallbackMessage = WM_LBUTTONDBLCLK;
	//StringCchCopy(m_trayIcon.szTip, ARRAYSIZE(m_trayIcon.szTip), wMsg);
	wcscpy_s(m_trayIcon.szInfoTitle,_T("qweqweqwe"));
	wcscpy_s(m_trayIcon.szInfo,_T("asdasdasd"));
	BOOL re=Shell_NotifyIconW(NIM_MODIFY, &m_trayIcon);
	re=GetLastError();
}