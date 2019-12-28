#pragma once
#include <vector>

// FileDlg 对话框

class FileDlg : public CDialogEx
{
	DECLARE_DYNAMIC(FileDlg)

public:
	FileDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~FileDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_FileDlg };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:

	CListCtrl m_list;
	std::vector<FIND_FILE_INFO> m_FileEntryVector;
	CMenu m_menu;

	virtual BOOL OnInitDialog();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	void FindFile();
	afx_msg void OnNMRDblclkList1(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void On32774();
};
