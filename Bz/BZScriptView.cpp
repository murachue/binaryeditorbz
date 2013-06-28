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

static VALUE ruby_write(VALUE self, VALUE str)
{
	Check_Type(str, T_STRING);

	str = rb_obj_as_string(str);
	CString cstr(RSTRING_PTR(str), RSTRING_LEN(str));

	CString rstr;
	cbzsv->m_editResult.GetWindowText(rstr);
	cstr.Replace(_T("\n"), _T("\r\n"));
	rstr.Append(cstr);
	cbzsv->m_editResult.SetWindowText(rstr);

	return Qnil;
}

static VALUE ruby_read(int argc, VALUE *argv, VALUE self)
{
	VALUE vsize;
	long lsize;

	if(rb_scan_args(argc, argv, "01", &vsize) == 0)
	{
		lsize = 0;
	} else
	{
		Check_Type(vsize, T_FIXNUM);
		lsize = FIX2LONG(vsize);
	}

	// TODO: stdinの扱いをどうするか。Dialog出す?
	// とりあえずEOF的な感じにしておく。
	VALUE emptystr = rb_str_new("", 0);

	return emptystr;
}

static VALUE ruby_caret(VALUE self) { return UINT2NUM(cbzsv->m_pView->m_dwCaret); }

static VALUE ruby_careteq(VALUE self, VALUE val)
{
	Check_Type(val, T_FIXNUM);
	VALUE orgcaret = UINT2NUM(cbzsv->m_pView->m_dwCaret); // おまけ
	cbzsv->m_pView->MoveCaretTo(FIX2ULONG(val)); // TODO: 4GB越え対応
	return orgcaret;
}

