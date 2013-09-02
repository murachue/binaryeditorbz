#include "stdafx.h"
#include "Bz.h" // options
#include "BZView.h"
#include "BZDoc.h"
#include "BZScriptInterface.h"
#include "BZScriptView.h"
#include "BZScriptPython.h"

#if !defined(BZPYTHON2) ^ !defined(BZPYTHON3) == 0
#error "Define either BZPYTHON2 or BZPYTHON3 but not both."
#endif

// !!!!!
// Py_NO_ENABLE_SHARED also disables dllimport, I just want to avoid to linking against python27_d.lib in Debug config which is not included in Win32 Python distrib...
#ifdef _DEBUG
#define _DEBUG_IS_ON
#undef _DEBUG
#endif

#include "python.h"

#ifdef _DEBUG_IS_ON
#define _DEBUG
#endif

#ifdef BZPYTHON3
#define PyString_FromString PyBytes_FromString
#define PyString_FromStringAndSize PyBytes_FromStringAndSize
#define PyString_ConcatAndDel PyBytes_ConcatAndDel
#endif

static CBZScriptView *cbzsv; // TODO: Global...
static BOOL auto_invalidate_flag; // TODO: Global...


extern "C" BZScriptInterface __declspec(dllexport) * getScriptInterface(void)
{
	return new BZScriptPython();
}


BZScriptPython::BZScriptPython(void)
{
}


BZScriptPython::~BZScriptPython(void)
{
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

	cstr.Replace(_T("\n"), _T("\r\n"));
	cbzsv->write(cstr);

	Py_INCREF(Py_None);
	return Py_None;
}

#ifdef BZPYTHON3
static PyObject* python_flush(PyObject *self, PyObject *args)
{
	if(!PyArg_ParseTuple(args, ""))
		return NULL;

	// Do nothing.

	Py_INCREF(Py_None);
	return Py_None;
}
#endif

static PyMethodDef bzwriterMethods[] =
{
	{"write", python_write, METH_VARARGS, ""},
#ifdef BZPYTHON3
	{"flush", python_flush, METH_VARARGS, ""},
#endif
	{NULL, NULL, 0, NULL}
};

#ifdef BZPYTHON3
static PyModuleDef bzwriterModule = {
	PyModuleDef_HEAD_INIT, "bzwriter", NULL, -1, bzwriterMethods,
	NULL, NULL, NULL, NULL
};

static PyObject* PyInit_bzwriter(void)
{
	return PyModule_Create(&bzwriterModule);
}
#endif


static ULONGLONG convert_minus_index(LONGLONG i, ULONGLONG size)
{
	// TODO: 実装
	return i;
}

static PyObject *convert_bool(BOOL val)
{
	PyObject *obj;

	if(val)
	{
		obj = Py_True;
	} else
	{
		obj = Py_False;
	}

	Py_INCREF(obj);
	return obj;
}

static BOOL get_auto_invalidate(PyObject *self)
{
	/*
	PyObject *aikey = PyString_FromString("auto_invalidate");
	PyObject *ai = PyObject_GetItem(self, aikey);
	Py_DECREF(aikey);

	BOOL ret;
	if(PyObject_IsTrue(ai))
		ret = TRUE;
	else
		ret = FALSE;

	Py_DECREF(ai);

	return ret;
	*/
	return auto_invalidate_flag;
}

static void set_auto_invalidate(PyObject *self, BOOL val)
{
	/*
	PyObject *aikey = PyString_FromString("auto_invalidate");
	PyObject_SetItem(self, aikey, convert_bool(val));
	Py_DECREF(aikey);
	*/
	auto_invalidate_flag = val;
}

static void auto_invalidate(PyObject *self)
{
	if(get_auto_invalidate(self))
		cbzsv->m_pView->Invalidate();
}

static PyObject* bzpy_caret(PyObject *self, PyObject *args)
{
	LONGLONG i = LONGLONG_MIN;
	if(!PyArg_ParseTuple(args, "|L", &i)) return NULL;

	if(i == LONGLONG_MIN)
	{
		// get
		return Py_BuildValue("k", cbzsv->m_pView->m_dwCaret); // TODO: 4GB越え対応
	} else
	{
		// set
		CBZView *view = cbzsv->m_pView;
		DWORD old = view->m_dwCaret; // おまけ // TODO: 4GB越え対応
		cbzsv->m_pView->MoveCaretTo(convert_minus_index(i, view->GetDocument()->GetDocSize()));
		return Py_BuildValue("k", old); // TODO: 4GB越え対応
	}
}

