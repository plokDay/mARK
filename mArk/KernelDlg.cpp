// KernelDlg.cpp: 实现文件
//

#include "pch.h"
#include "mArk.h"
#include "KernelDlg.h"
#include "afxdialogex.h"
#include "mArkDlg.h"

// KernelDlg 对话框
DWORD ssdtNum = 0;

IMPLEMENT_DYNAMIC(KernelDlg, CDialogEx)

KernelDlg::KernelDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_KernelDlg, pParent)
{

}

KernelDlg::~KernelDlg()
{
}

void KernelDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST1, m_list);
	DDX_Control(pDX, IDC_RADIO1, m_rdIDT);
}


BEGIN_MESSAGE_MAP(KernelDlg, CDialogEx)
	ON_WM_SHOWWINDOW()
	ON_BN_CLICKED(IDC_RADIO2, &KernelDlg::OnBnClickedRadio2)
	ON_BN_CLICKED(IDC_RADIO1, &KernelDlg::OnBnClickedRadio1)
	ON_BN_CLICKED(IDC_RADIO3, &KernelDlg::OnBnClickedRadio3)
	ON_BN_CLICKED(IDC_BUTTON1, &KernelDlg::OnBnClickedButton1)
END_MESSAGE_MAP()


// KernelDlg 消息处理程序


BOOL KernelDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	m_rdIDT.SetCheck(TRUE);
	m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);


	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}


void KernelDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialogEx::OnShowWindow(bShow, nStatus);

	OnBnClickedRadio1();


}


void KernelDlg::GetIDT()
{
	BOOL bOk = FALSE;
	DWORD size = 0;
	//m_DriverEntryVector.clear();
	std::vector<IDT_ENTRY> newProcList;
	m_IDTEntryVector.swap(newProcList);
	IDT_ENTRY idt = { 0 };
	CString buff = L"";

	for (int i = 0; i < 0x100; ++i) {
		CString buffer;
		DeviceIoControl(CmArkDlg::g_hSys,
			DF_GET_IDT,
			0,
			0,
			&idt,
			sizeof(IDT_ENTRY),
			&size,
			NULL);
		m_IDTEntryVector.push_back(idt);
		//	buffer.Format(L"size：%08X\tImageName：%s\n", dri.Size,dri.wzDriverName);
		//	buff = buff + buffer;
	}
	//	MessageBox(buff, L"Get Driver", 0);

	//填充m_list
	CString buffer;
	m_list.DeleteAllItems();
	for (int index = 0; index < m_IDTEntryVector.size(); ++index)
	{
		buffer.Format(L"%d", index);
		m_list.InsertItem(index, buffer);

		buffer.Format(L"0x%08X", MAKELONG(m_IDTEntryVector[index].uOffsetLow, m_IDTEntryVector[index].uOffsetHigh));
		m_list.SetItemText(index, 1, buffer);

		buffer.Format(L"%d", m_IDTEntryVector[index].uSeclector);
		m_list.SetItemText(index, 2, buffer);

		buffer.Format(L"%d", m_IDTEntryVector[index].GetType);
		m_list.SetItemText(index, 3, buffer);

		buffer.Format(L"%d", m_IDTEntryVector[index].DPL);
		m_list.SetItemText(index, 4, buffer);
	}
}

