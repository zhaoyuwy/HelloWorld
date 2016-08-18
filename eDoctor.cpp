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
//这个是后台模块的全局对象指针
eDoctorDeamon *g_pEDD=NULL;
//主窗口的hwnd
HWND MainHwnd = NULL;
//应用程序实例的句柄
HINSTANCE MainInstance;
//消息钩子句柄
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

//主程序创建函数，只能返回一个实例
eDoctor* eDoctor::CreateEDoctor()
{
	if(m_eDoctor==NULL)
	{
		m_eDoctor=new eDoctor();
	}
	return m_eDoctor;
}
//主程序销毁
void eDoctor::ReleaseEDoctor()
{
	if(m_eDoctor!=NULL)
	{
		delete m_eDoctor;
		m_eDoctor=NULL;
	}
}
//设置应用程序基本信息
void eDoctor::SetInstanceInfo(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPTSTR lpCmdLine,int nCmdShow)
{
	m_hInstance=hInstance;
	m_hPrevInstance=hPrevInstance;
	m_lpCmdLine=lpCmdLine;
	m_nCmdShow=nCmdShow;
}

//检查更新
int eDoctor::CheckUpdate()
{
	//直接先比较
	//int cmp=CheckUpdate::CompareVersions();
	std::string sPath=VersionConfig::GetInstance()->GetNewVersionPath();

	EDLog("开始检查更新");
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
//移动updater并删除临时目录
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
//判定进程是否启动。只允许用户创建一个进程
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
		//printf(" 进程名称：%s \n", pe32.szExeFile);  
		if(L"eDoctor.exe"==std::wstring(pe32.szExeFile))  
		{  
			//printf("进程运行中");  
			i+=1;  
		}  
		bMore = ::Process32Next(hProcessSnap, &pe32);  
	}  
	if(i>1){           //大于1，排除自身  
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
	//MessageBoxW(NULL,t.c_str(),L"提示",MB_OK);
	if (isRun > 0 && (t != std::wstring(L"IsStartUpAgain")))
	{
		MessageBoxW(NULL,L"e会诊已启动",L"提示",MB_OK);
		isRun = FALSE;
		return 1;
	}
	if(!InitXLUE())
    {
        MessageBoxW(NULL,L"初始化XLUE 失败!",L"错误",MB_OK);
        return 1;
    }
	EDLog("初始化XLUE---ok");

	MoveUpdater();
	int nCheckUpdate=CheckUpdate();
	if(nCheckUpdate==-2)
	{
		MessageBox(NULL,L"当前版本过低，请重新下载最新版客户端！",L"提示",0);
		UninitXLUE();
		::OleUninitialize();
		return 0;
	}
	else if(nCheckUpdate==-1)
	{
		EDLog("当前版本过低，需要更新");
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
        MessageBoxW(NULL,L"Load XAR失败!",L"错误",MB_OK);
        return 1;
    }
	EDLog("初始化XAR---ok");

	MSG msg;

	// 主消息循环-----------------------------------------
	//这里使用PeekMessage可以提高处理EDoctorMessageList的效率
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
			//有消息就去处理它
			if(g_EDMessageList.Size()>0)
			{
				EDoctorMessage avMsg;
				g_EDMessageList.GetEDTMessage(avMsg,1);
				if(avMsg.msg==EM_QUIT)//真正的退出在这里
				{
					EDLog("收到退出消息");;
					break;
				}
				g_pEDD->HandleAVMessage(avMsg);
			}
		}
	}
	UninitXLUE();
	EDLog("反初始化XLUE");;

	delete g_MyVedioWindow;
	delete g_OtherVedioWindow;
	::OleUninitialize();
	return (int) msg.wParam;
}
//lua出错后的回调函数
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

            MessageBoxW(0,str.c_str(),L"为了帮助我们改进质量,请反馈此脚本错误",MB_ICONERROR | MB_OK);

        }
        else
        {
			MessageBoxW(0,wcszLuaErrorString ? wcszLuaErrorString : L"" ,L"为了帮助我们改进质量,请反馈此脚本错误",MB_ICONERROR | MB_OK);
        }
        s_bEnter = false;
    }
    return 0;
}

