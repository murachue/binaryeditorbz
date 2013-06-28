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
#include "BZScriptView.h"
#include "MainFrm.h"

//#define RUBY_EXPORT // dont dllimport
#define RUBY_DONT_SUBST // dont substitute snprintf etc.
#include "ruby.h"

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

extern "C"
static VALUE ruby_write(VALUE self, VALUE str)
{
	str = rb_obj_as_string(str);
	CString cstr(RSTRING_PTR(str), RSTRING_LEN(str));

	CString rstr;
	cbzsv->m_editResult.GetWindowText(rstr);
	cstr.Replace(_T("\n"), _T("\r\n"));
	rstr.Append(cstr);
	cbzsv->m_editResult.SetWindowText(rstr);

	return Qnil;
}

//*
static PyObject* python_write(PyObject *self, PyObject *args)
{
	// TODO: 1つめのobjectを取ってきて、strをなんとかしてcstringに直してappend.
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
//*/

extern "C" RUBY_EXTERN int rb_io_init_std;

CBZScriptView::CBZScriptView()
	: CFormView(CBZScriptView::IDD)
	//, m_bSigned(true)
{
	rb_io_init_std = 0;
	/*
	DWORD *ioinitstd = (DWORD*)GetProcAddress(LoadLibrary(_T("msvcr110-ruby200")),"rb_io_init_std");
	if(ioinitstd)
	{
		*ioinitstd = 0; // TODO: I USE BLACK MAGIC!
	}
	//*/
	ruby_init(); // ruby_init WORK ONLY ONCE.
	ruby_init_loadpath();

	cbzsv = this;
	rb_stdout = rb_obj_alloc(rb_cObject);
	rb_define_singleton_method(rb_stdout, "write", reinterpret_cast<VALUE(*)(...)>(ruby_write), 1);
	// rb_define_global_function("p", f_p, -1);
	//rb_stdin;
	//rb_stderr;

	Py_SetProgramName("BZ");
	Py_Initialize();
	PyObject *name = PyString_FromString("sys");
	PyObject *mod = PyImport_Import(name);
	Py_DECREF(name);
	// TODO: sys.stdout/stdin/stderrを自作関数に置き換える
	PyObject *mymodule = Py_InitModule("mywriter", pythonMethods);
	name = PyString_FromString("write");
	PyObject_SetAttr(mod, name, mymodule);
	Py_DECREF(name);
	Py_DECREF(mod);
}

CBZScriptView::~CBZScriptView()
{
	//ruby_cleanup(0); // DO NOT CALL, ruby_init DO WORK ONLY ONCE!
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

static WNDPROC origeditwndproc;
static LRESULT CALLBACK editinputsubproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// TODO: stop beep when hit return key.
	//*
	if(msg == WM_KEYDOWN && wParam == VK_RETURN)
		PostMessage(GetParent(hWnd), msg, wParam, lParam);
	//*/
	//*
	if(msg == WM_GETDLGCODE)
	{
		LPMSG lmsg = (LPMSG)lParam;
		if(lmsg && lmsg->message == WM_KEYDOWN && lmsg->wParam == VK_RETURN)
			return DLGC_WANTMESSAGE;
	}
	//*/
	return /*((WNDPROC)GetWindowLong(hWnd, GWL_USERDATA))*/origeditwndproc(hWnd, msg, wParam, lParam);
}

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

	m_pView = (CBZView*)GetNextWindow();
	ClearAll();

	//SetWindowLong(m_editInput.m_hWnd, GWL_USERDATA, GetWindowLong(m_editInput.m_hWnd, GWL_WNDPROC));
	origeditwndproc = (WNDPROC)GetWindowLong(m_editInput.m_hWnd, GWL_WNDPROC);
	SetWindowLong(m_editInput.m_hWnd, GWL_WNDPROC, (LONG)editinputsubproc);
}

void CBZScriptView::ClearAll(void)
{
	m_editResult.SetWindowText(_T(""));
	m_editInput.SetWindowText(_T(""));
	//ruby_show_version();

	VALUE value;
	int state;
	/*
	wchar_t *wstr;
	char *cstr;
	int wstrlen, cstrlen;
	CString rstr;
	*/

	value = rb_eval_string_protect("puts RUBY_DESCRIPTION", &state);

	/*
	value = rb_obj_as_string(value);
	cstr = RSTRING_PTR(value);
	cstrlen = RSTRING_LEN(value);
	wstrlen = MultiByteToWideChar(CP_THREAD_ACP, 0, cstr, cstrlen, NULL, 0);
	wstr = new wchar_t[wstrlen + 1];
	MultiByteToWideChar(CP_THREAD_ACP, 0, cstr, cstrlen, wstr, wstrlen + 1);

	m_editResult.GetWindowText(rstr);
	rstr.AppendFormat(_T("#=> %s\r\n"), CString(wstr, wstrlen));
	m_editResult.SetWindowText(rstr);

	delete wstr;
	*/

	PyRun_SimpleString("print('Python ' + sys.version)");
}