static VALUE ruby_data(VALUE self, VALUE idx_range)
{
	rb_exc_raise(rb_exc_new2(rb_eNotImpError, "Sorry!"));
}
static VALUE ruby_dataeq(VALUE self, VALUE idx_range, VALUE val)
{
	rb_exc_raise(rb_exc_new2(rb_eNotImpError, "Sorry!"));
}
static VALUE ruby_value(VALUE self, VALUE voff, VALUE vsize)
{
	ULONG off; // TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
	int size;

	Check_Type(voff, T_FIXNUM);
	Check_Type(vsize, T_FIXNUM);

	off = FIX2ULONG(voff);
	size = FIX2INT(vsize);

	if(size == 8)
	{
		return rb_ull2inum(cbzsv->m_pView->GetValue64(off));
	} else if(size == 1 || size == 2 || size == 4)
	{
		return UINT2NUM(cbzsv->m_pView->GetValue(off, size));
	} else
	{
		rb_exc_raise(rb_exc_new2(rb_eArgError, "size must be 1, 2, 4 or 8"));
	}
}
static VALUE ruby_valueeq(VALUE self, VALUE voff, VALUE vsize, VALUE vval)
{
	ULONG off; // TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
	int size;
	int val;

	Check_Type(voff, T_FIXNUM);
	Check_Type(vsize, T_FIXNUM);
	Check_Type(vval, T_FIXNUM);

	off = FIX2ULONG(voff);
	size = FIX2INT(vsize);
	val = FIX2INT(vval);

	if(size == 1 || size == 2 || size == 4)
	{
		cbzsv->m_pView->SetValue(off, size, val);
		return vval; // Qnilでも可
	} else
	{
		rb_exc_raise(rb_exc_new2(rb_eArgError, "size must be 1, 2 or 4"));
	}
}
static VALUE ruby_blockbegin(VALUE self) { return UINT2NUM(cbzsv->m_pView->BlockBegin()); } // TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
static VALUE ruby_blockend(VALUE self) { return UINT2NUM(cbzsv->m_pView->BlockEnd()); } // TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
static VALUE ruby_block(VALUE self) { return rb_funcall(rb_cRange, rb_intern("new"), 3, UINT2NUM(cbzsv->m_pView->BlockBegin()), UINT2NUM(cbzsv->m_pView->BlockEnd()), rb_cFalseClass); } // TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
// BZ.setblock(begin, end) 2 Fixnums
// BZ.setblock(begin...end) 1 Range
static VALUE ruby_setblock(int argc, VALUE *argv, VALUE self)
{
	// TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
	ULONG begin, end;

	VALUE v1, v2;
	int trueargc = rb_scan_args(argc, argv, "11", &v1, &v2);

	if(trueargc == 1)
	{
		if(CLASS_OF(v1) != rb_cRange)
			rb_exc_raise(rb_exc_new2(rb_eArgError, "arguments must be 2 fixnums or 1 range"));
		VALUE rbegin = rb_funcall(v1, rb_intern("begin"), 0);
		VALUE rend = rb_funcall(v1, rb_intern("end"), 0);
		VALUE reexc = rb_funcall(v1, rb_intern("exclude_end?"), 0);

		Check_Type(rbegin, T_FIXNUM);
		Check_Type(rend, T_FIXNUM);
		BOOL eexc;
		if(CLASS_OF(reexc) == rb_cTrueClass)
			eexc = TRUE;
		else if(CLASS_OF(reexc) == rb_cFalseClass)
			eexc = FALSE;
		else
			rb_exc_raise(rb_exc_new2(rb_eArgError, "Range#exclude_end? returns not true nor end!!"));

		begin = FIX2ULONG(rbegin);
		end = FIX2ULONG(rend) + (eexc ? 0 : 1);
	} else if(trueargc == 2)
	{
		Check_Type(v1, T_FIXNUM);
		Check_Type(v2, T_FIXNUM);

		begin = FIX2ULONG(v1);
		end = FIX2ULONG(v2);
	} else
	{
		ASSERT(FALSE); // panic
	}

	cbzsv->m_pView->setBlock(begin, end);

	return Qnil; // TODO: rangeでも返す?
}
static VALUE ruby_setmark(VALUE self, VALUE voff)
{
	// TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
	ULONG off;

	Check_Type(voff, T_FIXNUM);

	off = FIX2ULONG(voff);

	if(cbzsv->m_pView->GetDocument()->CheckMark(off) == FALSE)
		cbzsv->m_pView->GetDocument()->SetMark(off);

	return Qtrue;
}
static VALUE ruby_unsetmark(VALUE self, VALUE voff)
{
	// TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
	ULONG off;

	Check_Type(voff, T_FIXNUM);

	off = FIX2ULONG(voff);

	if(cbzsv->m_pView->GetDocument()->CheckMark(off) == FALSE)
		cbzsv->m_pView->GetDocument()->SetMark(off);

	return Qfalse;
}
static VALUE ruby_togglemark(VALUE self, VALUE voff)
{
	// TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
	ULONG off;

	Check_Type(voff, T_FIXNUM);

	off = FIX2ULONG(voff);

	cbzsv->m_pView->GetDocument()->SetMark(off);
	return cbzsv->m_pView->GetDocument()->CheckMark(off) ? Qtrue : Qfalse;
}
static VALUE ruby_ismarked(VALUE self, VALUE voff)
{
	// TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
	ULONG off;

	Check_Type(voff, T_FIXNUM);

	off = FIX2ULONG(voff);

	return cbzsv->m_pView->GetDocument()->CheckMark(off) ? Qtrue : Qfalse;
}
static VALUE ruby_isfilemapping(VALUE self) { return cbzsv->m_pView->GetDocument()->IsFileMapping() ? Qtrue : Qfalse; }
static VALUE ruby_invalidate(VALUE self) { cbzsv->m_pView->Invalidate(); return Qnil; }
static VALUE ruby_endianess(VALUE self) { return UINT2NUM(options.bByteOrder); } // 適当
static VALUE ruby_setendianess(VALUE self, VALUE vendian)
{
	BOOL endian;

	// boolなclassなら==で問題なさそう。
	if(CLASS_OF(vendian) == rb_cTrueClass)
		endian = TRUE;
	else if(CLASS_OF(vendian) == rb_cFalseClass)
		endian = FALSE;
	else
		rb_exc_raise(rb_exc_new2(rb_eArgError, "an argument must be true or false"));

	options.bByteOrder = endian;

	return UINT2NUM(options.bByteOrder); // おまけ
}
static VALUE ruby_isle(VALUE self) { return options.bByteOrder == 0 ? Qtrue : Qfalse; }
static VALUE ruby_isbe(VALUE self) { return options.bByteOrder != 0 ? Qtrue : Qfalse; }
static VALUE ruby_filename(VALUE self) { return rb_str_new2(CStringA(cbzsv->m_pView->GetDocument()->GetPathName())); }

