// PasteType.cpp : 実装ファイル
//

#include "stdafx.h"
#include "Bz.h"
#include "PasteType.h"
#include "afxdialogex.h"


// CPasteType ダイアログ

IMPLEMENT_DYNAMIC(CPasteType, CDialogEx)

CPasteType::CPasteType(CWnd* pParent /*=NULL*/)
	: CDialogEx(CPasteType::IDD, pParent)
{
	m_ItemSize.SetWindowText(_T(""));
	if(!OpenClipboard())
	{
		AfxMessageBox(_T("クリップボードが開けません。"), MB_ICONERROR);
		return;
	}
	int i = 0;
	while((i = EnumClipboardFormats(i)) != 0)
	{
		TCHAR fmtname[256];
		int len = GetClipboardFormatName(i, fmtname, sizeof(fmtname) / sizeof(fmtname[0]) - 1);
		fmtname[len] = '\0';
		m_FormatList.AddString(fmtname);
		m_formats.Add(i);
	}
	CloseClipboard();
}

CPasteType::~CPasteType()
{
}

void CPasteType::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CLIPTYPELIST, m_FormatList);
	DDX_Control(pDX, IDC_CLIPITEMSIZE, m_ItemSize);
	DDX_Control(pDX, IDOK, m_OKButton);
}


BEGIN_MESSAGE_MAP(CPasteType, CDialogEx)
	ON_LBN_SELCHANGE(IDC_CLIPTYPELIST, &CPasteType::OnLbnSelchangeCliptypelist)
	ON_BN_CLICKED(IDOK, &CPasteType::OnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CPasteType::OnClickedCancel)
END_MESSAGE_MAP()


// CPasteType メッセージ ハンドラー


void CPasteType::OnLbnSelchangeCliptypelist()
{
	// TODO: ここにコントロール通知ハンドラー コードを追加します。
}


void CPasteType::OnClickedOk()
{
	// クリップボードが変更されていないか確認。
	// 本当はClipboardViewerなどに登録して、変更されたときに通知すべきだが
	// Windows2000 or WindowsXP～専用なので見送り。
	// フォーマット一覧があっていればOKとする(実害ないので)。
	if(!OpenClipboard())
	{
		AfxMessageBox(_T("クリップボードが開けません。"), MB_ICONERROR);
		return;
	}
	int i = 0, j = 0;
	while((i = EnumClipboardFormats(i)) != 0)
	{
		if(i != m_formats.GetAt(j))
		{
			break;
		}
		j++;
	}
	CloseClipboard();

	if(i != 0)
	{
		AfxMessageBox(_T("クリップボードが変更されているため貼り付けられません。"), MB_ICONERROR);
		m_OKButton.EnableWindow(FALSE);
		return;
	}

	CDialogEx::OnOK();

	EndDialog(i);
}


void CPasteType::OnClickedCancel()
{
	// TODO: ここにコントロール通知ハンドラー コードを追加します。
	CDialogEx::OnCancel();
	EndDialog(0);
}