CString run_python(const char *cmdstr)
{
	// TODO: Isn't there method that handle write() as Ruby?
    const char *stdOutErr =
"import sys\n\
class CatchOutErr:\n\
    def __init__(self):\n\
        self.value = ''\n\
    def write(self, txt):\n\
        self.value += txt\n\
catchOutErr = CatchOutErr()\n\
sys.stdout = catchOutErr\n\
sys.stderr = catchOutErr\n\
"; //this is python code to redirect stdouts/stderr

    //Py_Initialize();
    PyObject *pModule = PyImport_AddModule("__main__"); //create main module
    //PyRun_SimpleString(stdOutErr); //invoke code to redirect
    PyRun_SimpleString(cmdstr);
    //PyObject *catcher = PyObject_GetAttrString(pModule,"catchOutErr"); //get our catchOutErr created above
    PyErr_Print(); //make python print any errors

    //PyObject *output = PyObject_GetAttrString(catcher,"value"); //get the stdout and stderr from our catchOutErr object
	//Py_DECREF(catcher);

	/*
    char *cstr = PyString_AsString(output);
	Py_DECREF(output);
	int cstrlen = strlen(cstr);
	wchar_t *wstr;
	int wstrlen;
	CString ostr;

	wstrlen = MultiByteToWideChar(CP_THREAD_ACP, 0, cstr, cstrlen, NULL, 0);
	wstr = new wchar_t[wstrlen + 1];
	MultiByteToWideChar(CP_THREAD_ACP, 0, cstr, cstrlen, wstr, wstrlen + 1);
	CString pstr(wstr, wstrlen);
	pstr.Replace(_T("\n"), _T("\r\n"));
	ostr.Format(_T("==> %s\r\n"), pstr);
	delete wstr;
	//*/

	return _T("");//ostr;
}

CString run_ruby(const char *cmdstr)
{
	CString ostr;
	wchar_t *wstr;
	int wstrlen;
	char *cstr;
	int cstrlen;

	VALUE value;
	int state;

	value = rb_eval_string_protect(cmdstr, &state);

	CStringA csvalue;
	if(state == 0)
	{
		ostr.SetString(_T("#=> "));
		value = rb_obj_as_string(value);
		cstr = RSTRING_PTR(value);
		cstrlen = RSTRING_LEN(value);
	} else
	{
		ostr.Format(_T("Error: state=%d\r\n"), state);
		value = rb_errinfo();
		VALUE klass = rb_obj_as_string(rb_funcall(value, rb_intern("class"), 0));
		VALUE messg = rb_obj_as_string(value);
		VALUE trace = rb_obj_as_string(rb_funcall(value, rb_intern("backtrace"), 0)); // TODO: already String?
		CStringA csklass(RSTRING_PTR(klass), RSTRING_LEN(klass));
		CStringA csmessg(RSTRING_PTR(messg), RSTRING_LEN(messg));
		CStringA cstrace(RSTRING_PTR(trace), RSTRING_LEN(trace));
		csvalue.Format("%s: %s\n%s\n", csklass, csmessg, cstrace); // "\n" is ok. (converted later)
		cstr = csvalue.GetBuffer();
		cstrlen = csvalue.GetLength() + 1;
	}

	wstrlen = MultiByteToWideChar(CP_THREAD_ACP, 0, cstr, cstrlen, NULL, 0);
	wstr = new wchar_t[wstrlen + 1];
	MultiByteToWideChar(CP_THREAD_ACP, 0, cstr, cstrlen, wstr, wstrlen + 1);
	CString pstr(wstr, wstrlen);
	pstr.Replace(_T("\n"), _T("\r\n"));
	ostr.AppendFormat(_T("%s\r\n"), pstr);
	delete wstr;

	return ostr;
}

void CBZScriptView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	// TODO: ここにメッセージ ハンドラー コードを追加するか、既定の処理を呼び出します。

	CFormView::OnKeyDown(nChar, nRepCnt, nFlags);

	if(nChar == VK_RETURN)
	{
		if((GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0)
		{
			CString istr, rstr, ostr;
			wchar_t *wstr;
			int wstrlen;
			char *cstr;
			int cstrlen;
			int state = 0;

			m_editInput.GetWindowText(istr);

			// TODO: XXX Unicodeビルド時しか考慮していない。
			wstr = istr.GetBuffer();
			wstrlen = istr.GetLength();
			cstrlen = WideCharToMultiByte(CP_THREAD_ACP, 0, wstr, wstrlen, NULL, 0, NULL, NULL);
			cstr = new char[cstrlen + 1];
			WideCharToMultiByte(CP_THREAD_ACP, 0, wstr, wstrlen, cstr, cstrlen + 1, NULL, NULL);
			cstr[cstrlen] = '\0';

			m_editInput.SetWindowText(_T(""));

			m_editResult.GetWindowText(rstr);
			rstr.AppendFormat(_T(">%s\r\n"), istr);
			m_editResult.SetWindowText(rstr);

			if((GetAsyncKeyState(VK_SHIFT) & 0x8000) == 0)
				ostr = run_ruby(cstr);
			else
				ostr = run_python(cstr);

			delete cstr;

			m_editResult.GetWindowText(rstr);
			rstr.Append(ostr);
			m_editResult.SetWindowText(rstr);
		}
	}
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


UINT CBZScriptView::OnGetDlgCode()
{
	UINT result = CFormView::OnGetDlgCode(); // __super::OnGetDlgCode();
	const MSG* lpMsg = CWnd::GetCurrentMessage();
	lpMsg = (LPMSG)lpMsg->lParam;
	if (lpMsg && lpMsg->message == WM_KEYDOWN && lpMsg->wParam == VK_RETURN)
		result |= DLGC_WANTMESSAGE;
	return result;
}
