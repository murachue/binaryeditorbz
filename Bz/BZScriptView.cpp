/*
licenced by New BSD License

Copyright (c) 1996-2013, c.mos(original author) & devil.tamachan@gmail.com(Modder)
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
#include "BZView.h"
#include "BZDoc.h"
#include "BZScriptView.h"
#include "BZScriptRuby.h"
#include "BZScriptPython.h"
#include "MainFrm.h"

// CBZScriptView

IMPLEMENT_DYNCREATE(CBZScriptView, CFormView)


static CString outbuf;

void CBZScriptView::write(CString str)
{
	CString rstr;
	if(m_editResult.m_hWnd != NULL)
	{
		m_editResult.GetWindowText(rstr);
		// append output buf if there are
		rstr.Append(outbuf);
		outbuf.SetString(_T(""));
		// then append current str
		rstr.Append(str);
		m_editResult.SetWindowText(rstr);
	} else
	{
		outbuf.Append(str);
	}
}

CBZScriptView::CBZScriptView()
	: CFormView(CBZScriptView::IDD)
{
	sruby = new BZScriptRuby();
	sruby->init();
	spython = new BZScriptPython();
	spython->init();
	histidx = 0;
}

CBZScriptView::~CBZScriptView()
{
	sruby->cleanup();
	spython->cleanup();
}

void CBZScriptView::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SCRIPT_RESULT, m_editResult);
	DDX_Control(pDX, IDC_SCRIPT_INPUT, m_editInput);
}

BEGIN_MESSAGE_MAP(CBZScriptView, CFormView)
	ON_WM_CREATE()
	ON_WM_KEYDOWN()
	ON_WM_SIZE()
	ON_WM_GETDLGCODE()
END_MESSAGE_MAP()


// CBZScriptView 診断

#ifdef _DEBUG
void CBZScriptView::AssertValid() const
{
	CFormView::AssertValid();
}

#ifndef _WIN32_WCE
void CBZScriptView::Dump(CDumpContext& dc) const
{
	CFormView::Dump(dc);
}
#endif
#endif //_DEBUG


// CBZScriptView メッセージ ハンドラ

int CBZScriptView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFormView::OnCreate(lpCreateStruct) == -1)
		return -1;

	if(options.xSplitStruct == 0)
		options.xSplitStruct = lpCreateStruct->cx;

	return 0;
}

void CBZScriptView::OnInitialUpdate()
{
	CFormView::OnInitialUpdate();

	// TODO: ここに特定なコードを追加するか、もしくは基本クラスを呼び出してください。

	SetScrollSizes(MM_TEXT, CSize(0, 0));	
	m_pView = (CBZView*)GetNextWindow();
	ClearAll();
}

void CBZScriptView::ClearAll(void)
{
	m_editResult.SetWindowText(_T(""));
	m_editInput.SetWindowText(_T(""));
	//ruby_show_version(); // uses stdio (not $stdout.write)...

	sruby->onClear(this);
	spython->onClear(this);
}


void CBZScriptView::OnSize(UINT nType, int cx, int cy)
{
	CFormView::OnSize(nType, cx, cy);

	if(m_editResult.m_hWnd && m_editInput.m_hWnd)
	{
		CRect rc, rer, rei;
		GetWindowRect(rc);
		m_editResult.GetWindowRect(rer);
		m_editInput.GetWindowRect(rei);
		ScreenToClient(rc);
		ScreenToClient(rer);
		ScreenToClient(rei);

		int margin = rer.left - rc.left;

		rer.right = rc.right - margin;
		rer.bottom = rc.bottom - margin - rei.Height() - margin;

		rei.right = rc.right - margin;
		rei.top = rc.bottom - margin - rei.Height();
		rei.bottom = rc.bottom - margin;

		m_editResult.MoveWindow(rer);
		m_editInput.MoveWindow(rei);
	}
}


void CBZScriptView::run(void)
{
	if((GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0)
	{
		CString istr, rstr, ostr;

		m_editInput.GetWindowText(istr);

		CStringA zstr(istr);

		m_editInput.SetWindowText(_T(""));

		// 履歴の最後と同じなら履歴に追加しない
		if(history.GetCount() == 0 || history.GetAt(history.GetCount() - 1) != istr)
		{
			histidx = history.Add(istr) + 1;
		} else
		{
			histidx = history.GetCount();
		}

		m_editResult.GetWindowText(rstr);
		rstr.AppendFormat(_T(">%s\r\n"), istr);
		m_editResult.SetWindowText(rstr);

		if((GetAsyncKeyState(VK_SHIFT) & 0x8000) == 0)
			ostr = sruby->run(this, zstr);
		else
			ostr = spython->run(this, zstr);

		m_editResult.GetWindowText(rstr);
		rstr.Append(ostr);
		m_editResult.SetWindowText(rstr);
		m_editResult.SetSel(rstr.GetLength(),rstr.GetLength());
	}
}


// TODO: ctrl-c/ctrl-vがscript viewに行きつかないけど、ここで何とかできないか?
BOOL CBZScriptView::PreTranslateMessage(MSG* pMsg)
{
	// 入力窓でEnterが押されたら実行
	if(pMsg->hwnd == m_editInput.m_hWnd && pMsg->message == WM_KEYDOWN)
	{
		if(pMsg->wParam == VK_RETURN)
		{
			run();
			return TRUE;
		} else if(pMsg->wParam == VK_UP)
		{
			if(0 < histidx)
			{
				histidx--;
				CString str = history.GetAt(histidx);
				int strl = str.GetLength();
				m_editInput.SetWindowText(str);
				m_editInput.SetSel(strl, strl);
			}
			return TRUE;
		} else if(pMsg->wParam == VK_DOWN)
		{
			if(histidx < history.GetCount())
			{
				histidx++;
				if(histidx < history.GetCount())
				{
					CString str = history.GetAt(histidx);
					int strl = str.GetLength();
					m_editInput.SetWindowText(str);
					m_editInput.SetSel(strl, strl);
				} else
				{
					m_editInput.SetWindowText(_T(""));
				}
			}
			return TRUE;
		}
	}
	//*
	// その他メッセージも必要そうなものを結果窓および入力窓に届ける。
	// すごく重いかもしれない…
	// ここでTranslateMessageしてSendMessageとかちょっと具体的すぎる…
	// TODO: 超ad-hocなので将来的に抹消する
	if(pMsg->hwnd == m_editResult.m_hWnd && (pMsg->message == WM_KEYDOWN || pMsg->message == WM_KEYUP))
	{
		TranslateMessage(pMsg);
		m_editResult.SendMessage(pMsg->message, pMsg->wParam, pMsg->lParam);
		return TRUE;
	}
	if(pMsg->hwnd == m_editInput.m_hWnd && (pMsg->message == WM_KEYDOWN || pMsg->message == WM_KEYUP))
	{
		TranslateMessage(pMsg);
		m_editInput.SendMessage(pMsg->message, pMsg->wParam, pMsg->lParam);
		return TRUE;
	}
	//*/

	return CFormView::PreTranslateMessage(pMsg);
}


void CBZScriptView::OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView)
{
	// TODO: ここに特定なコードを追加するか、もしくは基本クラスを呼び出してください。

	CFormView::OnActivateView(bActivate, pActivateView, pDeactiveView);
	if(bActivate)
		m_editInput.SetFocus();
}
