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

// OMG PYTHON!
#undef HAVE_SYS_UTIME_H
#undef HAVE_STRERROR
#undef HAVE_HYPOT
#undef HAVE_STRFTIME
#undef copysign
#undef SIZEOF_OFF_T
#undef HAVE_TZNAME
#undef HAVE_PROTOTYPES
#undef HAVE_MKTIME
#undef HAVE_STDARG_PROTOTYPES

#undef ssize_t
#define ssize_t pyssize_t

// !!!!!
#ifdef _DEBUG
#define _DEBUG_IS_ON
#undef _DEBUG
#endif

#include "python.h"

#undef ssize_t
#ifdef _DEBUG_IS_ON
#define _DEBUG
#endif

// CBZScriptView

IMPLEMENT_DYNCREATE(CBZScriptView, CFormView)


static CBZScriptView *cbzsv; // TODO: YES GLOBAL AS RUBY DO!!!
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

static PyObject* python_write(PyObject *self, PyObject *args)
{
	int ok;
	char *str;
	int len;

	ok = PyArg_ParseTuple(args, "s#", &str, &len);
	if(!ok) // TODO: is it correct?
	{
		// TODO: exception
		PyErr_SetString(PyExc_NotImplementedError, "argument must be like write(str)");
		return NULL; // must NULL when raising exception?
	}
	CString cstr(str, len);

	CString rstr;
	cbzsv->m_editResult.GetWindowText(rstr);
	cstr.Replace(_T("\n"), _T("\r\n"));
	rstr.Append(cstr);
	cbzsv->m_editResult.SetWindowText(rstr);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef pythonMethods[] =
{
	{"write", python_write, METH_VARARGS, ""},
	{NULL, NULL, 0, NULL}
};

static void init_python(void)
{
	Py_SetProgramName("BZ");
	Py_Initialize();
	PyObject *name;
	//name = PyString_FromString("sys");
	//PyObject *mod = PyImport_ImportModule("sys");
	//PyObject *mod = PyImport_Import(name);
	//Py_DECREF(name);

	PyObject *module = PyImport_AddModule("__main__");
	PyObject *py_globals = (module == NULL) ? NULL : PyModule_GetDict(module);
	PyObject *mod = PyImport_ImportModuleEx("sys", py_globals, py_globals, NULL);

	// TODO: sys.stdout/stdin/stderrを自作関数に置き換える
	// なぜうごかない…
	PyObject *mymodule = Py_InitModule("bzwriter", pythonMethods);
	name = PyString_FromString("write");
	PyObject_SetAttr(mod, name, mymodule);
	Py_DECREF(name);
	Py_DECREF(mod);

	// しかたないのでad-hocに TODO: なんとかしてこの行を抹消する。
	PyRun_SimpleString("import sys; import bzwriter; sys.stdout=bzwriter; sys.stderr=bzwriter");
}

CBZScriptView::CBZScriptView()
	: CFormView(CBZScriptView::IDD)
{
	sruby = new BZScriptRuby();
	sruby->init();
	spython = new BZScriptPython();
	init_python();
}

CBZScriptView::~CBZScriptView()
{
	sruby->cleanup();
	Py_Finalize();
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
	cbzsv = this;
	PyRun_SimpleString("print('Python ' + sys.version)");
}


CString run_python(const char *cmdstr)
{
	// TODO: Isn't there method that handle write() as Ruby?
    //PyObject *pModule = PyImport_AddModule("__main__"); //create main module

	// referring IDAPython PythonEvalOrExec..
	PyCompilerFlags cf = {0};
	PyObject *py_code = Py_CompileStringFlags(cmdstr, "<string>", Py_eval_input, &cf);
	if(py_code == NULL || PyErr_Occurred())
	{
		// Not an expression?
		PyErr_Clear();

		// Run as a string
		PyRun_SimpleString(cmdstr);
		return _T("");
	}

	PyObject *module = PyImport_AddModule("__main__");
	PyObject *py_globals = (module == NULL) ? NULL : PyModule_GetDict(module);
	//PYW_GIL_ENSURE;
	PyObject *py_result = PyEval_EvalCode((PyCodeObject*)py_code, py_globals, py_globals);
	//PYW_GIL_RELEASE;
	Py_DECREF(py_code);

	if(py_result == NULL || PyErr_Occurred())
	{
		PyObject *t,*v,*b;
		PyErr_Fetch(&t,&v,&b);
		PyErr_Print();
		return _T("");
	}
	if(py_result != Py_None)
	{
		PyObject *pystr = PyObject_Str(py_result);
		Py_DECREF(py_result);
		char *cstr = PyString_AsString(pystr);
		int cstrlen = strlen(cstr);
		CStringA csresult(cstr, cstrlen);
		Py_DECREF(pystr);
		CString csnative(csresult);
		CString ostr;

		csnative.Replace(_T("\n"), _T("\r\n"));
		ostr.Format(_T("%s\r\n"), csnative);

		return ostr;
	}

	Py_DECREF(py_result);

	return _T("");//ostr;
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


// TODO: ctrl-c/ctrl-vがscript viewに行きつかないけど、ここで何とかできないか?
BOOL CBZScriptView::PreTranslateMessage(MSG* pMsg)
{
	// 入力窓でEnterが押されたら実行
	if(pMsg->hwnd == m_editInput.m_hWnd && pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
	{
		if((GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0)
		{
			CString istr, rstr, ostr;

			m_editInput.GetWindowText(istr);

			// TODO: XXX Unicodeビルド時しか考慮していない。
			CStringA zstr(istr);
			//zstr.AppendChar('\0'); // not need?

			m_editInput.SetWindowText(_T(""));

			m_editResult.GetWindowText(rstr);
			rstr.AppendFormat(_T(">%s\r\n"), istr);
			m_editResult.SetWindowText(rstr);

			if((GetAsyncKeyState(VK_SHIFT) & 0x8000) == 0)
				ostr = sruby->run(this, zstr);
			else
			{
				cbzsv = this;
				ostr = run_python(zstr);
			}

			m_editResult.GetWindowText(rstr);
			rstr.Append(ostr);
			m_editResult.SetWindowText(rstr);
			m_editResult.SetSel(rstr.GetLength(),rstr.GetLength());
		}

		return TRUE;
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