extern "C" RUBY_EXTERN int rb_io_init_std;

static void init_ruby(void)
{
	rb_io_init_std = 0;
	ruby_init(); // ruby_init WORK ONLY ONCE.
	ruby_init_loadpath();

	// remap stdio
	rb_stdout = rb_obj_alloc(rb_cIO);
	rb_define_singleton_method(rb_stdout, "write", reinterpret_cast<VALUE(*)(...)>(ruby_write), 1);
	rb_stderr = rb_obj_alloc(rb_cIO);
	rb_define_singleton_method(rb_stderr, "write", reinterpret_cast<VALUE(*)(...)>(ruby_write), 1);
	rb_stdin = rb_obj_alloc(rb_cIO);
	rb_define_singleton_method(rb_stdin, "read", reinterpret_cast<VALUE(*)(...)>(ruby_read), -1);

	// export Bz remoting
	VALUE mBz = rb_define_module("BZ");
	rb_define_module_function(mBz, "caret", reinterpret_cast<VALUE(*)(...)>(ruby_caret), 0);
	rb_define_module_function(mBz, "caret=", reinterpret_cast<VALUE(*)(...)>(ruby_careteq), 1);
	rb_define_module_function(mBz, "[]", reinterpret_cast<VALUE(*)(...)>(ruby_data), 1);
	rb_define_module_function(mBz, "[]=", reinterpret_cast<VALUE(*)(...)>(ruby_dataeq), 2);
	rb_define_module_function(mBz, "value", reinterpret_cast<VALUE(*)(...)>(ruby_value), 2);
	rb_define_module_function(mBz, "setvalue", reinterpret_cast<VALUE(*)(...)>(ruby_valueeq), 3);
	//rb_define_module_function(mBz, "fill", reinterpret_cast<VALUE(*)(...)>(NULL), 0); // TODO: 実装する?
	rb_define_module_function(mBz, "blockbegin", reinterpret_cast<VALUE(*)(...)>(ruby_blockbegin), 0);
	rb_define_module_function(mBz, "blockend", reinterpret_cast<VALUE(*)(...)>(ruby_blockend), 0);
	rb_define_module_function(mBz, "block", reinterpret_cast<VALUE(*)(...)>(ruby_block), 0);
	rb_define_module_function(mBz, "setblock", reinterpret_cast<VALUE(*)(...)>(ruby_setblock), -1);
	rb_define_module_function(mBz, "setmark", reinterpret_cast<VALUE(*)(...)>(ruby_setmark), 1);
	rb_define_module_function(mBz, "unsetmark", reinterpret_cast<VALUE(*)(...)>(ruby_unsetmark), 1);
	rb_define_module_function(mBz, "togglemark", reinterpret_cast<VALUE(*)(...)>(ruby_togglemark), 1);
	rb_define_module_function(mBz, "ismarked", reinterpret_cast<VALUE(*)(...)>(ruby_ismarked), 1);
	rb_define_module_function(mBz, "ismarked?", reinterpret_cast<VALUE(*)(...)>(ruby_ismarked), 1);
	rb_define_module_function(mBz, "isfilemapping", reinterpret_cast<VALUE(*)(...)>(ruby_isfilemapping), 0);
	rb_define_module_function(mBz, "isfilemapping?", reinterpret_cast<VALUE(*)(...)>(ruby_isfilemapping), 0);
	rb_define_module_function(mBz, "invalidate", reinterpret_cast<VALUE(*)(...)>(ruby_invalidate), 0);
	rb_define_module_function(mBz, "setendianess", reinterpret_cast<VALUE(*)(...)>(ruby_setendianess), 0);
	rb_define_module_function(mBz, "endianess", reinterpret_cast<VALUE(*)(...)>(ruby_endianess), 0);
	rb_define_module_function(mBz, "isle", reinterpret_cast<VALUE(*)(...)>(ruby_isle), 0);
	rb_define_module_function(mBz, "isle?", reinterpret_cast<VALUE(*)(...)>(ruby_isle), 0);
	rb_define_module_function(mBz, "isbe", reinterpret_cast<VALUE(*)(...)>(ruby_isbe), 0);
	rb_define_module_function(mBz, "isbe?", reinterpret_cast<VALUE(*)(...)>(ruby_isbe), 0);
	rb_define_module_function(mBz, "filename", reinterpret_cast<VALUE(*)(...)>(ruby_filename), 0);
	//rb_define_module_function(mBz, "setfilename", reinterpret_cast<VALUE(*)(...)>(ruby_setfilename), 1);
	//rb_define_module_function(mBz, "open", reinterpret_cast<VALUE(*)(...)>(ruby_open), 1);
	//rb_define_module_function(mBz, "save", reinterpret_cast<VALUE(*)(...)>(ruby_save), 1);
	//rb_define_module_function(mBz, "new", reinterpret_cast<VALUE(*)(...)>(ruby_new), 1);
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
	PyObject *mymodule = Py_InitModule("mywriter", pythonMethods);
	name = PyString_FromString("write");
	PyObject_SetAttr(mod, name, mymodule);
	Py_DECREF(name);
	Py_DECREF(mod);
}