// TODO: 4GB越え対応
static PyObject *data2pystr(CBZDoc *doc, DWORD begin, DWORD end)
{
	DWORD remain = end - begin; // TODO: 4GB越え対応
	PyObject *str = PyString_FromString("");
	while(remain > 0)
	{
		LPBYTE pdata = doc->QueryMapViewTama2(begin, remain);
		DWORD mappedsize = doc->GetMapRemain(begin); // TODO: 4GB越え対応
		DWORD size = min(mappedsize, remain); // TODO: 4GB越え対応
		PyObject *tstr = PyString_FromStringAndSize((const char*)pdata, size);
		PyString_ConcatAndDel(&str, tstr);
		if(str == NULL) return NULL;
		begin += size;
		remain -= size;
	}
	return str;
}

static PyObject* bzpy_data(PyObject *self, PyObject *args, PyObject *kwargs)
{
	LONGLONG begin = LONGLONG_MIN, end = LONGLONG_MIN;
	const char *data = NULL;
	Py_ssize_t datalen = 0; // TODO: 4GB越え対応?
	static /*const*/ char *kws[] = {"begin", "end", "data", NULL/*redundant sentinel*/};
	if(!PyArg_ParseTupleAndKeywords(args, kwargs, "|LLs#", kws, &begin, &end, &data, &datalen)) return NULL;
	//if(!PyArg_ParseTuple(args, "|LLs#", &begin, &end, &data, &datalen)) return NULL;

	CBZView *view = cbzsv->m_pView;
	CBZDoc *doc = view->GetDocument();
	DWORD docsize = doc->GetDocSize(); // TODO: 4GB越え対応

	if(begin == LONGLONG_MIN)
	{
		if(end == LONGLONG_MIN)
		{
			begin = 0;
			end = docsize;
		} else
		{
			begin = view->m_dwCaret;
		}
	}

	begin = convert_minus_index(begin, docsize);

	if(end == LONGLONG_MIN)
	{
		if(data == NULL)
		{
			end = begin + 1;
		} else
		{
			end = min(docsize, begin + datalen);
		}
	}

	end = convert_minus_index(end, docsize);

	if(end < begin)
	{
		PyErr_SetString(PyExc_IndexError, "\"begin\" beyonds \"end\"");
		return NULL;
	}

	if(docsize < end)
	{
		PyErr_SetString(PyExc_IndexError, "\"end\" beyonds document size");
		return NULL;
	}

	if(data == NULL)
	{
		// get
		return data2pystr(doc, begin, end);
	} else
	{
		if(doc->m_bReadOnly)
		{
			PyErr_SetString(PyExc_RuntimeError, "can't modify read-only file");
			return NULL;
		}

		DWORD dstsize = end - begin; // TODO: 4GB越え対応

#ifdef FILE_MAPPING
		if(doc->IsFileMapping() && dstsize != datalen)
		{
			// XXX: RuntimeErrorじゃなくてTypeError? うーんArgumentErrorがほしい…。
			PyErr_SetString(PyExc_RuntimeError, "can't modify with different size in file-mapping mode");
			return NULL;
		}
#endif

		if(dstsize == 0 && datalen == 0)
			return Py_BuildValue("s#", data, datalen); // おまけ: Py_INCREFしたPy_Noneでも可

		if(dstsize == 0) // 単に挿入される
		{
			doc->StoreUndo(begin, datalen, UNDO_DEL);
			doc->InsertData(begin, datalen, TRUE);
		} else if(dstsize < datalen) // 伸びる
		{
			// TODO: このundo操作、atomicの方がいい気がするが仕様的にできない…
			doc->StoreUndo(begin + dstsize, datalen - dstsize, UNDO_DEL);
			doc->StoreUndo(begin, dstsize, UNDO_OVR);
			doc->InsertData(begin + dstsize, datalen - dstsize, TRUE);
		} else if(dstsize == datalen) // 同じ長さ
		{
			doc->StoreUndo(begin, datalen, UNDO_OVR);
			//doc->InsertData(begin, valsize, FALSE); // 必要ないはず
		} else // valsize < docsize; 縮む
		{
			// TODO: このundo操作、atomicの方がいい気がするが仕様的にできない…
			doc->StoreUndo(begin + datalen, dstsize - datalen, UNDO_INS);
			doc->StoreUndo(begin, datalen, UNDO_OVR);
			doc->DeleteData(begin + datalen, dstsize - datalen);
		}

		DWORD endi = begin + datalen; // TODO: 4GB越え対応
		const char *valptr = data; // "data"を後で使うのでとっておく。
		DWORD valrem = datalen; // "datalen"を後で使うのでとっておく。 // TODO: 4GB越え対応
		for(DWORD i = begin; i < endi; )
		{
			LPBYTE pData = doc->QueryMapViewTama2(i, valrem);
			DWORD remain = doc->GetMapRemain(i); // TODO: 4GB越え対応
			DWORD ovwsize = min(remain, valrem); // TODO: 4GB越え対応
			memcpy(pData, valptr, ovwsize);
			i += ovwsize;
			valptr += ovwsize;
			valrem -= ovwsize;
		}
		view->UpdateDocSize();

		return Py_BuildValue("s#", data, datalen); // おまけ: Py_INCREFしたPy_Noneでも可
	}
}

