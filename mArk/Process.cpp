// Process.cpp: 实现文件
//

#include "pch.h"
#include "mArk.h"
#include "Process.h"
#include <strsafe.h>
#include "afxdialogex.h"
#include "mArkDlg.h"
#include "ModuleThreadDlg.h"
#pragma comment(lib, "Version.lib")      // GetFileVersionInfo 需要链接此库
DWORD num = 0;
std::vector<MODULE_ENTRY> Process::m_ModuleVector;
std::vector<THREAD_ENTRY> Process::m_ThreadVector;
BOOL Process::isModule = FALSE;
// Process 对话框

IMPLEMENT_DYNAMIC(Process, CDialogEx)

Process::Process(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_ProcessDlg, pParent)
{

}

Process::~Process()
{
}

void Process::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST1, m_list);
}


BEGIN_MESSAGE_MAP(Process, CDialogEx)
	ON_NOTIFY(NM_RCLICK, IDC_LIST1, &Process::OnNMRClickList1)
	ON_WM_SHOWWINDOW()
	ON_COMMAND(ID_Menu, &Process::OnProtectProcess)
	ON_COMMAND(ID_PROCESS_32773, &Process::OnTerminateProcess)
	ON_COMMAND(ID_PROCESS_32772, &Process::OnEnumModuel)
	ON_COMMAND(ID_PROCESS_32776, &Process::OnEnumThread)
	ON_COMMAND(ID_PROCESS_32777, &Process::OnHideProcess)
END_MESSAGE_MAP()


// Process 消息处理程序


BOOL Process::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	m_menu.LoadMenu(IDR_MENU2);

	m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_list.InsertColumn(0, L"进程名", 0, 150);
	m_list.InsertColumn(1, L"进程ID", 0, 80);
	m_list.InsertColumn(2, L"父进程ID", 0, 80);
	m_list.InsertColumn(3, L"映像路径", 0, 400);
	m_list.InsertColumn(4, L"EPROCESS", 0, 120);
	m_list.InsertColumn(5, L"应用层访问状态", 0, 150);

	
	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}

BOOL Process::GrantPriviledge(IN PWCHAR PriviledgeName, IN BOOL bEnable)
{
	TOKEN_PRIVILEGES TokenPrivileges, OldPrivileges;
	DWORD			 dwReturnLength = sizeof(OldPrivileges);
	HANDLE			 TokenHandle = NULL;
	LUID			 uID;

	// 打开权限令牌
	if (!OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, &TokenHandle))
	{
		if (GetLastError() != ERROR_NO_TOKEN)
		{
			return FALSE;
		}
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &TokenHandle))
		{
			return FALSE;
		}
	}

	if (!LookupPrivilegeValue(NULL, PriviledgeName, &uID))		// 通过权限名称查找uID
	{
		CloseHandle(TokenHandle);
		TokenHandle = NULL;
		return FALSE;
	}

	TokenPrivileges.PrivilegeCount = 1;		// 要提升的权限个数
	TokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;    // 动态数组，数组大小根据Count的数目
	TokenPrivileges.Privileges[0].Luid = uID;

	// 在这里我们进行调整权限
	if (!AdjustTokenPrivileges(TokenHandle, FALSE, &TokenPrivileges, sizeof(TOKEN_PRIVILEGES), &OldPrivileges, &dwReturnLength))
	{
		CloseHandle(TokenHandle);
		TokenHandle = NULL;
		return FALSE;
	}

	// 成功了
	CloseHandle(TokenHandle);
	TokenHandle = NULL;

	return TRUE;
}
//提升权限，看能否打开OpenProcess
BOOL Process::QueryProcessUserAccess(UINT32 ProcessId)
{

	GrantPriviledge(SE_DEBUG_NAME, TRUE);
	
	HANDLE ProcessHandle = OpenProcess(PROCESS_TERMINATE
		| PROCESS_VM_OPERATION, TRUE, ProcessId);
	
	GrantPriviledge(SE_DEBUG_NAME, FALSE);
	if (ProcessHandle)
	{
		CloseHandle(ProcessHandle);
		return TRUE;
	}

	return FALSE;
}

