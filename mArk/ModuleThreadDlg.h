#pragma once
#include <vector>

// ModuleThreadDlg 对话框

class ModuleThreadDlg : public CDialogEx
{
	DECLARE_DYNAMIC(ModuleThreadDlg)

public:
	ModuleThreadDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~ModuleThreadDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ModuleDlg };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CListCtrl m_list;

	virtual BOOL OnInitDialog();
	void GetModule();
};
