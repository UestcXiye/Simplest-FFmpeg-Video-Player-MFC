
// Simplest FFmpeg Player MFCDlg.h : 头文件
//

#pragma once
#include "afxwin.h"


// CSimplestFFmpegPlayerMFCDlg 对话框
class CSimplestFFmpegPlayerMFCDlg : public CDialogEx
{
// 构造
public:
	CSimplestFFmpegPlayerMFCDlg(CWnd* pParent = NULL);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SIMPLESTFFMPEGPLAYERMFC_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedAbout();
	CEdit m_url;
	afx_msg void OnBnClickedFiledialog();
	afx_msg void OnBnClickedPlay();
	afx_msg void OnBnClickedPause();
	afx_msg void OnBnClickedStop();
	afx_msg void OnBnClickedClose();
};
