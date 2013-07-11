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
#include "BZScriptInterface.h"
#include "BZScriptWrapper.h"
#include "BZScriptView.h"
#include "BZScriptRuby.h"
#include "BZScriptPython.h"
#include "MainFrm.h"

// CBZScriptView

IMPLEMENT_DYNCREATE(CBZScriptView, CFormView)


void CBZScriptView::write(CString str)
{
	if(m_editResult.m_hWnd != NULL)
	{
		CString rstr;

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
}

CBZScriptView::~CBZScriptView()
{
	for(int i = 0; i < scripts.GetCount(); i++)
		scripts.GetAt(i)->getSIF()->cleanup(this);

	for(int i = 0; i < scripts.GetCount(); i++)
		delete scripts.GetAt(i);

	// sifs.RemoveAll()はdtorが何とかしてくれると思うので、呼ばなくても問題ない。
}

void CBZScriptView::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SCRIPT_RESULT, m_editResult);
	DDX_Control(pDX, IDC_SCRIPT_ENGINE, m_comboEngine);
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

	// TODO: 存在するプラグインをスキャンし、scripts.addする。
	{
		BZScriptWrapper *sw = new BZScriptWrapper();
		BOOL ok1 = sw->init(new BZScriptRuby(), this);
		ASSERT(ok1);
		scripts.Add(sw);
		m_comboEngine.AddString(sw->getSIF()->name());
	}
	{
		BZScriptWrapper *sw = new BZScriptWrapper();
		BOOL ok1 = sw->init(new BZScriptPython(), this);
		ASSERT(ok1);
		scripts.Add(sw);
		m_comboEngine.AddString(sw->getSIF()->name());
	}
	m_comboEngine.SetCurSel(0);

	ClearAll();
}

void CBZScriptView::ClearAll(void)
{
	m_editResult.SetWindowText(_T(""));
	m_editInput.SetWindowText(_T(""));
	//ruby_show_version(); // uses stdio (not $stdout.write)...

	for(int i = 0; i < scripts.GetCount(); i++)
		scripts.GetAt(i)->getSIF()->onClear(this);
}


void CBZScriptView::OnSize(UINT nType, int cx, int cy)
{
	CFormView::OnSize(nType, cx, cy);

	if(m_editResult.m_hWnd && m_editInput.m_hWnd)
	{
		CRect rc, rer, rce, rei;
		GetWindowRect(rc);
		m_editResult.GetWindowRect(rer);
		m_comboEngine.GetWindowRect(rce);
		m_editInput.GetWindowRect(rei);
		ScreenToClient(rc);
		ScreenToClient(rer);
		ScreenToClient(rce);
		ScreenToClient(rei);

		int margin = rer.left - rc.left;

		rer.right = rc.right - margin;
		rer.bottom = rc.bottom - margin - rei.Height() - margin;

		rce.top = rc.bottom - margin - rce.Height();
		rce.bottom = rc.bottom - margin;

		rei.right = rc.right - margin;
		rei.top = rc.bottom - margin - rei.Height();
		rei.bottom = rc.bottom - margin;

		m_editResult.MoveWindow(rer);
		m_comboEngine.MoveWindow(rce);
		m_editInput.MoveWindow(rei);
	}
}


void CBZScriptView::run(void)
{
	if((GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0)
	{
		CString istr, rstr, ostr;

		m_editInput.GetWindowText(istr);

		m_editInput.SetWindowText(_T(""));

		BZScriptWrapper *sw = scripts.GetAt(m_comboEngine.GetCurSel());

		m_editResult.GetWindowText(rstr);
		rstr.AppendFormat(_T("%s>%s\r\n"), sw->getSIF()->name(), istr);
		m_editResult.SetWindowText(rstr);

		ostr = sw->run(this, istr);

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
			if(GetAsyncKeyState(VK_CONTROL) & 0x8000)
			{
				int idx = m_comboEngine.GetCurSel() - 1;
				if(0 <= idx)
					m_comboEngine.SetCurSel(idx);
			} else
			{
				CString str;
				if(scripts.GetAt(m_comboEngine.GetCurSel())->getPrevHistory(str))
				{
					int strl = str.GetLength();
					m_editInput.SetWindowText(str);
					m_editInput.SetSel(strl, strl);
				}
			}
			return TRUE;
		} else if(pMsg->wParam == VK_DOWN)
		{
			if(GetAsyncKeyState(VK_CONTROL) & 0x8000)
			{
				int idx = m_comboEngine.GetCurSel() + 1;
				if(idx < m_comboEngine.GetCount())
					m_comboEngine.SetCurSel(idx);
			} else
			{
				CString str;
				if(scripts.GetAt(m_comboEngine.GetCurSel())->getNextHistory(str))
				{
					int strl = str.GetLength();
					m_editInput.SetWindowText(str);
					m_editInput.SetSel(strl, strl);
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
