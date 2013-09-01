#pragma once
#include "afxwin.h"
#include "afxtempl.h"


// CPasteType ダイアログ

class CPasteType : public CDialogImpl<CPasteType>, public WTL::CWinDataExchange<CPasteType>
{
public:
	CPasteType();
	virtual ~CPasteType();

// ダイアログ データ
	enum { IDD = IDD_PASTETYPE };

protected:

public:
	WTL::CListBox m_FormatList;
	WTL::CStatic m_ItemSize;
protected:
	CArray<UINT> m_formats;
public:
	WTL::CButton m_OKButton;
	BOOL OnInitDialog(CWindow wndFocus, LPARAM lInitParam);
	void OnClickedOk(UINT uNotifyCode, int nID, CWindow wndCtl);
	void OnClickedCancel(UINT uNotifyCode, int nID, CWindow wndCtl);
	void OnLbnSelChangeCliptypelist(UINT uNotifyCode, int nID, CWindow wndCtl);

	BEGIN_MSG_MAP(CPasteType)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDOK, BN_CLICKED, OnClickedOk)
		COMMAND_HANDLER_EX(IDCANCEL, BN_CLICKED, OnClickedCancel)
		COMMAND_HANDLER_EX(IDC_CLIPTYPELIST, LBN_SELCHANGE, OnLbnSelChangeCliptypelist)
		COMMAND_HANDLER_EX(IDC_CLIPTYPELIST, LBN_DBLCLK, OnClickedOk) // Double click = OK
	END_MSG_MAP()

	BEGIN_DDX_MAP(CPasteType)
		DDX_CONTROL_HANDLE(IDC_CLIPTYPELIST, m_FormatList)
		DDX_CONTROL_HANDLE(IDC_CLIPITEMSIZE, m_ItemSize)
		DDX_CONTROL_HANDLE(IDOK, m_OKButton)
	END_DDX_MAP()
};
