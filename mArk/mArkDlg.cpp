
// mArkDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "mArk.h"
#include "mArkDlg.h"
#include "afxdialogex.h"
#include "Process.h"
#include "DriverDlg.h"
#include "FileDlg.h"
#include "KernelDlg.h"
#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CmArkDlg 对话框

HANDLE CmArkDlg::g_hSys = 0;

CmArkDlg::CmArkDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(AHOME, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	LoadDriver();
}

void CmArkDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TAB1, m_tabCtl);
}

BEGIN_MESSAGE_MAP(CmArkDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB1, &CmArkDlg::OnTcnSelchangeTab1)
	ON_WM_CLOSE()
END_MESSAGE_MAP()


// CmArkDlg 消息处理程序

BOOL CmArkDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标
	
	ShowWindow(SW_MINIMIZE);
	SetWindowText(L"MyArk");
	addTabWnd(L"进程", new Process, IDD_ProcessDlg);
	addTabWnd(L"驱动", new DriverDlg, IDD_Driver);
	addTabWnd(L"系统文件", new FileDlg, IDD_FileDlg);
	addTabWnd(L"内核", new KernelDlg, IDD_KernelDlg);
	// TODO: 在此添加额外的初始化代码
	m_tabCtl.SetCurSel(0);
	m_tabSubWnd[m_tabCtl.GetCurSel()]->ShowWindow(SW_SHOW);
	
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CmArkDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CmArkDlg::OnQueryDragIcon()
{
	
	return static_cast<HCURSOR>(m_hIcon);
}

void CmArkDlg::addTabWnd(const CString & title, CDialogEx * pSubWnd, UINT uId)
{
	m_tabCtl.InsertItem(m_tabCtl.GetItemCount(), title);
	pSubWnd->Create(uId, &m_tabCtl);
	CRect rect;
	m_tabCtl.GetClientRect(rect);
	rect.DeflateRect(1, 23, 1, 1);
	pSubWnd->MoveWindow(rect);
	m_tabSubWnd.push_back(pSubWnd);
}



void CmArkDlg::OnTcnSelchangeTab1(NMHDR *pNMHDR, LRESULT *pResult)
{
	for (auto&i : m_tabSubWnd)
	{
		i->ShowWindow(SW_HIDE);
	}
	m_tabSubWnd[m_tabCtl.GetCurSel()]->ShowWindow(SW_SHOW);
	*pResult = 0;
}


void CmArkDlg::OnClose()
{
	//UnLoadDriver();
	CloseHandle(g_hSys);

	CDialogEx::OnClose();
}


void CmArkDlg::LoadDriver()
{
	// 1. 打开服务管理器
	//hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	//// 2. 创建服务
	//m_hService = CreateService(hSCManager, L"abcde", L"MyARKDriver1",
	//	SERVICE_ALL_ACCESS,
	//	SERVICE_KERNEL_DRIVER,
	//	SERVICE_DEMAND_START,
	//	SERVICE_ERROR_IGNORE,
	//	 L"C:\\Users\\15pb-win7\\Desktop\\mArkDri.sys",    //驱动文件 sys
	//	NULL, NULL, NULL, NULL, NULL);
	//// 2.1 判断服务是否存在
	//if (GetLastError() == ERROR_SERVICE_EXISTS)
	//{
	//	// 如果服务存在，只要打开
	//	m_hService = OpenService(hSCManager, L"abcde", SERVICE_ALL_ACCESS);
	//}
	//// 2.2 创建服务是否成功
	//if (!m_hService)
	//{
	//	CString cs;
	//	cs.Format(_T("%d"), GetLastError());
	//	MessageBox(L"CreateService Failed", cs);
	//	CloseServiceHandle(hSCManager);
	//	return;
	//}

	//// 3. 启动服务
	//SERVICE_STATUS status;
	//// 查询服务状态
	//QueryServiceStatus(m_hService, &status);
	//// 服务处于暂停状态
	//if (status.dwCurrentState == SERVICE_STOPPED)
	//{
	//	// 启动服务
	//	StartService(m_hService, NULL, NULL);
	//	Sleep(1000);
	//	// 再检查服务状态
	//	QueryServiceStatus(m_hService, &status);
	//	// 服务是否处于运行状态
	//	if (status.dwCurrentState != SERVICE_RUNNING)
	//	{
	//		//m_State = L"StartService Failed";
	//		CloseServiceHandle(hSCManager);
	//		CloseServiceHandle(m_hService);
	//	}
	//	else
	//	{
	//		//m_State = L"启动成功";
	//	}
	//}
	g_hSys = CreateFile(L"\\??\\device_mARK",
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		0);
	if (g_hSys == INVALID_HANDLE_VALUE)
	{
		MessageBox(L"驱动打开失败", L"错误", 0);
		//exit(0);
	}
	else
	{
		MessageBox(L"驱动打开成功", L"提示", 0);
	}
}


void CmArkDlg::UnLoadDriver()
{
	// 1.先停止服务
	SERVICE_STATUS status;
	// 检查服务状态
	BOOL bRet = QueryServiceStatus(m_hService, &status);
	// 如果不处于暂停状态
	if (status.dwCurrentState != SERVICE_STOPPED)
	{
		//停止服务
		ControlService(m_hService, SERVICE_CONTROL_STOP, &status);
		// 直到服务停止
		while (QueryServiceStatus(m_hService, &status) == TRUE)
		{
			Sleep(status.dwWaitHint);
			if (status.dwCurrentState == SERVICE_STOPPED)
			{
				//m_State = L"已暂停";
				//UpdateData(FALSE);
			}
			else
			{
				//m_State = L"无法暂停";
				//UpdateData(FALSE);
			}
			break;
		}
	}



	// Delete the service.
	// 删除服务（卸载驱动）
	if (!DeleteService(m_hService))
	{
		CString cs;
		cs.Format(_T("%d"), GetLastError());
		MessageBox(L"DeleteService Failed", cs);
		//m_State = L"卸载失败";
	}
	else
	{
		//m_State = L"卸载成功";
	}

	// 关闭服务管理器句柄
	CloseServiceHandle(m_hService);
	CloseServiceHandle(hSCManager);
}