static PyObject* bzpy_value(PyObject *self, PyObject *args, PyObject *kwargs)
{
	CBZView *view = cbzsv->m_pView;
	LONGLONG off = view->m_dwCaret;
	int size = view->m_nBytes;
	ULONGLONG value = ULONGLONG_MAX;
	static /*const*/ char *kws[] = {"offset", "width", "value", NULL/*redundant sentinel*/};
	if(!PyArg_ParseTupleAndKeywords(args, kwargs, "|LiK", kws, &off, &size, &value)) return NULL;

	off = convert_minus_index(off, view->GetDocument()->GetDocSize());

	if(value == ULONGLONG_MAX)
	{
		// get
		// TODO: valueはLONGLONG_MINの場合も有り得る…。
		if(size == 8)
		{
			return Py_BuildValue("K", view->GetValue64(off));
		} else if(size == 1 || size == 2 || size == 4)
		{
			return Py_BuildValue("k", view->GetValue(off, size));
		} else
		{
			// XXX: RuntimeErrorじゃなくてTypeError? うーんArgumentErrorがほしい…。
			PyErr_SetString(PyExc_RuntimeError, "size must be 1, 2, 4 or 8");
			return NULL;
		}
	} else
	{
		// set
		if(size == 1 || size == 2 || size == 4)
		{
			view->SetValue(off, size, value);
			return Py_BuildValue("k", value); // Py_INCREFしたPy_Noneでも可
		} else
		{
			// XXX: RuntimeErrorじゃなくてTypeError? うーんArgumentErrorがほしい…。
			PyErr_SetString(PyExc_RuntimeError, "size must be 1, 2 or 4");
			return NULL;
		}
	}
}

static PyObject* bzpy_block(PyObject *self, PyObject *args)
{
	CBZView *view = cbzsv->m_pView;
	LONGLONG begin = LONGLONG_MIN, end = LONGLONG_MIN;
	if(!PyArg_ParseTuple(args, "|LL", &begin, &end)) return NULL;

	if(begin == LONGLONG_MIN)
	{
		// get
		if(!view->IsBlockAvailable())
		{
			Py_INCREF(Py_None);
			return Py_None;
		}
		// TODO: 4GB越え対応(ULONGLONG/Py_BuildValue "K")
		return Py_BuildValue("(kk)", view->BlockBegin(), view->BlockEnd());
	} else if(end == LONGLONG_MIN)
	{
		// XXX: ...use "caret"!
		PyErr_SetString(PyExc_RuntimeError, "\"begin\" passed but \"end\"; Use caret()");
		return NULL;
	} else
	{
		// set
		CBZDoc *doc = view->GetDocument();
		DWORD docsize = doc->GetDocSize(); // TODO: 4GB越え対応

		begin = convert_minus_index(begin, docsize);
		end = convert_minus_index(end, docsize);

		if(begin < 0)
			begin = 0;
		if(docsize < begin)
			begin = docsize;
		if(end < 0)
			end = 0;
		if(docsize < end)
			end = docsize;

		view->setBlock(static_cast<DWORD>(begin), static_cast<DWORD>(end)); // TODO: 4GB越え対応

		Py_INCREF(Py_None);
		return Py_None;
	}
}