//注册各种lua环境的对象
bool eDoctor::InitLuaObjects()
{
	XL_LRT_ENV_HANDLE hEnv = XLLRT_GetEnv(NULL);
	//注册各种全局对象
	if(!LuaMainApp::RegisterGlobalObj(hEnv))
	{
		return false;
	}
	XLLRT_ReleaseEnv(hEnv);
	return true;
}
//初始化bolt
bool eDoctor::InitXLUE()
{
    //初始化图形库
    XLGraphicParam param;
    XL_PrepareGraphicParam(&param);
	long result = XL_InitGraphicLib(&param);
    //初始化XLUE,这函数是一个复合初始化函数
    //完成了初始化Lua环境,标准对象,XLUELoader的工作
    result = XLUE_InitLoader(NULL);
	XLGraphicPlusParam plusParam;
	XLGP_PrepareGraphicPlusParam(&plusParam);
	XLGP_InitGraphicPlus(&plusParam);

	//设置一个简单的脚本出错提示
    XLLRT_ErrorHandle(LuaErrorHandle);

	//初始化各种全局对象
	if(!InitLuaObjects())
	{
		return false;
	}

	//初始化自己定义的各种全局变量
	g_IM.Initialize(Crypto::GetInstance()->MD5_encrypt("eDoctor"));
	g_pEDD=eDoctorDeamon::CreateeDoctorDeamon();
	MainPath=WSTR_2_STR(GetResDir());
#ifdef _DEBUG
	AllocConsole();
	g_hConsole = GetStdHandle( STD_OUTPUT_HANDLE );
	std::wstring sDbgTitle=L"<调试信息窗口>\n";
	WriteConsole( g_hConsole, sDbgTitle.c_str(), sDbgTitle.size(),NULL, NULL );
	SetConsoleTitle(L"Debug Window");
	std::ofstream fOut("DbgLog.log");
	fOut.close();
#endif
	
    return true; 
}

//销毁bolt
void eDoctor::UninitXLUE()
{
    //退出流程
    XLUE_Uninit(NULL);
	//XLUE_UninitLuaHost(NULL);
	XLGP_UnInitGraphicPlus();
    XL_UnInitGraphicLib();
    XLUE_UninitHandleMap(NULL);
	

	//销毁各种自定义的全局变量
	
	g_pEDD=NULL;
	g_IM.Uninitialize();
	FreeConsole();
}
//加载资源文件
bool eDoctor::LoadMainXAR()
{
    long result = 0;
    //设置XAR的搜索路径
    result = XLUE_AddXARSearchPath(GetResDir());
    //加载主XAR,此时会执行该XAR的启动脚本onload.lua
    result = XLUE_LoadXAR(ED_XAR_PATH);
    if(result != 0)
    {
        return false;
    }
    return true;
}
//显示托盘
bool eDoctor::CreateInTray(HWND hWnd)
{
	::ZeroMemory(&m_trayIcon, sizeof(m_trayIcon));
	m_trayIcon.cbSize=NOTIFYICONDATA_V1_SIZE;
	m_trayIcon.hWnd=hWnd;
	m_trayIcon.uID=1;
	m_trayIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	m_trayIcon.hIcon=LoadIcon(MainInstance, MAKEINTRESOURCE(IDI_ICON1));

	//WCHAR *wMsg=L"e会诊";
	//StringCchCopy(m_trayIcon.szTip, ARRAYSIZE(m_trayIcon.szTip), wMsg);

	Shell_NotifyIconW(NIM_ADD, &m_trayIcon);

	return true;
}
//显示托盘提示信息
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