char* wchar2char(const wchar_t* szStr)
{
	int nLen = WideCharToMultiByte(CP_ACP, 0, szStr, -1, NULL, 0, NULL, NULL);
	if (nLen == 0)
	{
		return NULL;
	}
	char* pResult = new char[nLen];
	WideCharToMultiByte(CP_ACP, 0, szStr, -1, pResult, nLen, NULL, NULL);
	return pResult;
}
void Process::PerfectProcessInfo(PPROCESS_ENTRY_INFO ProcessEntry)
{
	if (ProcessEntry->ProcessId == 0)
	{
		// Idle
		strcpy_s(ProcessEntry->szImageName, strlen("System Idle Process")+1, "System Idle Process");
		StringCchCopyW(ProcessEntry->wzFilePath, wcslen(L"System Idle Process") + 1, L"System Idle Process");
		ProcessEntry->bUserAccess = FALSE;
		ProcessEntry->ParentProcessId = 0;
	}
	else if (ProcessEntry->ProcessId == 4)
	{
		// System
		strcpy_s(ProcessEntry->szImageName, strlen("System") + 1, "System");
		WCHAR wzFilePath[MAX_PATH] = { 0 };
		GetSystemDirectory(wzFilePath, MAX_PATH);      // 获得System32Directory
		StringCchCatW(wzFilePath, MAX_PATH, L"\\ntoskrnl.exe");

		StringCchCopyW(ProcessEntry->wzFilePath, wcslen(wzFilePath) + 1, wzFilePath);
		ProcessEntry->bUserAccess = FALSE;
		ProcessEntry->ParentProcessId = 0xffffffff;
	
	}
	else
	{
		// Others
		WCHAR *wzImageName = NULL;
		wzImageName = wcsrchr(ProcessEntry->wzFilePath, '\\');
		wzImageName++;  // 过 '\\'
		CHAR* szImageName = wchar2char(wzImageName);
		strcpy_s(ProcessEntry->szImageName, strlen(szImageName) + 1, szImageName);
		if (QueryProcessUserAccess(ProcessEntry->ProcessId) == TRUE)
		{
			ProcessEntry->bUserAccess = TRUE;
		}
		else
		{
			ProcessEntry->bUserAccess = FALSE;
		}

	}
}

UINT32 Process::GetProcessNum()
{
	BOOL   bOk = FALSE;
	DWORD  dwReturnLength = 0;

	DWORD ProcessNum = 0;
	bOk = DeviceIoControl(CmArkDlg::g_hSys,
		DF_GET_PROCESSNUM,
		NULL,		// InputBuffer
		0,
		&ProcessNum,
		sizeof(DWORD),
		&dwReturnLength,
		NULL);
	
	num = ProcessNum;
	return (UINT32)ProcessNum;
}
wchar_t * char2wchar(const char* cchar)
{
	wchar_t *m_wchar;
	int len = MultiByteToWideChar(CP_ACP, 0, cchar, strlen(cchar), NULL, 0);
	m_wchar = new wchar_t[len + 1];
	MultiByteToWideChar(CP_ACP, 0, cchar, strlen(cchar), m_wchar, len);
	m_wchar[len] = '\0';
	return m_wchar;

}
BOOL Process::EnumProcessInfo()
{
	BOOL bOk = FALSE;
	DWORD size = 0;
	//m_ProcessEntryVector.clear();
	//清空vector
	std::vector<PROCESS_ENTRY_INFO> newProcList;
	m_ProcessEntryVector.swap(newProcList);
	PROCESS_ENTRY_INFO fpi = { 0 };
	CString buff=L"";

	for(int i=0;i<num;++i){
		CString buffer;
		
		DeviceIoControl(CmArkDlg::g_hSys,
			DF_ENUM_PROCESS,
			0,
			0,
			&fpi,
			sizeof(PROCESS_ENTRY_INFO),
			&size,
			NULL);
		PerfectProcessInfo(&fpi);
		m_ProcessEntryVector.push_back(fpi);
		//buffer.Format(L"PID：%d\tImageName：%s\n", fpi.ProcessId, char2wchar(fpi.szImageName));
		//buff = buff + buffer;
	} 
	//MessageBox(buff, L"Get Process", 0);

	
	if (m_ProcessEntryVector.empty())
	{
		return FALSE;
	}
	CString buffer;
	//填充m_list
	m_list.DeleteAllItems();

	for (int index = 0; index < m_ProcessEntryVector.size(); ++index)
	{
	
		buffer.Format(L"%s", char2wchar(m_ProcessEntryVector[index].szImageName));
		m_list.InsertItem(index, buffer);

		buffer.Format(L"%d", m_ProcessEntryVector[index].ProcessId);
		m_list.SetItemText(index, 1, buffer);

		buffer.Format(L"%d", m_ProcessEntryVector[index].ParentProcessId);
		m_list.SetItemText(index, 2, buffer);

		buffer.Format(L"%s", m_ProcessEntryVector[index].wzFilePath);
		m_list.SetItemText(index, 3, buffer);

		buffer.Format(L"0x%08X", m_ProcessEntryVector[index].EProcess);
		m_list.SetItemText(index, 4, buffer);

		m_ProcessEntryVector[index].bUserAccess == 1 ?
			m_list.SetItemText(index, 5, L"允许") : m_list.SetItemText(index, 5, L"拒绝");

		
	}
	return bOk;
}

//右键
void Process::OnNMRClickList1(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	CMenu* pSubMenu = m_menu.GetSubMenu(0);
	CPoint pos;
	GetCursorPos(&pos);
	pSubMenu->TrackPopupMenu(0, pos.x, pos.y, this);
	*pResult = 0;
}