static PyObject* bzpy_mark(PyObject *self, PyObject *args, PyObject *kwargs)
{
	CBZView *view = cbzsv->m_pView;
	LONGLONG off = LONGLONG_MIN;
	PyObject *mark = Py_None;
	static /*const*/ char *kws[] = {"offset", "mark", NULL/*redundant sentinel*/};
	if(!PyArg_ParseTupleAndKeywords(args, kwargs, "|LO", kws, &off, &mark)) return NULL;

	if(off == LONGLONG_MIN)
	{
		off = view->m_dwCaret;
	}

	CBZDoc *doc = view->GetDocument();
	convert_minus_index(off, doc->GetDocSize());

	if(mark == Py_None)
	{
		// get
		return convert_bool(doc->CheckMark(off));
	} else
	{
		// set
		BOOL target;
		if(PyObject_IsTrue(mark))
		{
			target = TRUE;
		} else
		{
			target = FALSE;
		}

		BOOL org = doc->CheckMark(off);
		if(org != target)
		{
			doc->SetMark(off); // toggle.

			auto_invalidate(self);
		}

		return convert_bool(org); // おまけ; valかPy_Noneもあり
	}
}

static PyObject* bzpy_togglemark(PyObject *self, PyObject *args)
{
	CBZView *view = cbzsv->m_pView;
	LONGLONG off = LONGLONG_MIN;
	if(!PyArg_ParseTuple(args, "|L", &off)) return NULL;

	if(off == LONGLONG_MIN)
	{
		off = view->m_dwCaret;
	}

	CBZDoc *doc = view->GetDocument();
	convert_minus_index(off, doc->GetDocSize());

	// set
	BOOL org = doc->CheckMark(off);

	doc->SetMark(off); // toggle.

	auto_invalidate(self);

	return convert_bool(org); // おまけ; valかPy_Noneもあり
}

static PyObject* bzpy_filemapping(PyObject *self, PyObject *args)
{
	if(!PyArg_ParseTuple(args, "")) return NULL;

	return convert_bool(cbzsv->m_pView->GetDocument()->IsFileMapping());
}

