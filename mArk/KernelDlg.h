#pragma once
#include <vector>

// KernelDlg 对话框

class KernelDlg : public CDialogEx
{
	DECLARE_DYNAMIC(KernelDlg)

public:
	KernelDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~KernelDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_KernelDlg };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CListCtrl m_list;
	std::vector<IDT_ENTRY> m_IDTEntryVector;
	std::vector<GDT_ENTRY> m_GDTEntryVector;
	std::vector<LONG> m_SSDTEntryVector;

	virtual BOOL OnInitDialog();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	void GetIDT();
	void GetGDT();
	void GetSSDT();
	CButton m_rdIDT;
	afx_msg void OnBnClickedRadio2();
	afx_msg void OnBnClickedRadio1();
	afx_msg void OnBnClickedRadio3();
	afx_msg void OnBnClickedButton1();
};
