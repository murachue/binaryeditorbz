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
static CString outbuf;

static VALUE bzruby_write(VALUE self, VALUE str)
{
	Check_Type(str, T_STRING);

	str = rb_obj_as_string(str);
	CString cstr(RSTRING_PTR(str), RSTRING_LEN(str));
	cstr.Replace(_T("\n"), _T("\r\n"));

	CString rstr;
	if(cbzsv->m_editResult.m_hWnd != NULL)
	{
		cbzsv->m_editResult.GetWindowText(rstr);
		// append output buf if there are
		rstr.Append(outbuf);
		outbuf.SetString(_T(""));
		// then append current str
		rstr.Append(cstr);
		cbzsv->m_editResult.SetWindowText(rstr);
	} else
	{
		outbuf.Append(cstr);
	}

	return Qnil;
}

static VALUE bzruby_read(int argc, VALUE *argv, VALUE self)
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

static VALUE bzruby_caret(VALUE self) { return UINT2NUM(cbzsv->m_pView->m_dwCaret); }

static VALUE bzruby_careteq(VALUE self, VALUE val)
{
	Check_Type(val, T_FIXNUM);
	VALUE orgcaret = UINT2NUM(cbzsv->m_pView->m_dwCaret); // おまけ
	cbzsv->m_pView->MoveCaretTo(FIX2ULONG(val)); // TODO: 4GB越え対応
	return orgcaret;
}

