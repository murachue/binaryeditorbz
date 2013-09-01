#pragma once
#include "afxwin.h"
#include "afxtempl.h"


// CPasteType ダイアログ

class CPasteType : public CDialogEx
{
	DECLARE_DYNAMIC(CPasteType)

public:
	CPasteType(CWnd* pParent = NULL);   // 標準コンストラクター
	virtual ~CPasteType();

// ダイアログ データ
	enum { IDD = IDD_PASTETYPE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV サポート

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnLbnSelchangeCliptypelist();
	CListBox m_FormatList;
	CStatic m_ItemSize;
protected:
	CArray<int> m_formats;
public:
	afx_msg void OnClickedOk();
	afx_msg void OnClickedCancel();
	CButton m_OKButton;
};