static PyObject* bzpy_invalidate(PyObject *self, PyObject *args)
{
	if(!PyArg_ParseTuple(args, "")) return NULL;

	cbzsv->m_pView->Invalidate();

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* bzpy_endianess(PyObject *self, PyObject *args)
{
	int endianess = INT_MIN;
	if(!PyArg_ParseTuple(args, "|i", &endianess)) return NULL;

	if(endianess == INT_MIN)
	{
		// get
		return Py_BuildValue("i", options.bByteOrder ? 1 : 0);
	} else
	{
		// set
		if(endianess != 0 && endianess != 1)
		{
			PyErr_SetString(PyExc_IndexError, "endianess is allowed only become 0 or 1");
		}
		int old = options.bByteOrder;
		options.bByteOrder = endianess;

		// UTF-16はendianessで表示が変わるため必要。
		auto_invalidate(self);

		return Py_BuildValue("i", old ? 1 : 0);
	}
}

static PyObject* bzpy_filename(PyObject *self, PyObject *args)
{
	if(!PyArg_ParseTuple(args, "")) return NULL;

	return Py_BuildValue("s", CStringA(cbzsv->m_pView->GetDocument()->GetPathName()));
}

static PyObject* bzpy_len(PyObject *self, PyObject *args)
{
	if(!PyArg_ParseTuple(args, "")) return NULL;

	return Py_BuildValue("k", cbzsv->m_pView->GetDocument()->GetDocSize());
}

// TODO: イテレータ実装。Pythonは非常に面倒くさいようだ。
/*
static PyObject* bzpy_bytes(PyObject *self, PyObject *args)
{
	if(!PyArg_ParseTuple(args, "")) return NULL;

	CBZDoc *doc = cbzsv->m_pView->GetDocument();
	// TODO: 4GB越え対応
	DWORD begin = 0;
	DWORD end = doc->GetDocSize();
	for(DWORD i = begin; i < end; ) // TODO: 4GB越え対応
	{
		LPBYTE pData = doc->QueryMapViewTama2(i, end - begin);
		DWORD remain = doc->GetMapRemain(i);
		for(DWORD j = 0; j < remain; j++)
		{
			rb_yield(UINT2NUM(pData[j]));
		}
		i += remain;
	}

	return Py_BuildValue("i", i + 1);
}

static PyObject* bzpy_words(PyObject *self, PyObject *args)
{
	if(!PyArg_ParseTuple(args, "")) return NULL;

	CBZView *view = cbzsv->m_pView;
	DWORD wide = view->m_nBytes;
	CBZDoc *doc = view->GetDocument();
	// TODO: 4GB越え対応
	DWORD begin = 0;
	DWORD end = doc->GetDocSize();
	DWORD end_rd = end - ((end - begin) % wide);
	for(DWORD i = begin; i < end_rd; ) // TODO: 4GB越え対応
	{
		LPBYTE pData = doc->QueryMapViewTama2(i, end - begin);
		DWORD remain = doc->GetMapRemain(i);
		DWORD remain_rd = remain / wide * wide; // round_down(remain)
		for(DWORD j = 0; j < remain_rd; j += wide)
		{
			if(wide == 1 || wide == 2 || wide == 4)
				rb_yield(ULONG2NUM(view->GetValue(j, wide)));
			else if(wide == 8)
				rb_yield(ULL2NUM(view->GetValue64(j)));
			else
				ASSERT(FALSE); // panic
		}
		i += remain_rd;
	}

	return Py_BuildValue("i", i + 1);
}
*/

static PyObject* bzpy_wide(PyObject *self, PyObject *args)
{
	int wide = INT_MIN;
	if(!PyArg_ParseTuple(args, "i", &wide)) return NULL;

	if(wide == INT_MIN)
	{
		// get
		return Py_BuildValue("i", cbzsv->m_pView->m_nBytes);
	} else
	{
		// set
		if(!(wide == 1 || wide == 2 || wide == 4 || wide == 8))
		{
			// XXX: RuntimeErrorじゃなくてTypeError? うーんArgumentErrorがほしい…。
			PyErr_SetString(PyExc_RuntimeError, "wide must be 1, 2, 4 or 8");
		}
		int orgwide = cbzsv->m_pView->m_nBytes; // おまけ
		cbzsv->m_pView->m_nBytes = wide;
		return Py_BuildValue("i", orgwide);
	}
}

static PyObject* bzpy_autoinvalidate(PyObject *self, PyObject *args)
{
	PyObject *newai = Py_None;
	if(!PyArg_ParseTuple(args, "|O", &newai)) return NULL;
	PyObject *ai;

	ai = convert_bool(get_auto_invalidate(self));

	if(newai == Py_None)
	{
		// get
	} else
	{
		// set
		set_auto_invalidate(self, PyObject_IsTrue(newai));
	}

	return ai;
}

static PyObject* bzpy_undo(PyObject *self, PyObject *args)
{
	if(!PyArg_ParseTuple(args, "")) return NULL;

	CBZView *view = cbzsv->m_pView;
	CBZDoc *doc = view->GetDocument();
	if(doc->CanUndo())
	{
		doc->DoUndo();
		view->GotoCaret();
		view->UpdateDocSize();

		Py_INCREF(Py_True);
		return Py_True;
	} else
	{
		Py_INCREF(Py_False);
		return Py_False;
	}
}

static PyObject* bzpy_selected(PyObject *self, PyObject *args)
{
	if(!PyArg_ParseTuple(args, "")) return NULL;

	CBZView *view = cbzsv->m_pView;
	CBZDoc *doc = view->GetDocument();
	if(view->IsBlockAvailable())
	{
		return data2pystr(doc, view->BlockBegin(), view->BlockEnd());
	} else
	{
		Py_INCREF(Py_None);
		return Py_None;
	}
}

// XXX: どんなインターフェースが一般的かPythonistaに聞いて決める。
// TOOWTDI: "(|set)method" (to be modified to (get|set)method ...?)
// TMTOWTDI:
//   - __setattr__ / __getattr__
//   - (|get)method -- to get values
//   - map?
static PyMethodDef bzMethods[] =
{
	{"caret", bzpy_caret, METH_VARARGS, "caret()=long; Get caret position in bytes."},
	{"setcaret", bzpy_caret, METH_VARARGS, "setcaret(offset)=long; Set caret position in bytes, return caret position before set."},
	{"data", (PyCFunction)bzpy_data, METH_VARARGS | METH_KEYWORDS, "data(begin=0[or caret if end is not present],end=size[or caret+1 if begin is present])=str; Get part of data."},
	{"setdata", (PyCFunction)bzpy_data, METH_VARARGS | METH_KEYWORDS, "setdata(begin=0[or caret if end is not present],end=size[or caret if begin is present],data)=data; Insert or replace whole or part of data."},
	{"value", (PyCFunction)bzpy_value, METH_VARARGS | METH_KEYWORDS, "value(offset=caret)=long; Get part of data as a word."},
	{"setvalue", (PyCFunction)bzpy_value, METH_VARARGS | METH_KEYWORDS, "setvalue(offset=caret,data)=data; Set part of data as a word."},
	{"block", bzpy_block, METH_VARARGS, "block()=(begin,end); Get selection range."},
	{"setblock", bzpy_block, METH_VARARGS, "setblock(begin,end)=None; Set selection range."},
	{"mark", (PyCFunction)bzpy_mark, METH_VARARGS | METH_KEYWORDS, "mark(offset=caret)=long; Get mark."},
	{"togglemark", bzpy_togglemark, METH_VARARGS, "togglemark(offset=caret)=oldmark; Toggle mark."}, // call-only
	{"setmark", (PyCFunction)bzpy_mark, METH_VARARGS | METH_KEYWORDS, "setmark(offset=caret,mark)=oldmark; Set mark."},
	{"filemapping", bzpy_filemapping, METH_VARARGS, "filemapping()=bool; Get file is opened in File-mapping mode or not."}, // read-only
	{"invalidate", bzpy_invalidate, METH_VARARGS, "invalidate()=None; Redraw screen to reflect state to display."}, // call-only
	{"endianess", bzpy_endianess, METH_VARARGS, "endianess()=0[intel]/1[motorola]; Get endianess."},
	{"setendianess", bzpy_endianess, METH_VARARGS, "setendianess(0[intel]/1[motorola])=oldendianess; Set endianess."},
	{"filename", bzpy_filename, METH_VARARGS, "filename()=str; Get opened file name."}, // read-only
	{"len", bzpy_len, METH_VARARGS, "len()=long; Get data size."}, // read-only
	//{"bytes", bzpy_bytes, METH_VARARGS, ""}, // Iterator
	//{"words", bzpy_words, METH_VARARGS, ""}, // Iterator
	{"wide", bzpy_wide, METH_VARARGS, "wide()=int; Get word size."},
	{"setwide", bzpy_wide, METH_VARARGS, "setwide(int)=oldwide; Set word size."},
	{"autoinvalidate", bzpy_autoinvalidate, METH_VARARGS, "autoinvalidate()=bool; Set \"invalidate\" when operation that affects display is done."},
	{"setautoinvalidate", bzpy_autoinvalidate, METH_VARARGS, "setautoinvalidate(bool)=bool; Get \"invalidate\" or not when operation that affects display is done."},
	{"undo", bzpy_undo, METH_VARARGS, "undo()=bool[could undo]; Undo last change."}, // call-only
	{"selected", bzpy_selected, METH_VARARGS, "selected()=str; Get selected part of data."}, // bzruby's "b"
	// clipboard feature is hidden until merging b128dd8
	//{"clip", bzpy_, METH_VARARGS, ""},
	//{"setclip", bzpy_, METH_VARARGS, ""},
	{NULL, NULL, 0, NULL}
};

#define BZPYTHON_DOC "BZ embedded interface module."

#ifdef BZPYTHON3
static PyModuleDef bzModule = {
	PyModuleDef_HEAD_INIT, "bz", BZPYTHON_DOC, -1, bzMethods,
	NULL, NULL, NULL, NULL
};

static PyObject* PyInit_bz(void)
{
/*
	// test...
	PyObject *mod;
	mod = PyModule_Create(&bzModule);
	Py_DECREF(mod);
	return mod;
*/
	// http://docs.python.org/3/extending/embedding.html#extending-embedded-python のまま。
	return PyModule_Create(&bzModule);
}
#endif

BOOL BZScriptPython::init(CBZScriptView *sview)
{
#ifdef BZPYTHON2
	Py_SetProgramName("BZ");
#endif
#ifdef BZPYTHON3
	Py_SetProgramName(L"BZ");
	PyImport_AppendInittab("bz", &PyInit_bz);
#endif
	Py_Initialize();

	// "sys"をimportして色々するテスト…
	// なぜかsysがimportされたことにならない。

	//PyObject *name;
	//name = PyString_FromString("sys");
	//PyObject *mod = PyImport_ImportModule("sys");
	//PyObject *mod = PyImport_Import(name);
	//Py_DECREF(name);

	//PyObject *module = PyImport_AddModule("__main__");
	//PyObject *py_globals = (module == NULL) ? NULL : PyModule_GetDict(module);
	//PyObject *mod = PyImport_ImportModuleEx("sys", py_globals, py_globals, NULL);

	// sys.stdout/stderrを自作関数に置き換える
	// TODO: stdinもなんとかする。
	PyObject *mymodule;
#ifdef BZPYTHON2
	mymodule = Py_InitModule("bzwriter", bzwriterMethods);
#endif
#ifdef BZPYTHON3
	// bzwriterは隠しオブジェクトとして、PyImport_AppendInittabしない。
	mymodule = PyModule_Create(&bzwriterModule); // PyModule_Create may returns new reference...
#endif
	// PySys_SetObject(->PyDict_SetItem?)は、PyObject_SetAttrと違って参照を盗まないようだ。
	PySys_SetObject("stdout", mymodule);
	PySys_SetObject("stderr", mymodule);
	//Py_DECREF(mymodule); Py_InitModuleの戻り値は借り物なのでPy_DECREF不可

#ifdef BZPYTHON2
	mymodule = Py_InitModule3("bz", bzMethods, BZPYTHON_DOC);
#endif
#ifdef BZPYTHON3
	// Python3はPyModule_Create時に内部で参照を持ち、Py_Finalize時に放すように見える。
	// ここでPy_DECREFを呼んでもfreeされない。けど安全のため放置。多少のメモリーリークなんて知らない。
	// bzwriterの参照はstdout/stderrも握っている。
	//Py_DECREF(mymodule);

/*
	mymodule = PyImport_ImportModule("bz");
	Py_DECREF(mymodule);
*/
/*
	mymodule = PyModule_Create(&bzModule); // PyModule_Createはたぶん新しい参照を返す…?
	Py_DECREF(mymodule); // Py_DECREFするべき?
*/
#endif
	set_auto_invalidate(mymodule, TRUE);

	return TRUE;
}


void BZScriptPython::cleanup(CBZScriptView *sview)
{
	Py_Finalize();
}


void BZScriptPython::onClear(CBZScriptView* sview)
{
	cbzsv = sview;
	PyRun_SimpleString("import sys; print('Python ' + sys.version)");
}


CString BZScriptPython::run(CBZScriptView* sview, const char * cmdstr)
{
	cbzsv = sview;

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
#ifdef BZPYTHON2
	PyObject *py_result = PyEval_EvalCode((PyCodeObject*)py_code, py_globals, py_globals);
#endif
#ifdef BZPYTHON3
	PyObject *py_result = PyEval_EvalCode(py_code, py_globals, py_globals);
#endif
	//PYW_GIL_RELEASE;
	Py_DECREF(py_code);

	if(py_result == NULL || PyErr_Occurred())
	{
		PyErr_Print();
		return _T("");
	}
	if(py_result != Py_None)
	{
		PyObject *pystr = PyObject_Str(py_result);
		Py_DECREF(py_result);
#ifdef BZPYTHON2
		char *cstr = PyString_AsString(pystr);
		int cstrlen = strlen(cstr);
		CStringA csresult(cstr, cstrlen);
#endif
#ifdef BZPYTHON3
		// PyUnicode_AsUnicode->Py_UNICODE could be UCS4... only want wchar_t; use PyUnicode_AsWideChar(String).
		wchar_t *cstr = PyUnicode_AsWideCharString(pystr, NULL);
		int cstrlen = lstrlenW(cstr);
		CStringW csresult(cstr, cstrlen);
		PyMem_Free(cstr);
#endif
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


CString BZScriptPython::name(void)
{
#ifdef BZPYTHON2
	return CString(_T("Python2"));
#endif
#ifdef BZPYTHON3
	return CString(_T("Python3"));
#endif
}
