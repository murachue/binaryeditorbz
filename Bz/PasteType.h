/*
licenced by New BSD License

Copyright (c) 1996-2013, c.mos(original author) & devil.tamachan@gmail.com(Modder) & Murachue (Modder)
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

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
	BOOL populateFormats(void);
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
