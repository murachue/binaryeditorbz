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

#include "stdafx.h"
#include "BZ.h"
#include "PasteType.h"


// CPasteType ダイアログ

CPasteType::CPasteType() : CDialogImpl<CPasteType>()
{
}

CPasteType::~CPasteType()
{
}

static LPCTSTR clipFormatStrs[] = {
	_T("(Invalid)"), // 0
	_T("CF_TEXT"), // 1
	_T("CF_BITMAP"),
	_T("CF_METAFILEPICT"),
	_T("CF_SYLK"),
	_T("CF_DIF"),
	_T("CF_TIFF"),
	_T("CF_OEMTEXT"),
	_T("CF_DIB"),
	_T("CF_PALETTE"),
	_T("CF_PENDATA"),
	_T("CF_RIFF"),
	_T("CF_WAVE"),
	_T("CF_UNICODETEXT"),
	_T("CF_ENHMETAFILE"), // 14
//#if(WINVER >= 0x0400)
	_T("CF_HDROP"), // 15
	_T("CF_LOCALE"), // 16
//#endif /* WINVER >= 0x0400 */
//#if(WINVER >= 0x0500)
	_T("CF_DIBV5"), // 17
//#endif
};

BOOL CPasteType::populateFormats(void)
{
	m_FormatList.ResetContent();
	m_formats.RemoveAll();
	m_ItemSize.SetWindowText(_T(""));

	if(!OpenClipboard())
	{
		AfxMessageBox(IDS_CANT_OPEN_CLIPBOARD, MB_ICONERROR);
		return FALSE;
	}
	UINT i = 0;
	while((i = EnumClipboardFormats(i)) != 0)
	{
		TCHAR fmtname[256];
		int len;
		if(i < _countof(clipFormatStrs))
		{
			lstrcpyn(fmtname, clipFormatStrs[i], _countof(fmtname) - 1);
			len = lstrlen(fmtname);
		} else {
			len = GetClipboardFormatName(i, fmtname, _countof(fmtname) - 1);
		}
		// TODO: potentially cause buffer overflow; use CString?
		if(len == 0)
		{
			len = wsprintf(fmtname, _T("(id=%u=0x%x)"), i, i);
		} else {
			len += wsprintf(fmtname + len, _T(" (id=%u=0x%x)"), i, i);
		}
		fmtname[len] = '\0';
		m_FormatList.AddString(fmtname);
		m_formats.Add(i);
	}
	CloseClipboard();

	return TRUE;
}

// CPasteType メッセージ ハンドラー


BOOL CPasteType::OnInitDialog(CWindow wndFocus, LPARAM lInitParam)
{
	DoDataExchange(DDX_LOAD);

	if(populateFormats() == FALSE)
	{
		EndDialog(0);
	}

	return TRUE;
}

void CPasteType::OnClickedOk(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	DoDataExchange(DDX_SAVE);

	// クリップボードが変更されていないか確認。
	// 本当はClipboardViewerなどに登録して、変更されたときに通知すべきだが
	// Windows2000 or WindowsXP～専用なので見送り。
	// フォーマット一覧があっていればOKとする(実害ないので)。
	// 変更されていたらクリップボードのフォーマットを列挙しなおす。
	if(!OpenClipboard())
	{
		AfxMessageBox(IDS_CANT_OPEN_CLIPBOARD, MB_ICONERROR);
		EndDialog(0);
		return;
	}
	UINT i = 0, j = 0;
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
		AfxMessageBox(IDS_CLIPBOARD_MODIFIED, MB_ICONWARNING);
		if(populateFormats() == FALSE)
		{
			EndDialog(0);
		}
		return;
	}

	i = m_formats.GetAt(m_FormatList.GetCurSel());
	EndDialog(i); // TODO: int arg1 != UINT GetAt()
}


void CPasteType::OnClickedCancel(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	EndDialog(0);
}


void CPasteType::OnLbnSelChangeCliptypelist(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	UINT fmt = m_formats.GetAt(m_FormatList.GetCurSel());

	if(!OpenClipboard())
	{
		//AfxMessageBox(_T("クリップボードを開けません。"), MB_ICONERROR);
		// AfxMessageBoxの代わりにstaticを更新してみる(突然のポップアップ感を避ける)。
		CString buf;
		buf.LoadString(IDS_CANT_OPEN_CLIPBOARD);
		m_ItemSize.SetWindowText(buf);
		return;
	}
	HANDLE hMem;
	if((hMem = GetClipboardData(fmt)) == NULL)
	{
		CString buf;
		buf.Format(IDS_CANT_GETCLIPBOARDDATA, fmt);
		m_ItemSize.SetWindowText(buf);
	} else
	{
		CString buf;
		buf.Format(IDS_CLIPBOARD_ITEM_SIZE, GlobalSize(hMem));
		m_ItemSize.SetWindowText(buf);
	}
	CloseClipboard();
}
