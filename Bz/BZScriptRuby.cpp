#include "stdafx.h"
#include "Bz.h" // options
#include "BZView.h"
#include "BZDoc.h"
#include "BZScriptInterface.h"
#include "BZScriptView.h"
#include "BZScriptRuby.h"

//#define RUBY_EXPORT // dont dllimport
#define RUBY_DONT_SUBST // dont substitute snprintf etc.
#include "ruby.h"

static CBZScriptView *cbzsv; // TODO: YES GLOBAL AS RUBY DO!!!


extern "C" BZScriptInterface __declspec(dllexport) * getScriptInterface(void)
{
	return new BZScriptRuby();
}


BZScriptRuby::BZScriptRuby(void)
{
}


BZScriptRuby::~BZScriptRuby(void)
{
}


static VALUE bzruby_write(VALUE self, VALUE str)
{
	Check_Type(str, T_STRING);

	str = rb_obj_as_string(str);
	CString cstr(RSTRING_PTR(str), RSTRING_LEN(str));
	cstr.Replace(_T("\n"), _T("\r\n"));

	cbzsv->write(cstr);

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
	if(RTEST(rb_iv_get(self, "auto_invalidate")))
		cbzsv->m_pView->Invalidate();
	return orgcaret;
}

// [begin,end)
// TODO: 4GB越え対応
static VALUE bzruby_data2str(CBZDoc *doc, DWORD begin, DWORD end)
{
	DWORD remain = end - begin;
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
// begin>endやbegin,end<0、begin,end>docsizeなどのチェックはすでに済んでいるとする。
// TODO: 4GB越え対応
static VALUE bzruby_setdata(CBZView *view, DWORD begin, DWORD end, VALUE val)
{
	CBZDoc *doc = view->GetDocument();

	if(doc->m_bReadOnly)
	{
		rb_exc_raise(rb_exc_new2(rb_eRuntimeError, "can't modify read-only file"));
	}

	DWORD dstsize = end - begin; // TODO: 4GB越え対応

	const char *valptr = RSTRING_PTR(val);
	DWORD valsize = RSTRING_LEN(val); // TODO: 4GB越え対応

#ifdef FILE_MAPPING
	if(doc->IsFileMapping() && dstsize != valsize)
	{
		rb_exc_raise(rb_exc_new2(rb_eArgError, "can't modify with different size in file-mapping mode"));
	}
#endif

	if(dstsize == 0 && valsize == 0)
		return val;

	if(dstsize == 0) // 単に挿入される
	{
		doc->StoreUndo(begin, valsize, UNDO_DEL);
		doc->InsertData(begin, valsize, TRUE);
	} else if(dstsize < valsize) // 伸びる
	{
		// TODO: このundo操作、atomicの方がいい気がするが仕様的にできない…
		doc->StoreUndo(begin + dstsize, valsize - dstsize, UNDO_DEL);
		doc->StoreUndo(begin, dstsize, UNDO_OVR);
		doc->InsertData(begin + dstsize, valsize - dstsize, TRUE);
	} else if(dstsize == valsize) // 同じ長さ
	{
		doc->StoreUndo(begin, valsize, UNDO_OVR);
		//doc->InsertData(begin, valsize, FALSE); // 必要ないはず
	} else // valsize < docsize; 縮む
	{
		// TODO: このundo操作、atomicの方がいい気がするが仕様的にできない…
		doc->StoreUndo(begin + valsize, dstsize - valsize, UNDO_INS);
		doc->StoreUndo(begin, valsize, UNDO_OVR);
		doc->DeleteData(begin + valsize, dstsize - valsize);
	}

	DWORD endi = begin + valsize;
	for(DWORD i = begin; i < endi; )
	{
		LPBYTE pData = doc->QueryMapViewTama2(i, valsize);
		DWORD remain = doc->GetMapRemain(i);
		DWORD ovwsize = min(remain, valsize);
		memcpy(pData, valptr, ovwsize);
		i += ovwsize;
		valptr += ovwsize;
	}
	view->UpdateDocSize();

	return val; // おまけ: Qnilでも可
}
static VALUE bzruby_bracket(VALUE self, VALUE idx_range)
{
	LONGLONG ibegin, iend; // [ibegin,iend)

	if(TYPE(idx_range) == T_FIXNUM || TYPE(idx_range) == T_BIGNUM)
	{
		ibegin = NUM2LL(idx_range);
		iend = ibegin + 1;
	} else if(CLASS_OF(idx_range) == rb_cRange)
	{
		ibegin = NUM2LL(rb_funcall(idx_range, rb_intern("begin"),0));
		iend = NUM2LL(rb_funcall(idx_range, rb_intern("end"),0)) + (RTEST(rb_funcall(idx_range, rb_intern("exclude_end?"), 0)) ? 0 : 1);
	} else
	{
		rb_exc_raise(rb_exc_new2(rb_eArgError, "an argument must be Fixnum, Bignum or Range"));
	}

	CBZDoc *doc = cbzsv->m_pView->GetDocument();
	DWORD docsize = doc->GetDocSize(); // TODO: 4GB越え対応

	if(ibegin < 0) ibegin = (LONGLONG)docsize + ibegin;
	if(iend < 0) iend = (LONGLONG)docsize + iend + 1;

	if(iend - ibegin <= 0)
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
	if(iend < 0 || docsize < iend) // TODO: iend<0はありえないからチェック不要?
	{
		// Rangeだったらiendがファイルサイズを超えていた場合に切り詰める
		iend = docsize;
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
	LONGLONG iorgbegin, iorgend; // [ibegin,iend] (inclusive)
	if(TYPE(idx_range) == T_FIXNUM || TYPE(idx_range) == T_BIGNUM)
	{
		iorgbegin = NUM2LL(idx_range);
		iorgend = iorgbegin + 1;
	} else if(CLASS_OF(idx_range) == rb_cRange)
	{
		iorgbegin = NUM2LL(rb_funcall(idx_range, rb_intern("begin"),0));
		iorgend = NUM2LL(rb_funcall(idx_range, rb_intern("end"),0)) + (RTEST(rb_funcall(idx_range, rb_intern("exclude_end?"), 0)) ? 0 : 1);
	} else
	{
		rb_exc_raise(rb_exc_new2(rb_eArgError, "an argument must be Fixnum, Bignum or Range"));
	}
	Check_Type(val, T_STRING);

	CBZView *view = cbzsv->m_pView;
	CBZDoc *doc = view->GetDocument();

	DWORD docsize = doc->GetDocSize(); // TODO: 4GB越え対応
	LONGLONG ibegin = iorgbegin, iend = iorgend;

	if(ibegin < 0) ibegin = (LONGLONG)docsize + ibegin;
	if(iend < 0) iend = (LONGLONG)docsize + iend + 1;

	// Stringはこういう動作らしい…
	if(ibegin < 0 || docsize < ibegin)
	{
		CStringA buf;
		buf.Format("index %d out of data", iorgbegin);
		rb_exc_raise(rb_exc_new3(rb_eIndexError, rb_str_new_cstr(buf)));
	}

	if(iend - ibegin < 0)
	{
		iend = ibegin;
	}

	if(docsize < iend)
	{
		iend = docsize;
	}

	// do fetch and return it to Ruby world!
	// TODO: 4GB越え対応
	DWORD begin = static_cast<DWORD>(ibegin);
	DWORD end = static_cast<DWORD>(iend);
	
	return bzruby_setdata(view, begin, end, val);
}
static VALUE bzruby_data(VALUE self)
{
	CBZDoc *doc = cbzsv->m_pView->GetDocument();
	DWORD docsize = doc->GetDocSize(); // TODO: 4GB越え対応

	return bzruby_data2str(doc, 0, docsize);
}
static VALUE bzruby_dataeq(VALUE self, VALUE val)
{
	Check_Type(val, T_STRING);

	CBZView *view = cbzsv->m_pView;
	CBZDoc *doc = view->GetDocument();

	return bzruby_setdata(view, 0, doc->GetDocSize(), val);
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
	LONGLONG begin, end;

	VALUE v1, v2;
	int trueargc = rb_scan_args(argc, argv, "11", &v1, &v2);

	if(trueargc == 1)
	{
		if(CLASS_OF(v1) != rb_cRange)
			rb_exc_raise(rb_exc_new2(rb_eArgError, "arguments must be 2 fixnums or 1 range"));

		begin = NUM2LL(rb_funcall(v1, rb_intern("begin"),0));
		end = NUM2LL(rb_funcall(v1, rb_intern("end"),0)) + (RTEST(rb_funcall(v1, rb_intern("exclude_end?"), 0)) ? 0 : 1);
	} else if(trueargc == 2)
	{
		begin = NUM2LL(v1);
		end = NUM2LL(v2);
	} else
	{
		ASSERT(FALSE); // panic
	}

	CBZDoc *doc = cbzsv->m_pView->GetDocument();
	DWORD docsize = doc->GetDocSize(); // TODO: 4GB越え対応

	if(begin < 0)
		begin = docsize + begin;
	if(end < 0)
		end = docsize + end;

	if(begin < 0)
		begin = 0;
	if(docsize < begin)
		begin = docsize;
	if(end < 0)
		end = 0;
	if(docsize < end)
		end = docsize;

	cbzsv->m_pView->setBlock(static_cast<DWORD>(begin), static_cast<DWORD>(end)); // TODO: 4GB越え対応

	return Qnil; // TODO: rangeでも返す?
}
static VALUE bzruby_domark(VALUE self, VALUE voff, VALUE val)
{
	// TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
	ULONG off;

	off = NUM2ULONG(voff);

	VALUE ival = (val == Qtrue) ? Qfalse : Qtrue; // invert
	BOOL bival = (ival == Qtrue) ? TRUE : FALSE; // VALUE->BOOL
	VALUE org = val;

	if(cbzsv->m_pView->GetDocument()->CheckMark(off) == bival) // if CheckMark(off) != val
	{
		org = ival;
		cbzsv->m_pView->GetDocument()->SetMark(off); // toggle.

		if(RTEST(rb_iv_get(self, "auto_invalidate")))
			cbzsv->m_pView->Invalidate();
	}

	return org; // おまけ; valかQnilもあり
}
static VALUE bzruby_mark(int argc, VALUE *argv, VALUE self)
{
	VALUE voff;

	switch(rb_scan_args(argc, argv, "01", &voff))
	{
	case 0:
		voff = ULL2NUM((ULONGLONG)cbzsv->m_pView->m_dwCaret); // TODO: 4GB越え対応(ULONGLONGはずす)
		break;
	case 1:
		break;
	default:
		ASSERT(FALSE); // panic
	}

	return bzruby_domark(self, voff, Qtrue);
}
static VALUE bzruby_unmark(int argc, VALUE *argv, VALUE self)
{
	VALUE voff;

	switch(rb_scan_args(argc, argv, "01", &voff))
	{
	case 0:
		voff = ULL2NUM((ULONGLONG)cbzsv->m_pView->m_dwCaret); // TODO: 4GB越え対応(ULONGLONGはずす)
		break;
	case 1:
		break;
	default:
		ASSERT(FALSE); // panic
	}

	return bzruby_domark(self, voff, Qfalse);
}
static VALUE bzruby_setmark(int argc, VALUE *argv, VALUE self)
{
	VALUE v1, v2;
	VALUE voff, val;

	switch(rb_scan_args(argc, argv, "11", &v1, &v2))
	{
	case 1:
		voff = ULL2NUM((ULONGLONG)cbzsv->m_pView->m_dwCaret); // TODO: 4GB越え対応(ULONGLONGはずす)
		val = v1;
		break;
	case 2:
		voff = v1;
		val = v2;
		break;
	default:
		ASSERT(FALSE); // panic
	}
	return bzruby_domark(self, voff, val);
}
static VALUE bzruby_togglemark(VALUE self, VALUE voff)
{
	// TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
	ULONG off;

	off = NUM2ULONG(voff);

	VALUE old = cbzsv->m_pView->GetDocument()->CheckMark(off) ? Qtrue : Qfalse;
	cbzsv->m_pView->GetDocument()->SetMark(off);

	if(RTEST(rb_iv_get(self, "auto_invalidate")))
		cbzsv->m_pView->Invalidate();

	return old;
}
static VALUE bzruby_ismarked(VALUE self, VALUE voff)
{
	// TODO: 4GB越え対応(ULONGLONG/rb_big2ull)
	ULONG off;

	off = NUM2ULONG(voff);

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
	int val = FIX2INT(vval);
	if(!(val == 1 || val == 2 || val == 4 || val == 8))
	{
		rb_exc_raise(rb_exc_new2(rb_eArgError, "wide must be 1, 2, 4 or 8"));
	}
	VALUE orgwide = UINT2NUM(cbzsv->m_pView->m_nBytes); // おまけ
	cbzsv->m_pView->m_nBytes = val;
	return orgwide;
}
static VALUE bzruby_mapx(int argc, VALUE *argv, VALUE self)
{
	VALUE v1, v2;
	LONGLONG iorgbegin, iorgend; // TODO: 4GB越え対応

	switch(rb_scan_args(argc, argv, "02", &v1, &v2))
	{
	case 0:
		iorgbegin = 0;
		iorgend = cbzsv->m_pView->GetDocument()->GetDocSize();
		break;
	case 1:
		if(CLASS_OF(v1) != rb_cRange)
			rb_exc_raise(rb_exc_new2(rb_eArgError, "arguments must be none, a Range or two Fixnums"));

		iorgbegin = NUM2LL(rb_funcall(v1, rb_intern("begin"), 0));
		iorgend = NUM2LL(rb_funcall(v1, rb_intern("end"), 0)) + (RTEST(rb_funcall(v1, rb_intern("exclude_end?"), 0)) ? 0 : 1);
		break;
	case 2:
		iorgbegin = NUM2LL(v1);
		iorgend = NUM2LL(v2);
		break;
	default:
		ASSERT(FALSE); // panic
	}

	CBZDoc *doc = cbzsv->m_pView->GetDocument();
	DWORD docsize = doc->GetDocSize(); // TODO: 4GB越え対応
	LONGLONG ibegin = iorgbegin, iend = iorgend;

	// 範囲の扱いはset系に(BZ[]=のように)。
	if(ibegin < 0)
		ibegin = (LONGLONG)docsize + ibegin;
	if(iend < 0)
		iend = (LONGLONG)docsize + iend;

	if(docsize > 0 && (ibegin < 0 || docsize <= ibegin))
	{
		CStringA buf;
		buf.Format("index %d out of data", iorgbegin);
		rb_exc_raise(rb_exc_new3(rb_eIndexError, rb_str_new_cstr(buf)));
	}
	if(iend < ibegin)
		iend = ibegin;
	if(iend < 0)
		iend = ibegin;
	if(docsize < iend)
		iend = (LONGLONG)docsize;

	if(ibegin == iend)
		return Qnil;

	DWORD begin = static_cast<DWORD>(ibegin);
	DWORD end = static_cast<DWORD>(iend);

	// RETURN_ENUMERATOR_SIZEは引数があって使えない…
	for(DWORD i = begin; i < end; )
	{
		DWORD remain = end - i;
		LPBYTE pData = doc->QueryMapViewTama2(i, remain);
		DWORD remain_rd = min(remain, doc->GetMapRemain(i));
		for(DWORD j = 0; j < remain_rd; j++)
			pData[j] = (BYTE)NUM2CHR(rb_yield(INT2NUM(pData[j]))); // NUM2CHRはunsigned char(=BYTE)ではなくcharを返す
		i += remain_rd;
	}

	if(RTEST(rb_iv_get(self, "auto_invalidate")))
		cbzsv->m_pView->Invalidate();

	return Qnil;
}
static VALUE bzruby_bmapx(VALUE self)
{
	CBZView *view = cbzsv->m_pView;
	if(view->IsBlockAvailable())
	{
		VALUE argv[2] = {ULONG2NUM(view->BlockBegin()), ULONG2NUM(view->BlockEnd())};
		return bzruby_mapx(2, argv, self);
	}
	return Qnil;
}
static VALUE bzruby_auto_invalidate(VALUE self)
{
	return rb_iv_get(self, "auto_invalidate");
}
static VALUE bzruby_auto_invalidateeq(VALUE self, VALUE val)
{
	if(val != Qtrue && val != Qfalse)
	{
		rb_exc_raise(rb_exc_new2(rb_eArgError, "an argument must be true or false"));
	}
	VALUE old = rb_iv_get(self, "auto_invalidate");
	rb_iv_set(self, "auto_invalidate", val);
	return old;
}
static VALUE bzruby_undo(VALUE self)
{
	CBZView *view = cbzsv->m_pView;
	CBZDoc *doc = view->GetDocument();
	if(doc->CanUndo())
	{
		doc->DoUndo();
		view->GotoCaret();
		view->UpdateDocSize();
		return Qtrue;
	} else
	{
		return Qfalse;
	}
}
static VALUE bzruby_b(VALUE self)
{
	CBZView *view = cbzsv->m_pView;
	CBZDoc *doc = view->GetDocument();
	if(view->IsBlockAvailable())
	{
		return bzruby_data2str(doc, view->BlockBegin(), view->BlockEnd());
	} else
	{
		return Qnil;
	}
}
static VALUE bzruby_clip(VALUE self)
{
	HGLOBAL hMem;
	LPBYTE pMem, pWorkMem;

	DWORD dwSize;
	VALUE ret;

	CBZDoc *doc = cbzsv->m_pView->GetDocument();

	dwSize = doc->ClipboardReadOpen(hMem, pMem, pWorkMem);
	ret = rb_str_new((const char *)pMem, dwSize);
	doc->ClipboardReadClose(hMem, pMem, pWorkMem);

	return ret;
}
static VALUE bzruby_clipeq(VALUE self, VALUE val)
{
	Check_Type(val, T_STRING);
	cbzsv->m_pView->GetDocument()->DoCopyToClipboard((LPBYTE)RSTRING_PTR(val), RSTRING_LEN(val), FALSE);
	return val;
}

BOOL BZScriptRuby::init(CBZScriptView *sview)
{
	static int initialized = 0;
	if(initialized) return TRUE; else initialized = 1;

	int argc = 0;
	char **argv = NULL;
	ruby_sysinit(&argc, &argv); // Important to avoid SEGV on ruby_init/ruby_init_loadpath!
	// RUBY_INIT_STACK; // Win32では空なので呼ばなくてもOKのはず…
	ruby_init(); // ruby_init WORK ONLY ONCE.
	ruby_options(0, NULL);

	// remap stdio
	rb_stdout = rb_obj_alloc(rb_cIO);
	rb_define_singleton_method(rb_stdout, "write", reinterpret_cast<VALUE(*)(...)>(bzruby_write), 1);
	rb_stderr = rb_obj_alloc(rb_cIO);
	rb_define_singleton_method(rb_stderr, "write", reinterpret_cast<VALUE(*)(...)>(bzruby_write), 1);
	rb_stdin = rb_obj_alloc(rb_cIO);
	rb_define_singleton_method(rb_stdin, "read", reinterpret_cast<VALUE(*)(...)>(bzruby_read), -1);

	cbzsv = sview;

	ruby_init_loadpath();

	// export Bz remoting
	// TODO: split-view時、対象ドキュメントを取り違えるのでなんとかする。
	// TODO: 引数チェックの仕方がバラバラなので統一する。
	//       基本的にアドレスはNUM2LLでlonglongにする、NUM2xxを使っている場合はCheck_Typeなどしない(変換エラー例外に任せる)、begin...endは[begin,end)にする。
	VALUE mBz = rb_define_module("BZ");
	rb_define_module_function(mBz, "caret", reinterpret_cast<VALUE(*)(...)>(bzruby_caret), 0);
	rb_define_module_function(mBz, "caret=", reinterpret_cast<VALUE(*)(...)>(bzruby_careteq), 1);
	rb_define_module_function(mBz, "[]", reinterpret_cast<VALUE(*)(...)>(bzruby_bracket), 1);
	rb_define_module_function(mBz, "[]=", reinterpret_cast<VALUE(*)(...)>(bzruby_bracketeq), 2);
	rb_define_module_function(mBz, "data", reinterpret_cast<VALUE(*)(...)>(bzruby_data), 0);
	rb_define_module_function(mBz, "data=", reinterpret_cast<VALUE(*)(...)>(bzruby_dataeq), 1);
	rb_define_module_function(mBz, "value", reinterpret_cast<VALUE(*)(...)>(bzruby_value), -1);
	rb_define_module_function(mBz, "setvalue", reinterpret_cast<VALUE(*)(...)>(bzruby_valueeq), -1);
	rb_define_module_function(mBz, "value=", reinterpret_cast<VALUE(*)(...)>(bzruby_valueeq), 1); // 使われるか微妙だ…
	//rb_define_module_function(mBz, "fill", reinterpret_cast<VALUE(*)(...)>(NULL), 0); // TODO: 実装する?
	rb_define_module_function(mBz, "blockbegin", reinterpret_cast<VALUE(*)(...)>(bzruby_blockbegin), 0);
	rb_define_module_function(mBz, "blockend", reinterpret_cast<VALUE(*)(...)>(bzruby_blockend), 0);
	rb_define_module_function(mBz, "block", reinterpret_cast<VALUE(*)(...)>(bzruby_block), 0);
	rb_define_module_function(mBz, "setblock", reinterpret_cast<VALUE(*)(...)>(bzruby_setblock), -1);
	rb_define_module_function(mBz, "block=", reinterpret_cast<VALUE(*)(...)>(bzruby_setblock), -1); // BZ.block=start..end
	rb_define_module_function(mBz, "mark", reinterpret_cast<VALUE(*)(...)>(bzruby_mark), -1);
	rb_define_module_function(mBz, "unmark", reinterpret_cast<VALUE(*)(...)>(bzruby_unmark), -1);
	rb_define_module_function(mBz, "setmark", reinterpret_cast<VALUE(*)(...)>(bzruby_setmark), -1);
	rb_define_module_function(mBz, "togglemark", reinterpret_cast<VALUE(*)(...)>(bzruby_togglemark), 1);
	rb_define_module_function(mBz, "ismarked", reinterpret_cast<VALUE(*)(...)>(bzruby_ismarked), 1);
	rb_define_module_function(mBz, "ismarked?", reinterpret_cast<VALUE(*)(...)>(bzruby_ismarked), 1);
#ifdef FILE_MAPPING
	rb_define_module_function(mBz, "isfilemapping", reinterpret_cast<VALUE(*)(...)>(bzruby_isfilemapping), 0);
	rb_define_module_function(mBz, "isfilemapping?", reinterpret_cast<VALUE(*)(...)>(bzruby_isfilemapping), 0);
#endif
	rb_define_module_function(mBz, "invalidate", reinterpret_cast<VALUE(*)(...)>(bzruby_invalidate), 0);
	rb_define_module_function(mBz, "endianess", reinterpret_cast<VALUE(*)(...)>(bzruby_endianess), 0);
	// TODO: endianess変えた時、invalidateしてもステータスバーが変わらないのをなんとかする。
	// TODO: ついでにここに; フォーカスがScriptViewにある場合、ステータスバーをクリックしても表示が変わらないのもなんとかする。
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
	rb_define_module_function(mBz, "map!", reinterpret_cast<VALUE(*)(...)>(bzruby_mapx), -1); // BZ.pmap!(|begin..end|begin,end){block}; 引数省略時は全体が対象
	rb_define_module_function(mBz, "bmap!", reinterpret_cast<VALUE(*)(...)>(bzruby_bmapx), 0); // BZ.bmap!(){block}; 選択範囲をpmap!に渡した感じ、便利関数、選択していなければ何もしない
	// TODO: 適当にCBZView->m_nBytesのことをwideと呼称しているけど、いいのか?
	rb_define_module_function(mBz, "wide", reinterpret_cast<VALUE(*)(...)>(bzruby_wide), 0);
	rb_define_module_function(mBz, "wide=", reinterpret_cast<VALUE(*)(...)>(bzruby_wideeq), 1);
	rb_define_module_function(mBz, "auto_invalidate", reinterpret_cast<VALUE(*)(...)>(bzruby_auto_invalidate), 0);
	rb_define_module_function(mBz, "auto_invalidate=", reinterpret_cast<VALUE(*)(...)>(bzruby_auto_invalidateeq), 0);
	rb_define_module_function(mBz, "undo", reinterpret_cast<VALUE(*)(...)>(bzruby_undo), 0);
	rb_define_module_function(mBz, "b", reinterpret_cast<VALUE(*)(...)>(bzruby_b), 0);
	rb_define_module_function(mBz, "clip", reinterpret_cast<VALUE(*)(...)>(bzruby_clip), 0);
	rb_define_module_function(mBz, "clip=", reinterpret_cast<VALUE(*)(...)>(bzruby_clipeq), 1);
	rb_define_module_function(mBz, "setclip", reinterpret_cast<VALUE(*)(...)>(bzruby_clipeq), 1);
	//rb_define_module_function(mBz, "setfilename", reinterpret_cast<VALUE(*)(...)>(bzruby_setfilename), 1);
	//rb_define_module_function(mBz, "open", reinterpret_cast<VALUE(*)(...)>(bzruby_open), 1);
	//rb_define_module_function(mBz, "save", reinterpret_cast<VALUE(*)(...)>(bzruby_save), 0);
	//rb_define_module_function(mBz, "saveas", reinterpret_cast<VALUE(*)(...)>(bzruby_saveas), 1);
	//rb_define_module_function(mBz, "new", reinterpret_cast<VALUE(*)(...)>(bzruby_new), 0);
	//rb_define_module_function(mBz, "", reinterpret_cast<VALUE(*)(...)>(bzruby_), 0);
	rb_iv_set(mBz, "auto_invalidate", Qtrue);

	// register_hotkey
	// key-callback cursor-move-callback fileI/O-callback draw-callback(coloring)

	return TRUE;
}


void BZScriptRuby::cleanup(CBZScriptView *sview)
{
	//ruby_cleanup(0); // DO NOT CALL, ruby_init DO WORK ONLY ONCE!
}


void BZScriptRuby::onClear(CBZScriptView* sview)
{
	VALUE value;
	int state;

	cbzsv = sview;
	value = rb_eval_string_protect("puts RUBY_DESCRIPTION", &state);
}


static VALUE bz_ruby_eval_toplevel(VALUE rcmdstr)
{
	VALUE binding;
	//binding = rb_funcall(rb_cObject, rb_intern("binding"), 0); // だめ
	binding = rb_const_get(rb_cObject, rb_intern("TOPLEVEL_BINDING")); // OK
	return rb_funcall(rb_mKernel, rb_intern("eval"), 2, rcmdstr, binding);
}

CString BZScriptRuby::run(CBZScriptView* sview, const char * cmdstr)
{
	CString ostr;

	VALUE value;
	int state;

	// rb_eval_string_protect()だとローカル変数が呼び出しごとにリセットされるので、
	// (少し面倒だけど)bindingが指定できるKernel#evalにObject::TOPLEVEL_BINDINGを渡してやる。
	//value = rb_eval_string_protect(cmdstr, &state);
	cbzsv = sview;
	value = rb_protect(bz_ruby_eval_toplevel, rb_str_new_cstr(cmdstr), &state);

	if(state == 0)
	{
		//value = rb_obj_as_string(value);
		value = rb_funcall(value, rb_intern("inspect"), 0); // return value is String, so no need to call rb_obj_as_string()
		CStringA csvalue(RSTRING_PTR(value), RSTRING_LEN(value));
		CString pstr(csvalue);
		pstr.Replace(_T("\n"), _T("\r\n"));
		ostr.Format(_T("=> %s\r\n"), pstr);
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


CString BZScriptRuby::name(void)
{
	return CString(_T("Ruby"));
}
