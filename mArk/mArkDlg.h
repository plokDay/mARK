
// mArkDlg.h: 头文件
//

#pragma once
#include <vector>
#include <winsvc.h>
// CmArkDlg 对话框
class CmArkDlg : public CDialogEx
{
// 构造
public:
	CmArkDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = AHOME };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	SC_HANDLE hSCManager;
	SC_HANDLE m_hService;
	
	static HANDLE g_hSys;

	CTabCtrl m_tabCtl;
	std::vector<CDialogEx*> m_tabSubWnd;
	void addTabWnd(const CString& title, CDialogEx* pSubWnd, UINT uId);
	afx_msg void OnTcnSelchangeTab1(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnClose();
	void LoadDriver();
	void UnLoadDriver();
};
