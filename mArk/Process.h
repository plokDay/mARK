#pragma once
#include <vector>
//#include "../mArkDri/commonDef.h"

// Process 对话框

class Process : public CDialogEx
{
	DECLARE_DYNAMIC(Process)

public:
	Process(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~Process();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ProcessDlg };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
	BOOL GrantPriviledge(IN PWCHAR PriviledgeName, IN BOOL bEnable);//提升权限
	BOOL QueryProcessUserAccess(UINT32 ProcessId);//通过能否打开进程句柄判断能否让用户访问权限
	DECLARE_MESSAGE_MAP()
public:
	CMenu m_menu;

	virtual BOOL OnInitDialog();

	CListCtrl m_list;
	std::vector<PROCESS_ENTRY_INFO> m_ProcessEntryVector;
	static std::vector<MODULE_ENTRY> m_ModuleVector;
	static std::vector<THREAD_ENTRY> m_ThreadVector;
	static BOOL isModule;
	void PerfectProcessInfo(PPROCESS_ENTRY_INFO ProcessEntry);
	UINT32 GetProcessNum();//得到进程个数
	BOOL EnumProcessInfo();
	afx_msg void OnNMRClickList1(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnProtectProcess();
	afx_msg void OnTerminateProcess();
	afx_msg void OnEnumModuel();
	afx_msg void OnEnumThread();
	afx_msg void OnHideProcess();
};
