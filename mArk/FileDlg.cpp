// FileDlg.cpp: 实现文件
//

#include "pch.h"
#include "mArk.h"
#include "FileDlg.h"
#include "afxdialogex.h"
#include "mArkDlg.h"


// FileDlg 对话框

IMPLEMENT_DYNAMIC(FileDlg, CDialogEx)

FileDlg::FileDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_FileDlg, pParent)
{

}

FileDlg::~FileDlg()
{
}

void FileDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST1, m_list);
}


BEGIN_MESSAGE_MAP(FileDlg, CDialogEx)
	ON_WM_SHOWWINDOW()
	ON_NOTIFY(NM_RDBLCLK, IDC_LIST1, &FileDlg::OnNMRDblclkList1)
	ON_COMMAND(ID_32774, &FileDlg::On32774)
END_MESSAGE_MAP()


// FileDlg 消息处理程序


BOOL FileDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	m_menu.LoadMenu(IDR_MENU3);

	m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_list.InsertColumn(0, L"文件名", 0, 200);
	m_list.InsertColumn(1, L"大小(KB)", 0, 100);
	m_list.InsertColumn(2, L"创建时间", 0, 200);
	m_list.InsertColumn(3, L"修改时间", 0, 200);

	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}


void FileDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialogEx::OnShowWindow(bShow, nStatus);
	FindFile();
}


void FileDlg::FindFile()
{
	DWORD size = 0;
	std::vector<FIND_FILE_INFO> newProcList;
	m_FileEntryVector.swap(newProcList);
	const wchar_t* pStr = L"\\??\\C:\\1.txt";
	int inputSize = wcslen(L"\\??\\C:\\1.txt") * 2 + 2;
	FIND_FILE_INFO ffi = { 0 };
	pStr = L"\\??\\C:\\Windows";
	inputSize = wcslen(pStr) * 2 + 2;
	// 遍历文件:
	DeviceIoControl(CmArkDlg::g_hSys, 
		DF_GET_FILE_FIRST,
		(LPVOID)pStr,/*第一次查找,传入目录名*/
		inputSize,
		&ffi, /*保存输出信息*/
		sizeof(ffi),
		&size,
		NULL);
	if (size != sizeof(ffi))
	{
		CString buffer;
		buffer.Format(L"Size:%d", size);
		MessageBox(buffer, L"获取文件错误", 0);
	}
	else
	{
		do
		{
			m_FileEntryVector.push_back(ffi);
			DeviceIoControl(CmArkDlg::g_hSys,
				DF_GET_FILE_NEXT,
				(LPVOID)&ffi,/*后续查找,传入查找句柄*/
				sizeof(FIND_FILE_INFO),
				&ffi,/*保存输出的文件信息*/
				sizeof(FIND_FILE_INFO),
				&size,
				NULL);
			if (size != sizeof(ffi))
				break;
		} while (true);
	}

	//填充m_list
	m_list.DeleteAllItems();
	CString buffer;
	SYSTEMTIME stLocal;
	ZeroMemory(&stLocal, sizeof(SYSTEMTIME));

	for (int index = 0; index < m_FileEntryVector.size(); ++index)
	{
		buffer.Format(L"%s", m_FileEntryVector[index].cFileName);
		m_list.InsertItem(index, buffer);

		DWORD totalsize = (m_FileEntryVector[index].nFileSizeHigh << 32 | m_FileEntryVector[index].nFileSizeLow) / 1024;
		buffer.Format(L"%d", totalsize);
		m_list.SetItemText(index, 1, buffer);

		//SYSTEMTIME stLocal;
		FILETIME createTiem = { 0 }; 
		createTiem.dwLowDateTime = m_FileEntryVector[index].createTimeLow;
		createTiem.dwHighDateTime = m_FileEntryVector[index].createTimeHigh;
		const FILETIME ccreateTiem = createTiem;
		FileTimeToSystemTime(&ccreateTiem, &stLocal);
		buffer.Format(L"%04d-%02d-%02d %02d:%02d:%02d", stLocal.wYear, stLocal.wMonth, 
			stLocal.wDay, stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
		m_list.SetItemText(index, 2, buffer);

		FILETIME writeTiem = { 0 };
		writeTiem.dwLowDateTime = m_FileEntryVector[index].writeTimeLow;
		writeTiem.dwHighDateTime = m_FileEntryVector[index].writeTimeHigh;
		const FILETIME cwriteTiem = writeTiem;
		FileTimeToSystemTime(&cwriteTiem, &stLocal);
		buffer.Format(L"%04d-%02d-%02d %02d:%02d:%02d", stLocal.wYear, stLocal.wMonth, stLocal.wDay, stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
		m_list.SetItemText(index, 3, buffer);

	
	}
}

//右键
void FileDlg::OnNMRDblclkList1(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	CMenu* pSubMenu = m_menu.GetSubMenu(0);
	CPoint pos;
	GetCursorPos(&pos);
	pSubMenu->TrackPopupMenu(0, pos.x, pos.y, this);
	*pResult = 0;
}

//删除文件
void FileDlg::On32774()
{
	int index = (int)m_list.GetFirstSelectedItemPosition();
	if (index == 0) {
		return;
	}
	index -= 1;
	CString csname = m_list.GetItemText(index, 0);// 下标为0是文件名称
	WCHAR fName[100] = L"\\??\\C:\\Windows\\";
	wcscat_s(fName,  csname);
	BOOL bOk = FALSE;
	DWORD size = 0;
	DRIVER_ENTRY_INFO dri = { 0 };
	CString buff;
	bOk = DeviceIoControl(CmArkDlg::g_hSys,
		DF_DELETE_FILE,
		fName,
		100,
		0,
		0,
		&size,
		NULL);
	buff.Format(L"%d\t%s", bOk, csname);
	MessageBox(buff, L"Delete file", 0);
}