void Process::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialogEx::OnShowWindow(bShow, nStatus);
	if (bShow == TRUE)
	{
		GetProcessNum();
		EnumProcessInfo();
	}
	
}

//进程保护
void Process::OnProtectProcess()
{
	int index = (int)m_list.GetFirstSelectedItemPosition();
	if (index == 0) {
		return;
	}
	index -= 1;
	CString strPid = m_list.GetItemText(index, 1);// 下标为1是PID
	ULONG uPid = _ttoi(strPid);
	CString buff;
	
	BOOL bOk = FALSE;
	DWORD size = 0;
	bOk = DeviceIoControl(CmArkDlg::g_hSys,
		DF_PROTECT_PRPCESS,
		&uPid,
		sizeof(ULONG),
		0,
		0,
		&size,
		NULL);
	buff.Format(L"%d\t%d", bOk, uPid);
	MessageBox(buff, L"Get Driver", 0);
}

//结束进程
void Process::OnTerminateProcess()
{
	int index = (int)m_list.GetFirstSelectedItemPosition();
	if (index == 0) {
		return;
	}
	index -= 1;
	CString strPid = m_list.GetItemText(index, 1);// 下标为1是PID
	ULONG uPid = _ttoi(strPid);
	CString buff;

	BOOL bOk = FALSE;
	DWORD size = 0;
	bOk = DeviceIoControl(CmArkDlg::g_hSys,
		DF_TERMINATE_PRPCESS,
		&uPid,
		sizeof(ULONG),
		0,
		0,
		&size,
		NULL);
	buff.Format(L"%d\t%d", bOk, uPid);
	MessageBox(buff, L"Terminate Process", 0);
}


void Process::OnEnumModuel()
{
	int index = (int)m_list.GetFirstSelectedItemPosition();
	if (index == 0) {
		return;
	}
	index -= 1;
	CString strPid = m_list.GetItemText(index, 1);// 下标为1是PID
	ULONG uPid = _ttoi(strPid);
	CString buff;
	DWORD moduNum = 0;
	MODULE_ENTRY temp;

	BOOL bOk = FALSE;
	DWORD size = 0;
	bOk = DeviceIoControl(CmArkDlg::g_hSys,
		DF_ENUM_MODULEFIRST,
		&uPid,
		sizeof(ULONG),
		&moduNum,
		sizeof(DWORD),
		&size,
		NULL);
	
	for (int i = 0; i < moduNum; ++i)
	{
		bOk = DeviceIoControl(CmArkDlg::g_hSys,
			DF_ENUM_MODULENEXT,
			0,
			0,
			&temp,
			sizeof(MODULE_ENTRY),
			&size,
			NULL);
		m_ModuleVector.push_back(temp);
	}

	isModule = TRUE;
	ModuleThreadDlg mdlg;
	mdlg.DoModal();
}


void Process::OnEnumThread()
{
	int index = (int)m_list.GetFirstSelectedItemPosition();
	if (index == 0) {
		return;
	}
	index -= 1;
	CString strErpcess = m_list.GetItemText(index, 4);// 下标为4是ERPOCESS
	ULONG eprocess =0;
	swscanf_s(strErpcess.GetBuffer(0), L"%x", &eprocess);
	CString buff;
	
	THREAD_ENTRY temp;
	DWORD modNum = 0;
	BOOL bOk = FALSE;
	DWORD size = 0;
	bOk = DeviceIoControl(CmArkDlg::g_hSys,
		DF_ENUM_THREADFIRST,
		&eprocess,
		sizeof(ULONG),
		&modNum,
		sizeof(DWORD),
		&size,
		NULL);
	for (int i = 0; i < modNum; ++i)
	{
		bOk = DeviceIoControl(CmArkDlg::g_hSys,
			DF_ENUM_THREADNEXT,
			0,
			0,
			&temp,
			sizeof(THREAD_ENTRY),
			&size,
			NULL);
		m_ThreadVector.push_back(temp);
	}
	
	isModule = FALSE;
	ModuleThreadDlg mdlg;
	mdlg.DoModal();
}


void Process::OnHideProcess()
{
	int index = (int)m_list.GetFirstSelectedItemPosition();
	if (index == 0) {
		return;
	}
	index -= 1;
	CString strPid = m_list.GetItemText(index, 1);// 下标为1是PID
	ULONG uPid = _ttoi(strPid);
	CString buff;

	BOOL bOk = FALSE;
	DWORD size = 0;
	bOk = DeviceIoControl(CmArkDlg::g_hSys,
		DF_HIDE_PROCESS,
		&uPid,
		sizeof(ULONG),
		0,
		0,
		&size,
		NULL);
	buff.Format(L"%d\t%d", bOk, uPid);
	MessageBox(buff, L"Hide process", 0);
}