// [begin,end] (inclusive)
// TODO: 4GB越え対応
static VALUE bzruby_data2str(CBZDoc *doc, DWORD begin, DWORD end)
{
	DWORD remain = end - begin + 1;
	VALUE rstr = rb_str_new_cstr("");
	while(remain > 0)
	{
		LPBYTE pdata = doc->QueryMapViewTama2(begin, remain);
		DWORD mappedsize = doc->GetMapRemain(begin); // TODO: 4GB越え対応
		DWORD size = min(mappedsize, remain); // TODO: 4GB越え対応
		rb_str_cat(rstr, (const char*)pdata, size);
		begin += size;
		remain -= size;
	}

	return rstr;
}
static VALUE bzruby_bracket(VALUE self, VALUE idx_range)
{
	int64_t ibegin, iend; // [ibegin,iend] (inclusive)

	if(TYPE(idx_range) == T_FIXNUM || TYPE(idx_range) == T_BIGNUM)
	{
		ibegin = iend = NUM2LL(idx_range);
	} else if(rb_funcall(idx_range, rb_intern("is_a?"), 1, rb_cRange))
	{
		ibegin = NUM2LL(rb_funcall(idx_range, rb_intern("begin"),0));
		iend = NUM2LL(rb_funcall(idx_range, rb_intern("end"),0));
		if(rb_funcall(idx_range, rb_intern("exclude_end?"),0) == Qtrue)
			iend -= 1;
	} else
	{
		rb_exc_raise(rb_exc_new2(rb_eArgError, "an argument must be Fixnum, Bignum or Range"));
	}

	CBZDoc *doc = cbzsv->m_pView->GetDocument();
	DWORD docsize = doc->GetDocSize(); // TODO: 4GB越え対応

	if(ibegin < 0) ibegin = (int64_t)docsize + ibegin;
	if(iend < 0) iend = (int64_t)docsize + iend;

	if(iend - ibegin < 0)
	{
		return rb_str_new_cstr("");
	}

	// Stringはこういう動作らしい…
	if(ibegin == docsize)
	{
		return rb_str_new_cstr("");
	}
	if(ibegin < 0 || docsize < ibegin)
	{
		return Qnil;
	}
	if((iend < 0 || docsize <= iend) && iend != ibegin)
	{
		// Rangeだったらiendがファイルサイズを超えていた場合に切り詰める
		iend = docsize - 1;
	}

	// do fetch and return it to Ruby world!
	// TODO: 4GB越え対応
	DWORD begin = static_cast<DWORD>(ibegin);
	DWORD end = end = static_cast<DWORD>(iend);
	
	return bzruby_data2str(doc, begin, end);
}
static VALUE bzruby_bracketeq(VALUE self, VALUE idx_range, VALUE val)
{
	// TODO: BZ[idx/range,wide]=value形式も受け付けるか? (script利用者はendian考えなくていいので楽)
	int64_t ibegin, iend; // [ibegin,iend] (inclusive)
	if(TYPE(idx_range) == T_FIXNUM || TYPE(idx_range) == T_BIGNUM)
	{
		ibegin = iend = NUM2LL(idx_range);
	} else if(rb_funcall(idx_range, rb_intern("is_a?"), 1, rb_cRange))
	{
		ibegin = NUM2LL(rb_funcall(idx_range, rb_intern("begin"),0));
		iend = NUM2LL(rb_funcall(idx_range, rb_intern("end"),0));
		if(rb_funcall(idx_range, rb_intern("exclude_end?"),0) == Qtrue)
			iend -= 1;
	} else
	{
		rb_exc_raise(rb_exc_new2(rb_eArgError, "an argument must be Fixnum, Bignum or Range"));
	}
	Check_Type(val, T_STRING);

	CBZDoc *doc = cbzsv->m_pView->GetDocument();

	if(doc->m_bReadOnly)
	{
		rb_exc_raise(rb_exc_new2(rb_eRuntimeError, "can't modify read-only file"));
	}

	DWORD docsize = doc->GetDocSize(); // TODO: 4GB越え対応

	rb_exc_raise(rb_exc_new2(rb_eNotImpError, "Sorry!"));
}
static VALUE bzruby_data(VALUE self)
{
	CBZDoc *doc = cbzsv->m_pView->GetDocument();
	DWORD docsize = doc->GetDocSize(); // TODO: 4GB越え対応

	return bzruby_data2str(doc, 0, docsize - 1);
}
static VALUE bzruby_dataeq(VALUE self, VALUE val)
{
	Check_Type(val, T_STRING);

	CBZView *view = cbzsv->m_pView;
	CBZDoc *doc = view->GetDocument();

	if(doc->m_bReadOnly)
	{
		rb_exc_raise(rb_exc_new2(rb_eRuntimeError, "can't modify read-only file"));
	}

	DWORD docsize = doc->GetDocSize(); // TODO: 4GB越え対応

#ifdef FILE_MAPPING
	if(doc->IsFileMapping() && RSTRING_LEN(val) != docsize)
	{
		rb_exc_raise(rb_exc_new2(rb_eArgError, "can't modify with different size in file-mapping mode"));
	}
#endif

	const char *valptr = RSTRING_PTR(val);
	DWORD valsize = RSTRING_LEN(val); // TODO: 4GB越え対応

	if(docsize == 0)
	{
		doc->StoreUndo(0, valsize, UNDO_DEL);
	} else if(docsize < valsize)
	{
		// TODO: このundo操作、atomicの方がいい気がするが仕様的にできない…
		doc->StoreUndo(docsize, valsize - docsize, UNDO_DEL);
		doc->StoreUndo(0, docsize, UNDO_OVR);
	} else // valsize <= docsize
	{
		// TODO: このundo操作、atomicの方がいい気がするが仕様的にできない…
		doc->StoreUndo(valsize, docsize - valsize, UNDO_INS);
		doc->StoreUndo(0, valsize, UNDO_OVR);
	}
	doc->InsertData(0, valsize, FALSE);
	for(DWORD i = 0; i < valsize; )
	{
		LPBYTE pData = doc->QueryMapViewTama2(i, valsize);
		DWORD remain = doc->GetMapRemain(i);
		DWORD ovwsize = min(remain, valsize);
		memcpy(pData, valptr, ovwsize);
		i += ovwsize;
		valptr += ovwsize;
	}
	if(valsize < docsize)
		doc->DeleteData(valsize, docsize - valsize);
	view->UpdateDocSize();

	return val; // おまけ: Qnilでも可
}