CBZScriptView::CBZScriptView()
	: CFormView(CBZScriptView::IDD)
{
	cbzsv = this;
	init_ruby();
	init_python();
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
}

void CBZScriptView::ClearAll(void)
{
	m_editResult.SetWindowText(_T(""));
	m_editInput.SetWindowText(_T(""));
	//ruby_show_version(); // uses stdio (not $stdout.write)...

	VALUE value;
	int state;

	value = rb_eval_string_protect("puts RUBY_DESCRIPTION", &state);

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
//    PyRun_SimpleString(cmdstr);
    //PyObject *catcher = PyObject_GetAttrString(pModule,"catchOutErr"); //get our catchOutErr created above
//    PyErr_Print(); //make python print any errors

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
		ostr.Format(_T("==> %s\r\n"), csnative);

		return ostr;
	}

	Py_DECREF(py_result);

	return _T("");//ostr;
}

CString run_ruby(const char *cmdstr)
{
	CString ostr;

	VALUE value;
	int state;

	// TODO: 環境が呼び出しごとにリセットされるのをなんとかする。
	value = rb_eval_string_protect(cmdstr, &state);

	if(state == 0)
	{
		value = rb_obj_as_string(value);
		CStringA csvalue(RSTRING_PTR(value), RSTRING_LEN(value));
		CString pstr(csvalue);
		pstr.Replace(_T("\n"), _T("\r\n"));
		ostr.Format(_T("#=> %s\r\n"), pstr);
	} else
	{
		value = rb_errinfo();
		VALUE klass = rb_obj_as_string(rb_funcall(value, rb_intern("class"), 0));
		VALUE messg = rb_obj_as_string(value);
		VALUE trace = rb_obj_as_string(rb_funcall(value, rb_intern("backtrace"), 0)); // TODO: already String?
		CStringA csklass(RSTRING_PTR(klass), RSTRING_LEN(klass));
		CStringA csmessg(RSTRING_PTR(messg), RSTRING_LEN(messg));
		CStringA cstrace(RSTRING_PTR(trace), RSTRING_LEN(trace));
		CStringA csvalue;
		csvalue.Format("%s: %s\n%s", csklass, csmessg, cstrace); // "\n" is ok. (converted later)
		CString pstr(csvalue);
		pstr.Replace(_T("\n"), _T("\r\n"));
		ostr.Format(_T("Error: state=%d\r\n%s\r\n"), state, pstr);
	}

	return ostr;
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
	// TODO: ここに特定なコードを追加するか、もしくは基本クラスを呼び出してください。
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
				ostr = run_ruby(zstr);
			else
				ostr = run_python(zstr);

			m_editResult.GetWindowText(rstr);
			rstr.Append(ostr);
			m_editResult.SetWindowText(rstr);
			m_editResult.SetSel(rstr.GetLength(),rstr.GetLength());
		}

		return TRUE;
	}

	return CFormView::PreTranslateMessage(pMsg);
}
