#pragma once
#include <vector>

// DriverDlg 对话框

class DriverDlg : public CDialogEx
{
	DECLARE_DYNAMIC(DriverDlg)

public:
	DriverDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~DriverDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_Driver };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CListCtrl m_list;
	CMenu m_menu;
	virtual BOOL OnInitDialog();
	std::vector<DRIVER_ENTRY_INFO> m_DriverEntryVector;
	void EnumDriver();
	UINT32 GetDriverNum();
	afx_msg void OnRclickList1(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDriver32771();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
};