static VALUE bzruby_wide(VALUE self);
// BZ.value(offset = BZ.caret, size = BZ.wide)
static VALUE bzruby_value(int argc, VALUE *argv, VALUE self)
{
	VALUE voff;
	VALUE vsize;

	switch(rb_scan_args(argc, argv, "02", &voff, &vsize))
	{
	case 0:
		voff = bzruby_caret(self);
		/* FALLTHROUGH */
	case 1:
		vsize = bzruby_wide(self);
		/* FALLTHROUGH */
	case 2:
		// OK
		break;
	default:
		ASSERT(FALSE); // panic
	}

	Check_Type(voff, T_FIXNUM);
	Check_Type(vsize, T_FIXNUM);

	ULONG off = FIX2ULONG(voff); // TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
	int size = FIX2INT(vsize);

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
// BZ.value=val
// BZ.setvalue(val)
// BZ.setvalue(off, val)
// BZ.setvalue(off, size, val)
static VALUE bzruby_valueeq(int argc, VALUE *argv, VALUE self)
{
	VALUE voff;
	VALUE vsize;
	VALUE vval;
	VALUE v1, v2, v3;

	switch(rb_scan_args(argc, argv, "12", &v1, &v2, &v3))
	{
	case 1:
		voff = bzruby_caret(self);
		vsize = bzruby_wide(self);
		vval = v1;
		break;
	case 2:
		voff = v1;
		vsize = bzruby_wide(self);
		vval = v2;
		break;
	case 3:
		voff = v1;
		vsize = v2;
		vval = v3;
		break;
	default:
		ASSERT(FALSE); // panic
	}

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
static VALUE bzruby_blockbegin(VALUE self)
{
	if(!cbzsv->m_pView->IsBlockAvailable())
		return Qnil;
	// TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
	return UINT2NUM(cbzsv->m_pView->BlockBegin());
}
static VALUE bzruby_blockend(VALUE self)
{
	if(!cbzsv->m_pView->IsBlockAvailable())
		return Qnil;
	// TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
	return UINT2NUM(cbzsv->m_pView->BlockEnd());
}
static VALUE bzruby_block(VALUE self)
{
	if(!cbzsv->m_pView->IsBlockAvailable())
		return Qnil;
	// TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
	return rb_funcall(rb_cRange, rb_intern("new"), 3, UINT2NUM(cbzsv->m_pView->BlockBegin()), UINT2NUM(cbzsv->m_pView->BlockEnd()), rb_cFalseClass);
}
// BZ.setblock(begin, end) 2 Fixnums
// BZ.setblock(begin...end) 1 Range
static VALUE bzruby_setblock(int argc, VALUE *argv, VALUE self)
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
static VALUE bzruby_setmark(VALUE self, VALUE voff)
{
	// TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
	ULONG off;

	Check_Type(voff, T_FIXNUM);

	off = FIX2ULONG(voff);

	if(cbzsv->m_pView->GetDocument()->CheckMark(off) == FALSE)
		cbzsv->m_pView->GetDocument()->SetMark(off);

	return Qtrue;
}
static VALUE bzruby_unsetmark(VALUE self, VALUE voff)
{
	// TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
	ULONG off;

	Check_Type(voff, T_FIXNUM);

	off = FIX2ULONG(voff);

	if(cbzsv->m_pView->GetDocument()->CheckMark(off) == FALSE)
		cbzsv->m_pView->GetDocument()->SetMark(off);

	return Qfalse;
}
static VALUE bzruby_togglemark(VALUE self, VALUE voff)
{
	// TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
	ULONG off;

	Check_Type(voff, T_FIXNUM);

	off = FIX2ULONG(voff);

	cbzsv->m_pView->GetDocument()->SetMark(off);
	return cbzsv->m_pView->GetDocument()->CheckMark(off) ? Qtrue : Qfalse;
}
static VALUE bzruby_ismarked(VALUE self, VALUE voff)
{
	// TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
	ULONG off;

	Check_Type(voff, T_FIXNUM);

	off = FIX2ULONG(voff);

	return cbzsv->m_pView->GetDocument()->CheckMark(off) ? Qtrue : Qfalse;
}
#ifdef FILE_MAPPING
static VALUE bzruby_isfilemapping(VALUE self) { return cbzsv->m_pView->GetDocument()->IsFileMapping() ? Qtrue : Qfalse; }
#endif
static VALUE bzruby_invalidate(VALUE self) { cbzsv->m_pView->Invalidate(); return Qnil; }
static VALUE bzruby_endianess(VALUE self) { return UINT2NUM(options.bByteOrder ? 1 : 0); } // 適当
static VALUE bzruby_setendianess(VALUE self, VALUE vendian)
{
	int endian;
	int old_endian;

	Check_Type(vendian, T_FIXNUM);
	endian = NUM2INT(vendian);
	if(!(endian == 0 || endian == 1))
		rb_exc_raise(rb_exc_new2(rb_eArgError, "endianess must be 0 or 1"));

	old_endian = options.bByteOrder == TRUE ? 1 : 0;
	options.bByteOrder = (endian == 1) ? TRUE : FALSE;

	return UINT2NUM(old_endian); // おまけ
}
static VALUE bzruby_isle(VALUE self) { return options.bByteOrder == 0 ? Qtrue : Qfalse; }
static VALUE bzruby_isbe(VALUE self) { return options.bByteOrder != 0 ? Qtrue : Qfalse; }
static VALUE bzruby_filename(VALUE self) { return rb_str_new2(CStringA(cbzsv->m_pView->GetDocument()->GetPathName())); }
static VALUE bzruby_size(VALUE self)
{
	// TODO: 4GB越え対応
	return ULONG2NUM(static_cast<ULONG>(cbzsv->m_pView->GetDocument()->GetDocSize()));
}
static VALUE bzruby_each_byte(VALUE self)
{
	RETURN_SIZED_ENUMERATOR(self, 0, 0, reinterpret_cast<VALUE(*)(...)>(bzruby_size));

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

	return Qnil;
}
// TODO: 隠しメソッドのままにする?
static VALUE bzruby_wordsize(VALUE self)
{
	// TODO: 4GB越え対応
	return ULONG2NUM(static_cast<ULONG>(cbzsv->m_pView->GetDocument()->GetDocSize() / cbzsv->m_pView->m_nBytes));
}
static VALUE bzruby_each_word(VALUE self)
{
	RETURN_SIZED_ENUMERATOR(self, 0, 0, reinterpret_cast<VALUE(*)(...)>(bzruby_wordsize));

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

	return Qnil;
}
static VALUE bzruby_wide(VALUE self) { return UINT2NUM(cbzsv->m_pView->m_nBytes); }
static VALUE bzruby_wideeq(VALUE self, VALUE vval)
{
	Check_Type(vval, T_FIXNUM);
	int val = FIX2INT(vval);
	if(!(val == 1 || val == 2 || val == 4 || val == 8))
	{
		rb_exc_raise(rb_exc_new2(rb_eArgError, "wide must be 1, 2, 4 or 8"));
	}
	VALUE orgwide = UINT2NUM(cbzsv->m_pView->m_nBytes); // おまけ
	cbzsv->m_pView->m_nBytes = val;
	return orgwide;
}

extern "C" RUBY_EXTERN int rb_io_init_std;

static void init_ruby(void)
{
	rb_io_init_std = 0;
	ruby_init(); // ruby_init WORK ONLY ONCE.

	// remap stdio
	rb_stdout = rb_obj_alloc(rb_cIO);
	rb_define_singleton_method(rb_stdout, "write", reinterpret_cast<VALUE(*)(...)>(bzruby_write), 1);
	rb_stderr = rb_obj_alloc(rb_cIO);
	rb_define_singleton_method(rb_stderr, "write", reinterpret_cast<VALUE(*)(...)>(bzruby_write), 1);
	rb_stdin = rb_obj_alloc(rb_cIO);
	rb_define_singleton_method(rb_stdin, "read", reinterpret_cast<VALUE(*)(...)>(bzruby_read), -1);

	ruby_init_loadpath();

	// export Bz remoting
	// TODO: split-view時、対象ドキュメントを取り違えるのでなんとかする。
	VALUE mBz = rb_define_module("BZ");
	rb_define_module_function(mBz, "caret", reinterpret_cast<VALUE(*)(...)>(bzruby_caret), 0);
	rb_define_module_function(mBz, "caret=", reinterpret_cast<VALUE(*)(...)>(bzruby_careteq), 1);
	rb_define_module_function(mBz, "[]", reinterpret_cast<VALUE(*)(...)>(bzruby_bracket), 1);
	rb_define_module_function(mBz, "[]=", reinterpret_cast<VALUE(*)(...)>(bzruby_bracketeq), 2);
	rb_define_module_function(mBz, "data", reinterpret_cast<VALUE(*)(...)>(bzruby_data), 0);
	rb_define_module_function(mBz, "data=", reinterpret_cast<VALUE(*)(...)>(bzruby_dataeq), 1);
	rb_define_module_function(mBz, "value", reinterpret_cast<VALUE(*)(...)>(bzruby_value), -1);
	rb_define_module_function(mBz, "setvalue", reinterpret_cast<VALUE(*)(...)>(bzruby_valueeq), -1);
	//rb_define_module_function(mBz, "fill", reinterpret_cast<VALUE(*)(...)>(NULL), 0); // TODO: 実装する?
	rb_define_module_function(mBz, "blockbegin", reinterpret_cast<VALUE(*)(...)>(bzruby_blockbegin), 0);
	rb_define_module_function(mBz, "blockend", reinterpret_cast<VALUE(*)(...)>(bzruby_blockend), 0);
	rb_define_module_function(mBz, "block", reinterpret_cast<VALUE(*)(...)>(bzruby_block), 0);
	rb_define_module_function(mBz, "setblock", reinterpret_cast<VALUE(*)(...)>(bzruby_setblock), -1);
	rb_define_module_function(mBz, "block=", reinterpret_cast<VALUE(*)(...)>(bzruby_setblock), -1); // BZ.block=start..end
	rb_define_module_function(mBz, "setmark", reinterpret_cast<VALUE(*)(...)>(bzruby_setmark), 1);
	rb_define_module_function(mBz, "unsetmark", reinterpret_cast<VALUE(*)(...)>(bzruby_unsetmark), 1);
	rb_define_module_function(mBz, "togglemark", reinterpret_cast<VALUE(*)(...)>(bzruby_togglemark), 1);
	rb_define_module_function(mBz, "ismarked", reinterpret_cast<VALUE(*)(...)>(bzruby_ismarked), 1);
	rb_define_module_function(mBz, "ismarked?", reinterpret_cast<VALUE(*)(...)>(bzruby_ismarked), 1);
#ifdef FILE_MAPPING
	rb_define_module_function(mBz, "isfilemapping", reinterpret_cast<VALUE(*)(...)>(bzruby_isfilemapping), 0);
	rb_define_module_function(mBz, "isfilemapping?", reinterpret_cast<VALUE(*)(...)>(bzruby_isfilemapping), 0);
#endif
	rb_define_module_function(mBz, "invalidate", reinterpret_cast<VALUE(*)(...)>(bzruby_invalidate), 0);
	rb_define_module_function(mBz, "endianess", reinterpret_cast<VALUE(*)(...)>(bzruby_endianess), 0);
	rb_define_module_function(mBz, "setendianess", reinterpret_cast<VALUE(*)(...)>(bzruby_setendianess), 1);
	rb_define_module_function(mBz, "endianess=", reinterpret_cast<VALUE(*)(...)>(bzruby_setendianess), 1);
	rb_define_module_function(mBz, "isle", reinterpret_cast<VALUE(*)(...)>(bzruby_isle), 0);
	rb_define_module_function(mBz, "isle?", reinterpret_cast<VALUE(*)(...)>(bzruby_isle), 0);
	rb_define_module_function(mBz, "isbe", reinterpret_cast<VALUE(*)(...)>(bzruby_isbe), 0);
	rb_define_module_function(mBz, "isbe?", reinterpret_cast<VALUE(*)(...)>(bzruby_isbe), 0);
	rb_define_module_function(mBz, "filename", reinterpret_cast<VALUE(*)(...)>(bzruby_filename), 0);
	rb_define_module_function(mBz, "size", reinterpret_cast<VALUE(*)(...)>(bzruby_size), 0);
	rb_define_module_function(mBz, "length", reinterpret_cast<VALUE(*)(...)>(bzruby_size), 0);
	rb_define_module_function(mBz, "each", reinterpret_cast<VALUE(*)(...)>(bzruby_each_byte), 0); // each_byteと同義
	rb_define_module_function(mBz, "each_byte", reinterpret_cast<VALUE(*)(...)>(bzruby_each_byte), 0);
	rb_define_module_function(mBz, "each_word", reinterpret_cast<VALUE(*)(...)>(bzruby_each_word), 0); // wordはWORDではなく、byte/word/dwordは現在のステータスバーに依存
	// とりあえずbyte版だけmap!/pmap!/bmap!を用意しておく。word版いる?
	//rb_define_module_function(mBz, "pmap!", reinterpret_cast<VALUE(*)(...)>(bzruby_pmapx), 0); // BZ.pmap!(begin..end|begin,end){block}
	//rb_define_module_function(mBz, "map!", reinterpret_cast<VALUE(*)(...)>(bzruby_mapx), 0);　// BZ.map!{block}; 0..BZ.sizeをpmap!に渡した感じ。
	//rb_define_module_function(mBz, "bmap!", reinterpret_cast<VALUE(*)(...)>(bzruby_bmapx), 0); // BZ.bmap!(){block}; 選択範囲をpmap!に渡した感じ、便利関数、選択していなければ何もしない
	// TODO: 適当にCBZView->m_nBytesのことをwideと呼称しているけど、いいのか?
	rb_define_module_function(mBz, "wide", reinterpret_cast<VALUE(*)(...)>(bzruby_wide), 0);
	rb_define_module_function(mBz, "wide=", reinterpret_cast<VALUE(*)(...)>(bzruby_wideeq), 1);
	//rb_define_module_function(mBz, "auto_invalidate", reinterpret_cast<VALUE(*)(...)>(bzruby_auto_invalidate), 0);
	//rb_define_module_function(mBz, "auto_invalidate=", reinterpret_cast<VALUE(*)(...)>(bzruby_auto_invalidateeq), 0);
	//rb_define_module_function(mBz, "setfilename", reinterpret_cast<VALUE(*)(...)>(bzruby_setfilename), 1);
	//rb_define_module_function(mBz, "open", reinterpret_cast<VALUE(*)(...)>(bzruby_open), 1);
	//rb_define_module_function(mBz, "save", reinterpret_cast<VALUE(*)(...)>(bzruby_save), 0);
	//rb_define_module_function(mBz, "saveas", reinterpret_cast<VALUE(*)(...)>(bzruby_saveas), 1);
	//rb_define_module_function(mBz, "new", reinterpret_cast<VALUE(*)(...)>(bzruby_new), 0);
	//rb_define_module_function(mBz, "", reinterpret_cast<VALUE(*)(...)>(bzruby_), 0);

	// register_hotkey
	// key-callback cursor-move-callback fileI/O-callback draw-callback(coloring)
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

	SetScrollSizes(MM_TEXT, CSize(0, 0));	
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
    //PyObject *pModule = PyImport_AddModule("__main__"); //create main module

	PyRun_SimpleString("import sys; import mywriter; sys.stdout=mywriter; sys.stderr=mywriter");

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

static VALUE bz_ruby_eval_toplevel(VALUE rcmdstr)
{
	VALUE binding;
	//binding = rb_funcall(rb_cObject, rb_intern("binding"), 0); // だめ
	binding = rb_const_get(rb_cObject, rb_intern("TOPLEVEL_BINDING")); // OK
	return rb_funcall(rb_mKernel, rb_intern("eval"), 2, rcmdstr, binding);
}

CString run_ruby(const char *cmdstr)
{
	CString ostr;

	VALUE value;
	int state;

	// rb_eval_string_protect()だとローカル変数が呼び出しごとにリセットされるので、
	// (少し面倒だけど)bindingが指定できるKernel#evalにObject::TOPLEVEL_BINDINGを渡してやる。
	//value = rb_eval_string_protect(cmdstr, &state);
	value = rb_protect(bz_ruby_eval_toplevel, rb_str_new_cstr(cmdstr), &state);

	if(state == 0)
	{
		//value = rb_obj_as_string(value);
		value = rb_funcall(value, rb_intern("inspect"), 0); // return value is String, so no need to call rb_obj_as_string()
		CStringA csvalue(RSTRING_PTR(value), RSTRING_LEN(value));
		CString pstr(csvalue);
		pstr.Replace(_T("\n"), _T("\r\n"));
		ostr.Format(_T("#=> %s\r\n"), pstr);
	} else
	{
		value = rb_errinfo();
		rb_set_errinfo(Qnil);
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
		//ostr.Format(_T("Error: state=%d\r\n%s\r\n"), state, pstr);
		ostr.Format(_T("%s\r\n"), pstr);
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
