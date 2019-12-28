// ModuleThreadDlg.cpp: 实现文件
//

#include "pch.h"
#include "mArk.h"
#include "ModuleThreadDlg.h"
#include "afxdialogex.h"
#include "Process.h"

// ModuleThreadDlg 对话框

IMPLEMENT_DYNAMIC(ModuleThreadDlg, CDialogEx)

ModuleThreadDlg::ModuleThreadDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_ModuleDlg, pParent)
{

}

ModuleThreadDlg::~ModuleThreadDlg()
{
}

void ModuleThreadDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST1, m_list);
}


BEGIN_MESSAGE_MAP(ModuleThreadDlg, CDialogEx)
END_MESSAGE_MAP()


// ModuleThreadDlg 消息处理程序


BOOL ModuleThreadDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	CString buff;

	if (Process::isModule == TRUE)
	{
		m_list.InsertColumn(0, L"模块完整路径", 0, 300);
		m_list.InsertColumn(1, L"基地址", 0, 120);
		m_list.InsertColumn(2, L"大小", 0, 120);
		for (int index = 0; index < Process::m_ModuleVector.size(); ++index)
		{
			buff.Format(L"%s", Process::m_ModuleVector[index].wzFilePath);
			m_list.InsertItem(index, buff);

			buff.Format(L"0x%08X", Process::m_ModuleVector[index].BaseAddress);
			m_list.SetItemText(index, 1, buff);
			buff.Format(L"0x%X", Process::m_ModuleVector[index].SizeOfImage);
			m_list.SetItemText(index, 2, buff);
		}
	}
	else
	{
		m_list.InsertColumn(0, L"TID", 0, 100);
		m_list.InsertColumn(1, L"ETHREAD", 0, 120);
		for (int index = 0; index < Process::m_ThreadVector.size(); ++index)
		{
			buff.Format(L"%d", Process::m_ThreadVector[index].TID);
			m_list.InsertItem(index, buff);

			buff.Format(L"0x%08X", Process::m_ThreadVector[index].Ethread);
			m_list.SetItemText(index, 1, buff);
		
		}
	}
	

	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}


void ModuleThreadDlg::GetModule()
{
	// TODO: 在此处添加实现代码.
}