//点击遍历GDT
void KernelDlg::OnBnClickedRadio2()
{
	m_list.DeleteAllItems();
	for (int i = 0; i < 5; ++i)
	{
		m_list.DeleteColumn(0);
	}
	m_list.InsertColumn(0, L"段基址", 0, 120);
	m_list.InsertColumn(1, L"界限", 0, 120);
	m_list.InsertColumn(2, L"段粒度", 0, 100);
	m_list.InsertColumn(3, L"DPL", 0, 100);
	m_list.InsertColumn(4, L"类型", 0, 100);
	DWORD size = 0;
	DeviceIoControl(CmArkDlg::g_hSys,
		DF_GET_GDTFIRST,
		0,
		0,
		0,
		0,
		&size,
		NULL);
	GetGDT();
}
void KernelDlg::GetGDT()
{
	BOOL bOk = FALSE;
	DWORD size = 0;
	//m_DriverEntryVector.clear();
	std::vector<GDT_ENTRY> newProcList;
	m_GDTEntryVector.swap(newProcList);
	GDT_ENTRY gdt = { 0 };
	CString buff = L"";

	for (int i = 0; i < 128; ++i) {
		CString buffer;
		DeviceIoControl(CmArkDlg::g_hSys,
			DF_GET_GDT,
			0,
			0,
			&gdt,
			sizeof(GDT_ENTRY),
			&size,
			NULL);
		if (gdt.P != 0)
			m_GDTEntryVector.push_back(gdt);
		//	buffer.Format(L"size：%08X\tImageName：%s\n", dri.Size,dri.wzDriverName);
		//	buff = buff + buffer;
	}
	//	MessageBox(buff, L"Get Driver", 0);

	//填充m_list
	CString buffer;
	m_list.DeleteAllItems();

	for (int index = 0; index < m_GDTEntryVector.size(); ++index)
	{
		ULONG gdt_address = (m_GDTEntryVector[index].BaseHigh << 24) | 
			(m_GDTEntryVector[index].BaseMid << 16) | m_GDTEntryVector[index].BaseLow;
		buffer.Format(L"0x%08X", gdt_address);
		m_list.InsertItem(index, buffer);

		ULONG seg_limit =
			(m_GDTEntryVector[index].Limit16_19 << 16) | m_GDTEntryVector[index].Limit0_15;//拼接limit
		buffer.Format(L"0x%08X", seg_limit);
		m_list.SetItemText(index, 1, buffer);

		if (m_GDTEntryVector[index].G == 1)
		{
			m_list.SetItemText(index, 2, L"Page");

		}
		else
			m_list.SetItemText(index, 2, L"Byte");
		buffer.Format(L"%d", m_GDTEntryVector[index].DPL);
		m_list.SetItemText(index, 3, buffer);


		if (m_GDTEntryVector[index].S == 1)
		{
			if (m_GDTEntryVector[index].TYPE >= 8)
			{
				m_list.SetItemText(index, 4, L"数据段");
			}
			else
			{
				m_list.SetItemText(index, 4, L"代码段");

			}
		}
		else
		{
			m_list.SetItemText(index, 4, L"系统段");

		}

	}

}
//IDT
void KernelDlg::OnBnClickedRadio1()
{
	m_list.DeleteAllItems();
	for (int i = 0; i < 5; ++i)
	{
		m_list.DeleteColumn(0);
	}
	m_list.InsertColumn(0, L"中断号", 0, 100);
	m_list.InsertColumn(1, L"段基址", 0, 120);
	m_list.InsertColumn(2, L"段选择子", 0, 100);
	m_list.InsertColumn(3, L"类型", 0, 100);
	m_list.InsertColumn(4, L"DPL", 0, 100);
	DWORD size = 0;
	DeviceIoControl(CmArkDlg::g_hSys,
		DF_GET_IDTFIRST,
		0,
		0,
		0,
		0,
		&size,
		NULL);
	GetIDT();
}
void KernelDlg::GetSSDT()
{
	std::vector<LONG> newProcList;
	m_SSDTEntryVector.swap(newProcList);
	DWORD size = 0;

	LONG ssdtAddr = 0;
	for (int i = 0; i < ssdtNum; ++i)
	{
		DeviceIoControl(CmArkDlg::g_hSys,
			DF_ENUM_SSDT,
			0,
			0,
			&ssdtAddr,
			sizeof(LONG),
			&size,
			NULL);

		m_SSDTEntryVector.push_back(ssdtAddr);
	}
	//填充m_list
	m_list.DeleteAllItems();
	CString buff;
	for (int index = 0; index < m_SSDTEntryVector.size(); ++index)
	{
		buff.Format(L"%d", index);
		m_list.InsertItem(index, buff);

		buff.Format(L"0x%08X", m_SSDTEntryVector[index]);
		m_list.SetItemText(index, 1, buff);
	}
}
//SSDT
void KernelDlg::OnBnClickedRadio3()
{
	DWORD size = 0;

	m_list.DeleteAllItems();
	for (int i = 0; i < 5; ++i)
	{
		m_list.DeleteColumn(0);
	}
	m_list.InsertColumn(0, L"序号", 0, 80);
	m_list.InsertColumn(1, L"函数地址", 0, 120);
	DeviceIoControl(CmArkDlg::g_hSys,
		DF_GET_SSDT_COUNT,
		0,
		0,
		&ssdtNum,
		sizeof(DWORD),
		&size,
		NULL);
	/*CString buffer;
	buffer.Format(L"%d", ssdtNum);*/
	//MessageBox(buffer, L"ssdt个数", 0);
	GetSSDT();
}


void KernelDlg::OnBnClickedButton1()
{
	DWORD size = 0;
	DeviceIoControl(CmArkDlg::g_hSys,
		DF_KERNEL_RELOAD,
		0,
		0,
		0,
		0,
		&size,
		NULL);
	CString buffer;
	MessageBox(0, L"Kernel reload", 0);
}
