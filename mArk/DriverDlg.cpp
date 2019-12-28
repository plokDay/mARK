// DriverDlg.cpp: 实现文件
//

#include "pch.h"
#include "mArk.h"
#include "DriverDlg.h"
#include "afxdialogex.h"
#include "mArkDlg.h"

// DriverDlg 对话框
DWORD dnum = 0;
IMPLEMENT_DYNAMIC(DriverDlg, CDialogEx)

DriverDlg::DriverDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_Driver, pParent)
{

}

DriverDlg::~DriverDlg()
{
}

void DriverDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST1, m_list);
}


BEGIN_MESSAGE_MAP(DriverDlg, CDialogEx)
	ON_NOTIFY(NM_RCLICK, IDC_LIST1, &DriverDlg::OnRclickList1)
	ON_COMMAND(ID_DRIVER_32771, &DriverDlg::OnDriver32771)
	ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()


// DriverDlg 消息处理程序


BOOL DriverDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	m_menu.LoadMenu(IDR_MENU1);

	m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_list.InsertColumn(0, L"驱动名", 0, 100);
	m_list.InsertColumn(1, L"基地址", 0, 80);
	m_list.InsertColumn(2, L"大小", 0, 80);
	m_list.InsertColumn(3, L"驱动路径", 0, 500);

	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}


void DriverDlg::EnumDriver()
{
	BOOL bOk = FALSE;
	DWORD size = 0;
	//m_DriverEntryVector.clear();
	std::vector<DRIVER_ENTRY_INFO> newProcList;
	m_DriverEntryVector.swap(newProcList);
	DRIVER_ENTRY_INFO dri = { 0 };
	CString buff = L"";

	for (int i = 0; i < dnum; ++i) {
		CString buffer;
		DeviceIoControl(CmArkDlg::g_hSys,
			DF_ENUM_DRIVER,
			0,
			0,
			&dri,
			sizeof(DRIVER_ENTRY_INFO),
			&size,
			NULL);
		m_DriverEntryVector.push_back(dri);
		//	buffer.Format(L"size：%08X\tImageName：%s\n", dri.Size,dri.wzDriverName);
		//	buff = buff + buffer;
	}
	//	MessageBox(buff, L"Get Driver", 0);

	//填充m_list
	CString buffer;
	m_list.DeleteAllItems();
	for (int index = 0; index < m_DriverEntryVector.size(); ++index)
	{
		buffer.Format(L"%s", m_DriverEntryVector[index].wzDriverName);
		m_list.InsertItem(index, buffer);

		buffer.Format(L"%08X", m_DriverEntryVector[index].BaseAddress);
		m_list.SetItemText(index, 1, buffer);

		buffer.Format(L"%X", m_DriverEntryVector[index].Size);
		m_list.SetItemText(index, 2, buffer);

		buffer.Format(L"%s", m_DriverEntryVector[index].wzDriverPath);
		m_list.SetItemText(index, 3, buffer);

	}
}


UINT32 DriverDlg::GetDriverNum()
{
	BOOL   bOk = FALSE;
	DWORD  dwReturnLength = 0;

	bOk = DeviceIoControl(CmArkDlg::g_hSys,
		DF_GET_DRIVERNUM,
		NULL,		// InputBuffer
		0,
		&dnum,
		sizeof(DWORD),
		&dwReturnLength,
		NULL);
	//if (bOk != 0)
	{
		/*CString buff;
		buff.Format(L"status:%d\tnum:%d", bOk, dnum);
		MessageBox(buff, L"DriverNum", 0);*/
	}

	return (UINT32)dnum;
}

//右键
void DriverDlg::OnRclickList1(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	CMenu* pSubMenu = m_menu.GetSubMenu(0);
	CPoint pos;
	GetCursorPos(&pos);
	pSubMenu->TrackPopupMenu(0, pos.x, pos.y, this);
	*pResult = 0;
}

//隐藏驱动
//将驱动名称传递给0环
void DriverDlg::OnDriver32771()
{
	int index = (int)m_list.GetFirstSelectedItemPosition();
	if (index == 0) {
		return;
	}
	index -= 1;
	CString csname = m_list.GetItemText(index,0);// 下标为0是驱动名称
	WCHAR driverName[100] = {0};

	wcscpy_s(driverName, 100, csname);
	BOOL bOk = FALSE;
	DWORD size = 0;
	m_DriverEntryVector.clear();
	DRIVER_ENTRY_INFO dri = { 0 };
	CString buff;
	bOk= DeviceIoControl(CmArkDlg::g_hSys,
		DF_HIDE_DRIVER,
		driverName,
		100,
		0,
		0,
		&size,
		NULL);
	buff.Format(L"%d\t%s", bOk, csname);
	//MessageBox(buff, L"Get Driver", 0);
}


void DriverDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialogEx::OnShowWindow(bShow, nStatus);
	if (bShow == TRUE)
	{
		GetDriverNum();
		EnumDriver();
	}

	// TODO: 在此处添加消息处理程序代码
}
