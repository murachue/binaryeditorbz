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
#include "BZFormVw.h"
#include "BZInspectView.h"
#include "BZAnalyzerView.h"
#include "MainFrm.h"
#include "ColorDlg.h"
#include "SettingDlg.h"
#include "PasteType.h"
#include  <ctype.h>
#include  <winnls.h>
#include  <mbstring.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define ADDRCOLUMNS 6
#define ADDRCOLUMNS_MAP 9
#define DUMP_X		(m_nColAddr + 2)  // 8
#define DUMP_Y		1
#define CHAR_X		(m_nColAddr + 51) // 57
#define VIEWCOLUMNS	(m_nColAddr + 68) // 74
#define PAGESKIP	16

#define TIMER_DOWN	1
#define TIMER_UP	2

#define CHAR_NG		'.'
#define CHAR_EBCDIC_NG 0xFF
#define EBCDIC_BASE 0x40
#define EBCDIC_COUNT (256 - EBCDIC_BASE)

#define MEMFILE_GROWBY 16384

//static const UINT nMsgFindReplace = ::RegisterWindowMessage(FINDMSGSTRING);

static DWORD  MemCompByte2(LPCVOID p1, LPCVOID p2, DWORD len);
static DWORD  MemScanByteNeg(const BYTE *p, BYTE c, DWORD len);

BOOL CBZView::m_bHexSize = FALSE;
LPSTR CBZView::m_pEbcDic = NULL;
BOOL  CBZView::m_bLoadEbcDic = FALSE;

/////////////////////////////////////////////////////////////////////////////
// CBZView

IMPLEMENT_DYNCREATE(CBZView, CTextView)

BEGIN_MESSAGE_MAP(CBZView, CTextView)
	//{{AFX_MSG_MAP(CBZView)
	ON_WM_CREATE()
	ON_COMMAND(ID_VIEW_FONT, OnViewFont)
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_COMMAND(ID_JUMP_OFFSET, OnJumpOffset)
	ON_COMMAND(ID_JUMP_RETURN, OnJumpReturn)
	ON_WM_KEYDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_TIMER()
	ON_COMMAND(ID_JUMP_FINDNEXT, OnJumpFindnext)
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
	ON_WM_CHAR()
	ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
	ON_COMMAND(ID_JUMP_COMPARE, OnJumpCompare)
	ON_UPDATE_COMMAND_UI(ID_JUMP_COMPARE, OnUpdateJumpCompare)
	ON_UPDATE_COMMAND_UI(ID_EDIT_VALUE, OnUpdateEditValue)
	ON_COMMAND(ID_CHAR_AUTODETECT, OnCharAutoDetect)
	ON_UPDATE_COMMAND_UI(ID_CHAR_AUTODETECT, OnUpdateCharAutoDetect)
	ON_COMMAND(ID_VIEW_COLOR, OnViewColor)
	ON_WM_ERASEBKGND()
	ON_COMMAND(ID_JUMP_START, OnJumpStart)
	ON_COMMAND(ID_JUMP_END, OnJumpEnd)
	ON_WM_MBUTTONDOWN()
	ON_COMMAND(ID_EDIT_SELECT_ALL, OnEditSelectAll)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SELECT_ALL, OnUpdateEditSelectAll)
	ON_COMMAND(ID_EDIT_COPY_DUMP, OnEditCopyDump)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY_DUMP, OnUpdateEditCopyDump)
	ON_COMMAND(ID_EDIT_COPY_HEXSTRING, OnEditCopyHexstring)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY_HEXSTRING, OnUpdateEditCopyHexstring)
	ON_COMMAND(ID_EDIT_PASTE_HEXSTRING, OnEditPasteHexstring)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE_HEXSTRING, OnUpdateEditPasteHexstring)
	ON_COMMAND(ID_PASTETYPE, OnPasteType)
	ON_UPDATE_COMMAND_UI(ID_PASTETYPE, OnUpdatePasteType)
	ON_COMMAND(ID_JUMP_BASE, OnJumpBase)
	ON_COMMAND(ID_JUMP_MARK, SetMark)
	ON_COMMAND(ID_JUMP_MARKNEXT, JumpToMark)
	ON_WM_KEYUP()
	//}}AFX_MSG_MAP
	ON_COMMAND_RANGE(ID_CHAR_ASCII, ID_CHAR_LAST, OnCharMode)
	ON_UPDATE_COMMAND_UI_RANGE(ID_CHAR_ASCII, ID_CHAR_LAST, OnUpdateCharMode)

	ON_UPDATE_COMMAND_UI(ID_INDICATOR_INFO, OnUpdateStatusInfo)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_SIZE, OnUpdateStatusSize)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_CHAR, OnUpdateStatusChar)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_INS, OnUpdateStatusIns)
	ON_COMMAND(ID_INDICATOR_INFO, OnStatusInfo)
	ON_COMMAND(ID_INDICATOR_SIZE, OnStatusSize)
	ON_COMMAND(ID_INDICATOR_CHAR, OnStatusChar)

	ON_COMMAND_RANGE(ID_BYTEORDER_INTEL, ID_BYTEORDER_68K, OnByteOrder)
	ON_UPDATE_COMMAND_UI_RANGE(ID_BYTEORDER_INTEL, ID_BYTEORDER_68K, OnUpdateByteOrder)
	ON_UPDATE_COMMAND_UI_RANGE(ID_JUMP_COMPARE, ID_JUMP_END, OnUpdateJump)

	// Special registered message for Find and Replace
//	ON_REGISTERED_MESSAGE(nMsgFindReplace, OnFindNext)
ON_WM_VSCROLL()
ON_WM_MOUSEWHEEL()
ON_COMMAND(ID_VIEW_GRID1, &CBZView::OnViewGrid1)
ON_UPDATE_COMMAND_UI(ID_VIEW_GRID1, &CBZView::OnUpdateViewGrid1)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBZView construction/destruction

CBZView::CBZView()
{
	// TODO: add construction code here
	m_dwTotal = 0;
	m_timer = 0;
	m_nPageLen = 0;		// ### 1.54
	m_nBytesLength = 1;
}

CBZView::~CBZView()
{
	if(m_pEbcDic) {
		delete[] m_pEbcDic;
		m_pEbcDic = NULL;
		m_bLoadEbcDic = FALSE;
	}
}

int CBZView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	m_nColAddr = ADDRCOLUMNS;
	SetViewSize(CSize(VIEWCOLUMNS, 0));

	if (CTextView::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	// TODO: Add your specialized creation code here
//	uCharmode = AfxGetApp()->GetProfileInt("Option", "CharSet", 0) + CTYPE_ASCII;
	return 0;
}

void CBZView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	ATLTRACE("CBZView::OnUpdate()\n");
	
	m_pDoc = GetDocument();
	ASSERT_VALID(m_pDoc);
#ifdef FILE_MAPPING
	if(m_pDoc)
		m_pDoc->QueryMapViewTama2(0, 1);//ファイルサイズが０の場合で、デフォルト設定ではない場合、ファイルマッピングモードで開くとエラー
	m_nColAddr = (options.bDWordAddr || m_pDoc->IsFileMapping()) ? ADDRCOLUMNS_MAP : ADDRCOLUMNS;
	if(GetViewWidth() != VIEWCOLUMNS) {
		SetViewSize(CSize(VIEWCOLUMNS, 0));
		m_bResize = FALSE;
		// ResizeFrame();
	}
#else
	m_nColAddr = ADDRCOLUMNS;
#endif //FILE_MAPPING

	UpdateDocSize();
	ScrollToPos(CPoint(0,0));
	SetScrollHome(CPoint(0, DUMP_Y));
	m_dwCaret = m_dwOldCaret = m_dwBlock = m_dwStruct = 0;
	m_nMember = INVALID;
	m_nBytes = 1;
	m_bCaretOnChar = m_bBlock = m_bIns = FALSE;
	m_bEnterVal = FALSE;
	m_charset = (options.bAutoDetect) ? AutoDetectCharSet() : options.charset;
	if(m_charset == CTYPE_EBCDIC)
		LoadEbcDicTable();

	CMainFrame *pMainFrame = GetMainFrame();
	if(pMainFrame)
	{
		if(pMainFrame->m_bStructView) {
		CBZFormView* pView = (CBZFormView*)GetNextWindow(GW_HWNDPREV);
		if(pView!=NULL)
			pView->SelectTag();
	}
		if(pMainFrame->m_bAnalyzerView) {
		CBZAnalyzerView* pView = (CBZAnalyzerView*)GetNextWindow(GW_HWNDPREV);
		if(pView!=NULL)
			pView->Clear();
	}
	}
	Invalidate();
	InitCaret();
	DrawCaret();
}

/////////////////////////////////////////////////////////////////////////////
// CBZView diagnostics

#ifdef _DEBUG
void CBZView::AssertValid() const
{
	CTextView::AssertValid();
}

void CBZView::Dump(CDumpContext& dc) const
{
	CTextView::Dump(dc);
}

CBZDoc* CBZView::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CBZDoc)));
	return (CBZDoc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CBZView drawing

BOOL CBZView::OnPreparePrinting(CPrintInfo* pInfo) 
{
	// TODO: call DoPreparePrinting to invoke the Print dialog box
	if(m_nPageLen)
		SetMaxPrintingPage(pInfo);
	return CTextView::OnPreparePrinting(pInfo);
}

void CBZView::OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo)
{
	CTextView::OnBeginPrinting(pDC, pInfo);

	CRect rMargin;
	GetMargin(rMargin, pDC);
	POINT pt;
	pt.x = pDC->GetDeviceCaps(HORZRES) - rMargin.left - rMargin.right;
	pt.y = pDC->GetDeviceCaps(VERTRES) - rMargin.top - rMargin.bottom;
	PixelToGrid(pt);
	m_nPageLen = pt.y-1;
	SetMaxPrintingPage(pInfo);
}

void CBZView::SetMaxPrintingPage(CPrintInfo* pInfo)
{
	DWORD dwSize = (m_bBlock) ? BlockEnd()-(BlockBegin()&~(16-1)) : m_dwTotal;
	pInfo->SetMaxPage(dwSize/16/ m_nPageLen + 1);
}

void CBZView::OnPrint(CDC* pDC, CPrintInfo* pInfo) 
{
	// TODO: Add your specialized code here and/or call the base class
	m_dwPrint = (m_bBlock) ? (BlockBegin()&~(16-1)) : 0;
	m_dwPrint += (pInfo->m_nCurPage - 1) * m_nPageLen * 16;
	CView::OnPrint(pDC, pInfo);
}

void CBZView::SetColor(TextColor n)
{
	CTextView::SetColor(options.colors[n][0], options.colors[n][1]);
}

void CBZView::SetHeaderColor()
{
	CMainFrame *mainframe = GetMainFrame();
	BOOL bNoFocus = FALSE;
	if(mainframe) {
		bNoFocus = mainframe->m_nSplitView && mainframe->GetActiveDocument() != m_pDoc;
	}
	SetColor(bNoFocus ? TCOLOR_ADDRESS2 : TCOLOR_ADDRESS);
}

BOOL CBZView::OnEraseBkgnd(CDC* pDC) 
{
	// TODO: Add your message handler code here and/or call default

	COLORREF rgbBG = options.colors[TCOLOR_TEXT][1];
	if(!IsSystemColor(rgbBG)) {
		CBrush brushBG(rgbBG);
		CRect rcErase;
		pDC->GetClipBox(rcErase);
   		pDC->FillRect(rcErase, &brushBG);
		return TRUE;
	}
	return CTextView::OnEraseBkgnd(pDC);
}

/////////////////////////////////////////////////////////////////////////////
// CBZView OnDraw

void CBZView::DrawToFile(CFile* pFile) 	// ###1.63
{
	m_pFile = pFile;
	OnDraw(NULL);
	m_pFile = NULL;
}

void CBZView::OnDraw(CDC* pDC)
{
	if(pDC)
		CTextView::OnDraw(pDC);

	// TODO: add draw code for native data here
	LPBYTE p  = m_pDoc->GetDocPtr();
//	if(!p) return;

	RECT rClip;
	DWORD ofs;
	DWORD dwBegin = BlockBegin();
	DWORD dwEnd   = BlockEnd();
	DWORD dwTotal = m_dwTotal;

	DWORD dwTotalP1 = 0;

	int y;

	PutBegin(pDC);
	if(IsToFile()) {
		DrawHeader();
		rClip.y1 = DUMP_Y;
		rClip.y2 = LONG_MAX;
		ofs = 0;
		if(m_bBlock) {
			ofs = dwBegin & ~0xF;
			dwTotal = dwEnd;
		}
	} else if(pDC->IsPrinting()) {		// ### 1.54
		DrawHeader();
		rClip.y1 = DUMP_Y;
		rClip.y2 = m_nPageLen;
		ofs = m_dwPrint;
	} else {
		pDC->GetClipBox(&rClip);
		PixelToGrid(rClip);
		if(rClip.y1 < DUMP_Y) {
			DrawHeader();
			rClip.y1 += DUMP_Y;
		}
		POINT ptOrg = GetScrollPos();
		ofs = (ptOrg.y + rClip.y1 - DUMP_Y)*16;
	}
	if(!IsToFile() && (pDC->IsPrinting() && m_bBlock))
		dwTotal = dwEnd;
	LPBYTE p1 = NULL;
	CBZView* pView1 = GetBrotherView();
	if(pView1 /*&& (m_dwTotal == pView1->m_dwTotal)*/)
	{
		p1 = pView1->m_pDoc->GetDocPtr();
		dwTotalP1 = pView1->m_dwTotal;
	}

#ifdef FILE_MAPPING
		if(p && !(p = m_pDoc->QueryMapView(p, ofs))) return;
#endif //FILE_MAPPING
	InitCharMode(p, ofs);

	for(/*int */y = rClip.y1; y <= rClip.y2; y++) {
		Locate(0, y);
		if(ofs > dwTotal) break;
		if(IsToFile() && options.nDumpPage) {
			if(y > DUMP_Y && ((y - DUMP_Y) % options.nDumpPage) == 0) {
				Locate(0, y); // crlf
				DrawHeader();
				Locate(0, y); // crlf
			}
		}
		SetHeaderColor();
		DWORD ofs1 = ofs + m_pDoc->m_dwBase;
#ifdef FILE_MAPPING
		if(p && !(p = m_pDoc->QueryMapView(p, ofs1))) return;
		if(p1 && !(p1 = pView1->m_pDoc->QueryMapView(p1, ofs1))) return;

		if(m_nColAddr > ADDRCOLUMNS) {
			PutFormatStr("%04X:%04X", HIWORD(ofs1), LOWORD(ofs1));
			SetColor();
			PutStr("  ");
		} else
#endif //FILE_MAPPING
		{
			if(ofs1 < 0x1000000) {			// ###1.54
				PutFormatStr("%06X", ofs1);
				SetColor();
				PutStr("  ");
			} else {
				PutFormatStr("%07X", ofs1);
				SetColor();
				PutStr(" ");
			}
		}
		DWORD ofs0 = ofs;
		for_to(i,16) {
			if(ofs >= dwTotal) {
				SetColor();
				PutChar(' ', (16-i)*3-1); 
				break;
			}
			if(m_bBlock && ofs >= dwBegin && ofs < dwEnd)
				SetColor(TCOLOR_SELECT);
			else if(m_pDoc->CheckMark(ofs))
				SetColor(TCOLOR_MARK);
			else if(m_dwStruct && ofs >= m_dwStructTag && ofs < m_dwStruct) {
				if(m_nMember >= 0 && ofs >= m_dwStructTag + m_nMember && ofs < m_dwStructTag + m_nMember + m_nBytes * m_nBytesLength)
					SetColor(TCOLOR_MEMBER);
				else
					SetColor(TCOLOR_STRUCT);
			}
			else if(p1 && (ofs < dwTotalP1 && (*(p+ofs) != *(p1+ofs))))
				SetColor(TCOLOR_MISMATCH);
			else if(p1 && ofs >= dwTotalP1)
				SetColor(TCOLOR_OVERBROTHER);
			else
				SetColor();
			PutFormatStr("%02X", *(p + ofs++));
			if(i < 15)
				PutChar((i==7) ? '-' : ' ');
		}
		SetColor();
		PutStr("  ");
		ofs = ofs0;

#if 0//#ifdef _UNICODE
		wchar_t wcConv;
		char chConv[3]={0};
		bool fPutSkip = FALSE;
#endif

		for_to_(i,16) {
			BOOL wascolorset = TRUE;

			if(ofs >= dwTotal) {
				SetColor();
				PutChar(' ', 16-i);
				ofs++;
				break;
			}
			if(m_bBlock && ofs >= dwBegin && ofs < dwEnd)
				SetColor(TCOLOR_SELECT);
			else if(m_dwStruct && ofs >= m_dwStructTag && ofs < m_dwStruct) {	// ###1.62
				if(m_nMember >= 0 && ofs >= m_dwStructTag + m_nMember && ofs < m_dwStructTag + m_nMember + m_nBytes * m_nBytesLength)
					SetColor(TCOLOR_MEMBER);
				else
					SetColor(TCOLOR_STRUCT);
			}
			else {
				wascolorset = FALSE;
				SetColor();
			}

			if(m_charset == CTYPE_UNICODE) {
				WORD w = SwapWord(*((WORD*)(p+ofs)));	// ### 1.54a
				PutUnicodeChar(w, wascolorset);
				ofs += 2;
				i++;
			} else {
				WORD c = *(p + ofs++);//注意: WORDだけどpがLPBYTEなので1バイトしか格納されていない
				WORD orgc = c;
				switch(m_charset) {
				case CTYPE_ASCII:
					if(c < 0x20 || c > 0x7E) c = CHAR_NG;
					break;
				case CTYPE_SJIS:
				{
					if(c < 0x20) c = CHAR_NG;
					else if(i == 0 && IsMBS(p, ofs-1, TRUE))
						 c=' ';
					else if(IsMBS(p, ofs-1, FALSE)) {
						BYTE c1 = *(p + ofs);
						if(_ismbclegal(MAKEWORD(c1, c))) {
							PutChar((char)c);
							c = *(p + ofs);
							if(i < 15) {
								ofs++;
								i++;
							}
						} else
							c = CHAR_NG;
					} else if((c > 0x7E && c < 0xA1) || c > 0xDF)
						c = CHAR_NG;
					break;
				}
				case CTYPE_JIS:
				case CTYPE_EUC:
				case CTYPE_EPWING:
					c = GetCharCode(c, ofs - 1);
					if(c < 0x20 || (c > 0x7E && c < 0xA1) || (c > 0xDF && c <= 0xFF)) c = CHAR_NG;
					else if(c & 0xFF00) {
						BYTE c1 = *(p + ofs);
						c = (c | c1) & 0x7F7F;
						if(i < 15) {
							GetCharCode(c1, ofs);
							ofs++;
							i++;
						}
						c = _mbcjistojms(c);
						if(!c) c = '..';
#if 1//#ifndef _UNICODE
						PutChar(HIBYTE(c));//!""""""""!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
						c &= 0xFF;
#else
						chConv[0] = HIBYTE(c);
						chConv[1] = c&0xFF;
						chConv[2] = NULL;
						PutStr(CA2WEX<4>(chConv));
						fPutSkip = TRUE;
#endif
					}
					break;
				case CTYPE_UTF8:	// ### 1.54b
					if(c < 0x20) c = CHAR_NG;
					else if(c & 0x80) {
						if(!(c & 0x40)) c = CHAR_NG;
						else {
							WORD w = 0;
							if(!(c & 0x20)) {	// U+0080..U+07FF 00000xxx:xxyyyyyy 110xxxxx 10yyyyyy
								if(ofs + 1 >= dwTotal) c = CHAR_NG;
								else {
									if(((*(p + ofs))&0xC0/*11000000*/) != 0x80/*10000000*/)
									{
										c = CHAR_NG;
									} else {
									w = ((c & 0x1F) << 6) | ((*(p + ofs)) & 0x3F);
									if(++i < 16) ofs++;
								}
								}
							} else  {			// U+0800..U+FFFF xxxxyyyy:yyzzzzzz 1110xxxx 10yyyyyy 10zzzzzz 
								if(ofs + 2 >= dwTotal) c = CHAR_NG;
								else {
									if(((*(p + ofs))&0xC0) != 0x80 || ((*(p + ofs + 1))&0xC0) != 0x80)
									{
										c = CHAR_NG;
									} else {
									w = ((c & 0x0F) << 12) | (((*(p + ofs)) & 0x3F) << 6) | ((*(p + ofs + 1)) & 0x3F);
									if(++i < 16) ofs++;
									if(++i < 16) ofs++;
								}
							}
							}
							if(w) {
								PutUnicodeChar(w, wascolorset);
								if(w & ~0x07FF && i < 16) {
									c = CHAR_NG;
								} else
									continue;
							}
						}
					}
					break;
				case CTYPE_EBCDIC:	// ### 1.63
					if(m_pEbcDic == NULL || c < EBCDIC_BASE)
						c = CHAR_NG;
					else
						c = *(m_pEbcDic + ((BYTE)c - EBCDIC_BASE));
					break;
				}

				if(!wascolorset) {
					if(orgc == 0) {
						SetColor(TCOLOR_TEXTNULL);
					} else if(orgc != CHAR_NG && c == CHAR_NG) {
						SetColor(TCOLOR_CHARNG);
					}
				}

#if 0//#ifdef _UNICODE
				if(fPutSkip)
				{
					fPutSkip=FALSE;
				} else {
					chConv[0] = (char)c;
					chConv[1] = chConv[2] = NULL;
					PutStr(CA2WEX<4>(chConv));
				}
#else
				PutChar((char)c);
#endif
			}
		}
		SetColor();
//		if(m_charset == CTYPE_SJIS)
			PutChar(' ');
	}
	if(IsToFile()) {
		PutEnd();
	} else {
		if(y <= rClip.y2) {
			SetColor();
			for(y; y<=rClip.y2+1; y++) {
				Locate(0, y);
				PutChar(' ', VIEWCOLUMNS);
			}
		}
		PutEnd();
		DrawDummyCaret(pDC);
		DrawCaret();
	}

	/* Grid表示 */
	if(!IsToFile() && options.iGrid==1) {
		DrawGrid(pDC, rClip);
	}
}

void CBZView::DrawGrid(CDC* pDC, RECT& rClip)
{
	int OldBkMode = pDC->GetBkMode();
	pDC->SetBkMode(TRANSPARENT);
	CPen redSolid(PS_SOLID, 1, options.colors[TCOLOR_HORIZON][0]/*RGB(0xe2,0x04,0x1b)*/);
	CPen redDot(PS_SOLID, 1, options.colors[TCOLOR_HORIZON][1]/*RGB(0x3e,0xb3,0x70)*/);
	HGDIOBJ penOld = pDC->SelectObject(redSolid);
	CPoint ptScroll = GetScrollPos();

	int startLine = ptScroll.y + rClip.top;
	int endLine = ptScroll.y + rClip.bottom;
	int gridHeightNum = 16;
	int startGridLine;
	if(startLine % gridHeightNum == 0)startGridLine = startLine;
	else startGridLine = startLine - startLine % gridHeightNum;
	int gridLineWidth = m_cell.cx*(16*4+12);
	int minusTop = ptScroll.y*m_cell.cy;
	int kirikae=0;
	for(; startGridLine <= endLine; startGridLine += gridHeightNum/2) {
		//		TRACE("Grid: startGridLine=%d, y=%d\n", startGridLine, (startGridLine+1)*m_cell.cy);
		if(kirikae)pDC->SelectObject(redDot);
		else pDC->SelectObject(redSolid);
		kirikae=!kirikae;
		pDC->MoveTo(0, (startGridLine+1)*m_cell.cy - minusTop - 1);
		pDC->LineTo(gridLineWidth, (startGridLine+1)*m_cell.cy - minusTop - 1);
	}
	pDC->SelectObject(penOld);
	pDC->SetBkMode(OldBkMode);
}

void CBZView::PutUnicodeChar(WORD w, BOOL wascolorset)
{
	WCHAR wbs[3] = {0};
	wbs[0] = w;
	wbs[1] = NULL;
#if 1//#ifndef _UNICODE
	char  mbs[4];
	int len = ::WideCharToMultiByte(CP_ACP, 0, wbs, -1, mbs, 3, NULL, NULL);
	if(len <= 1 || mbs[0] == '?' || wbs[0] < 0x20) {
		if(!wascolorset) {
			if(len == 0 || wbs[0] == 0x00) {
				SetColor(TCOLOR_TEXTNULL);
			} else {
				SetColor(TCOLOR_CHARNG);
			}
		}
		PutStr(". ");
	} else {
		if(mbs[1] == 0) {
			mbs[1] = ' '; mbs[2] = 0;
		}
		PutStr(mbs);
	}
#else
	if(wbs[0] < 0x20)
		PutStr(_T(". "));
	else {
		CStringA tmp(wbs);
		if(tmp.GetLength()==1)
		{
			wbs[1]=' ';
			wbs[2]=NULL;
}
/*		if(mbs[1] == 0) {
			mbs[1] = ' '; mbs[2] = 0;
		}*/
		PutStr(wbs);
	}
#endif
}

void CBZView::DrawHeader()
{
	if(IsToFile() && !options.sDumpHeader.IsEmpty()) {
		m_pFile->Write(options.sDumpHeader, options.sDumpHeader.GetLength());
	} else {
		SetHeaderColor();
		Locate(DUMP_X,0);
		for_to(i,16)
			PutFormatStr("+%1X ", i);
		PutChar();
		for_to_(i,16)
			PutFormatStr("%1X", i);
	}
}

void CBZView::DrawDummyCaret(CDC* pDC)
{
	if(GetFocus() != this) {
		POINT ptOrg = GetScrollPos();
		DWORD dwOrg = ptOrg.y * 16;

		RECT rClient;
		GetClientRect(&rClient);
		PixelToGrid(rClient);
		DWORD dwBottom = dwOrg + (rClient.y2 - DUMP_Y) * 16;
		DWORD dwMax = m_dwTotal + 1;
		if(dwBottom > dwOrg && dwBottom < dwMax)	// ###1.61
			dwMax = dwBottom;
		POINT pt;
		LONG ptx2 = -1;	// ###1.62
		if(m_dwCaret < dwOrg || m_dwCaret >= dwMax) {
		} else {
			pt.x = (m_dwCaret - dwOrg)%16;
			ptx2 = pt.x + CHAR_X;
			pt.x = pt.x*3 + DUMP_X;
			if(m_bCaretOnChar) Swap(pt.x, ptx2);
			pt.x -= ptOrg.x;
			ptx2 -= ptOrg.x;
			pt.y = (m_dwCaret - dwOrg)/16 + DUMP_Y;

			GridToPixel(pt);
			CRgn rgn1;
			rgn1.CreateRectRgn(pt.x, pt.y, pt.x+m_cell.cx, pt.y+m_cell.cy);
			InvertRgn(pDC->m_hDC, rgn1);
		}
	}
}

BOOL CBZView::DrawCaret()
{
	BOOL bDraw = FALSE;
	POINT ptOrg = GetScrollPos();
	DWORD dwOrg = ptOrg.y * 16;

	RECT rClient;
	GetClientRect(&rClient);
	PixelToGrid(rClient);
	DWORD dwBottom = dwOrg + (rClient.y2 - DUMP_Y) * 16;
	DWORD dwMax = m_dwTotal + 1;
	if(dwBottom > dwOrg && dwBottom < dwMax)	// ###1.61
		dwMax = dwBottom;

	POINT pt;
	LONG ptx2 = -1;	// ###1.62
	if(m_dwCaret < dwOrg || m_dwCaret >= dwMax) {
		pt.x = pt.y = -1;
	} else {
		pt.x = (m_dwCaret - dwOrg)%16;
		ptx2 = pt.x + CHAR_X;
		pt.x = pt.x*3 + DUMP_X;
		if(m_bCaretOnChar) Swap(pt.x, ptx2);
		pt.x -= ptOrg.x;
		ptx2 -= ptOrg.x;
		pt.y = (m_dwCaret - dwOrg)/16 + DUMP_Y;
		bDraw = TRUE;
	}
	MoveCaret(pt);
	pt.x = ptx2;
	MoveCaret2(pt);

	
	if(GetMainFrame() && GetMainFrame()->m_bInspectView) {
		CBZInspectView* pView = (CBZInspectView*)GetNextWindow(GW_HWNDPREV);
		pView->Update();
	}

	return bDraw;
}

DWORD CBZView::PointToOffset(CPoint pt)
{
	CRect r;
	GetClientRect(&r);
	if(!r.PtInRect(pt))
		return UINT_MAX;
	PixelToGrid(pt);
	pt += GetScrollPos();
	if(pt.y < DUMP_Y) // || pt.x == CHAR_X-1 || pt.x >= CHAR_X+16)	// ###1.5
 		return UINT_MAX;
	if(pt.x >= CHAR_X) {
		if((pt.x -= CHAR_X) > 16) pt.x = 16;
		m_bCaretOnChar = TRUE;	
	} else {
		if(pt.x < DUMP_X)
			pt.x = 0;
		else
			pt.x -= DUMP_X;
		pt.x/=3;
		m_bCaretOnChar = FALSE;
	}
	DWORD ofs = (pt.y - DUMP_Y)*16 + pt.x;
	return (ofs > m_dwTotal) ? m_dwTotal : ofs;
}

BOOL CBZView::GotoCaret()
{
	if(DrawCaret()) {
		if(m_bBlock)
			Invalidate(FALSE);
		return TRUE;
	}
	POINT pt;
	pt.x = 0;
	pt.y = m_dwCaret/16 - 1;
	ScrollToPos(pt);
	Invalidate(FALSE);
	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// CBZView message handlers

void CBZView::Activate()
{
	GetMainFrame()->SetActiveView(this);
	if(GetFocus() != this) SetFocus();	// ###1.62
}

void CBZView::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	// TODO: Add your message handler code here and/or call default
	OnDoubleClick();
	CTextView::OnLButtonDblClk(nFlags, point);
}

void CBZView::OnDoubleClick()	// ### 1.62
{
	if(m_dwStruct) {
		CBZFormView* pView = (CBZFormView*)GetNextWindow(GW_HWNDPREV);
		pView->OnSelchangeListTag();
	} else {
		OnStatusInfo();
	}
}

void CBZView::OnLButtonDown(UINT nFlags, CPoint point) 
{
	// TODO: Add your message handler code here and/or call default
	if(m_bBlock) {
		m_bBlock = FALSE;
		Invalidate(FALSE);
	}
	BOOL bOnChar = m_bCaretOnChar;
	DWORD ofs = PointToOffset(point);
	if(ofs != UINT_MAX) {
		if(m_dwCaret != ofs || bOnChar != m_bCaretOnChar) {
			m_dwOldCaret = m_dwCaret;
			if(nFlags & MK_SHIFT) {
				m_bBlock = TRUE;
				Invalidate(FALSE);
			} else
				m_dwBlock = ofs;
			m_dwCaret = ofs;
			DrawCaret();
		}
		SetCapture();
	}
	m_bEnterVal = FALSE;
	CTextView::OnLButtonDown(nFlags, point);
}

void CBZView::OnMButtonDown(UINT nFlags, CPoint point) 
{
	m_bCaretOnChar = !m_bCaretOnChar;
	GotoCaret();
	CTextView::OnMButtonDown(nFlags, point);
}

void CBZView::OnMouseMove(UINT nFlags, CPoint point) 
{
	// TODO: Add your message handler code here and/or call default
	if(nFlags & MK_LBUTTON) {
		DWORD ofs = PointToOffset(point);
		if(ofs != UINT_MAX) {
			if(m_timer) {
				KillTimer(m_timer);
				m_timer = 0;
			}
			if(m_dwCaret != ofs && m_dwBlock != ofs) {
				DWORD dwCaretOld = m_dwCaret;		// ###1.5
				POINT ptCaretOld = m_ptCaret;
				m_dwCaret = ofs;
				m_bBlock = TRUE;
				if(DrawCaret())
					Invalidate(FALSE);
				else {
					m_dwCaret = dwCaretOld;
					m_ptCaret = ptCaretOld;
				}
			}
		} else {
			RECT r;
			GetClientRect(&r);
			if((point.y < 0 || point.y > r.y2) && !m_timer)
				m_timer = SetTimer(point.y < 0 ? TIMER_UP : TIMER_DOWN, 50, NULL);
		}	
	}
	CTextView::OnMouseMove(nFlags, point);
}

void CBZView::OnLButtonUp(UINT nFlags, CPoint point) 
{
	// TODO: Add your message handler code here and/or call default
	ReleaseCapture();
	if(m_timer) {
		KillTimer(m_timer);
		m_timer = 0;
	}
	CTextView::OnLButtonUp(nFlags, point);
}

void CBZView::OnTimer(UINT nIDEvent) 
{
	// TODO: Add your message handler code here and/or call default
//	TRACE("Caret.x=%d\n", m_ptCaret);
	if(m_timer == TIMER_UP)
		OnKeyDown(VK_UP, 0, 0);
	else
		OnKeyDown(VK_DOWN, 0, 0);
}


void CBZView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	// TODO: Add your message handler code here and/or call default
	DWORD dwNewCaret = m_dwCaret;
	BOOL bCtrl  = (GetKeyState(VK_CONTROL) < 0);
	BOOL bShift = (GetKeyState(VK_SHIFT) < 0 || GetKeyState(VK_LBUTTON) < 0);
	TRACE("KeyDown: %X %d %X\n", nChar, nRepCnt, nFlags);
	switch(nChar) {
	case VK_RETURN:
		if(bCtrl) OnDoubleClick();	// ### 1.62
		else
			PostMessage(WM_COMMAND, ID_JUMP_FINDNEXT);
		return;
	case VK_INSERT:
		if(!m_pDoc->m_bReadOnly
#ifdef FILE_MAPPING
		 && !m_pDoc->IsFileMapping()
#endif //FILE_MAPPING
		) {
			m_bIns = !m_bIns;
			InitCaret();
			return;
		}
		goto Error;
	case VK_TAB:
		if(bCtrl) {
			GetMainFrame()->ChangeView(this);
			return ;
		}
		if(bShift && GetMainFrame()->m_bStructView) {
			CBZFormView* pView = (CBZFormView*)GetNextWindow(GW_HWNDPREV);
			pView->Activate();
			return;
		}
		m_bCaretOnChar = !m_bCaretOnChar;
		break;
	case VK_RIGHT:
		if(dwNewCaret<m_dwTotal)dwNewCaret++;
		break;
	case VK_LEFT:
		if(dwNewCaret>0)dwNewCaret--;
		 break;
	case VK_DOWN:
		dwNewCaret += 16;
		if(dwNewCaret < m_dwCaret) dwNewCaret = m_dwCaret;
		break;
	case VK_UP:
		if(dwNewCaret>=16)dwNewCaret -= 16;
		break;
	case VK_NEXT://PageDown
		dwNewCaret += 16 * PAGESKIP;
		if(dwNewCaret > m_dwTotal || dwNewCaret < m_dwCaret)
			dwNewCaret = m_dwTotal;
		break;
	case VK_PRIOR://PageUp
		if(dwNewCaret>=16 * PAGESKIP)
		{
			dwNewCaret -= 16 * PAGESKIP;
			if(dwNewCaret > m_dwTotal || dwNewCaret > m_dwCaret)
				dwNewCaret = 0;
		} else dwNewCaret = 0;
		break;
	case VK_HOME:
		if(bCtrl)	dwNewCaret = 0;
		else		dwNewCaret&=~(16-1);
		break;
	case VK_END:
		if(bCtrl)	dwNewCaret = m_dwTotal;
		else		dwNewCaret|=(16-1);
		break;
	case VK_BACK:
		if(!dwNewCaret) goto Error;
		if(!m_bBlock) dwNewCaret--;
	case VK_DELETE:
		if(m_pDoc->m_bReadOnly)
			goto Error;
#ifdef FILE_MAPPING
		if(m_pDoc->IsFileMapping()) goto Error;
#endif //FILE_MAPPING
		if(m_bBlock) {
			CutOrCopy(EDIT_DELETE);
			m_bEnterVal = FALSE;
			return;
		} else {
			if(dwNewCaret == m_dwTotal) goto Error;
			m_pDoc->StoreUndo(dwNewCaret, 1, UNDO_INS);
			m_pDoc->DeleteData(dwNewCaret, 1);
			UpdateDocSize();
		}
		break;
	default:
		CTextView::OnKeyDown(nChar, nRepCnt, nFlags);
		return;
	}
	m_bEnterVal = FALSE;
	if(!m_bBlock && bShift) {
		m_dwBlock = m_dwCaret;
		m_bBlock = TRUE;
	} else if(m_bBlock && !bShift) {
		m_bBlock = FALSE;
		Invalidate(FALSE);
	}
	if(m_ptCaret.x == -1) {
		if(dwNewCaret <= m_dwTotal) {	// ### 1.62
			m_dwCaret = dwNewCaret;
			GotoCaret();
		}
	} else
		MoveCaretTo(dwNewCaret);
	return;
Error:
	MessageBeep(MB_NOFOCUS);
	return;
}

// CBZView::OnCharより後ろにあるので、ここでプロトタイプ宣言。
int convertWCHARtoUTF8(LPBYTE buffer, WORD w);

void CBZView::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	static UINT preChar = 0;
	//LPBYTE p;
	DWORD dwSize;

	// TODO: Add your message handler code here and/or call default
	// MBCSビルド時は、マルチバイト文字が来た場合2つのWM_CHARに分割され、それぞれのnCharは255以下になる。
	// 一方Unicodeビルド時は、1文字そのままWM_CHARとして届くため、nCharは256以上の場合もあり得る。
	if(nChar < ' ' || (m_charset == CTYPE_ASCII && nChar >= 256))
		return;
	if(m_pDoc->m_bReadOnly)
		goto Error;
	if(!m_bEnterVal && !preChar) {	// hex入力中でもなければ、(MBCSの)2文字目でもない
		dwSize = 1;
#ifdef UNICODE
		if(m_bCaretOnChar && (
			(m_charset == CTYPE_SJIS || m_charset == CTYPE_JIS) && nChar >= 256 && (nChar < 0xFF61 || nChar > 0xFF9F) || // 半角カナを考慮
			(m_charset == CTYPE_UNICODE) || // CTYPE_UNICODEはサロゲートペアじゃない限り常に2バイト。
			(m_charset > CTYPE_UNICODE && m_charset != CTYPE_JIS && nChar >= 128) // (特にUTF-8は)latin-1文字もマルチバイトになる。
			)) {
#else
		// TODO: EUC時半角カナが来ると、dwSize=1に対して実サイズ=2なのでデータ破壊してしまう。
		if(m_bCaretOnChar && (m_charset == CTYPE_UNICODE || (m_charset > CTYPE_UNICODE && _ismbblead((BYTE)nChar)))) {
#endif
			if(m_charset == CTYPE_UTF8) {	// ### 1.54b
				// TODO: 4バイトパターン対応
				if(nChar <= 0x7FF)
					dwSize = 2;
				else
					dwSize = 3;
			} else {
				// TODO: サロゲートペア対応
				dwSize = 2;
			}
		}
		if(m_bIns || (m_dwCaret == m_dwTotal)) {
#ifdef FILE_MAPPING
			if(m_pDoc->IsFileMapping()) goto Error;
#endif //FILE_MAPPING
			m_pDoc->StoreUndo(m_dwCaret, dwSize, UNDO_DEL);
			m_pDoc->InsertData(m_dwCaret, dwSize, TRUE);
			UpdateDocSize();
		} else
			m_pDoc->StoreUndo(m_dwCaret, dwSize, UNDO_OVR);
	}
	m_bBlock = FALSE;
	//p = m_pDoc->QueryMapViewTama2(m_dwCaret, 4); //p  = m_pDoc->GetDocPtr() + m_dwCaret;
	if(!m_bCaretOnChar) {
		if(nChar >= '0' && nChar <= '9')
			nChar -= '0';
		else if(nChar >= 'A' && nChar <= 'F')
			nChar -= 'A' - 10;
		else if(nChar >= 'a' && nChar <= 'f')
			nChar -= 'a' - 10;
		else
			goto Error;
		if(m_bEnterVal) {
			LPBYTE p = m_pDoc->QueryMapViewTama2(m_dwCaret, 1);
			BYTE nVal = *p;
			nChar |= nVal<<4;
			m_bEnterVal = FALSE;
		} else
			m_bEnterVal = TRUE;
#ifdef UNICODE
	// TODO: がんばって!UNICODE時のコードにマージ(#ifdef UNICODEは残ると思うが)。
	// TODO: UTF-8もconvertWCHARtoUTF8ではなくWideCharToMultiByteを使うように変更。
	} else if(m_charset == CTYPE_UNICODE) {
		// バイトオーダーを調整して、そのまま使用
		WORD w = SwapWord(nChar);
		m_pDoc->memcpyMem2Filemap(m_dwCaret, &w, sizeof(w));//*(LPWORD)p = SwapWord(nChar);

		// 後続の処理ではBYTEにキャストしていて使いまわせないので、上で代入したうえでコピペ…。
		Invalidate(FALSE);
		if(!m_bEnterVal) {
			MoveCaretTo(m_dwCaret + 2); // TODO: サロゲートペア対応
		}
		return;
	} else if(m_charset == CTYPE_UTF8) {
		// UTF-16から直接UTF-8に変換して使用  TODO: サロゲートペア対応
		BYTE buf[4];
		LPBYTE pb = buf;
		int len = convertWCHARtoUTF8(buf, nChar);
		m_pDoc->memcpyMem2Filemap(m_dwCaret, pb, len);//for_to(i, len) *p++ = *pb++;

		Invalidate(FALSE);
		if(!m_bEnterVal) {
			MoveCaretTo(m_dwCaret + len);
		}
		return;
	} else if(m_charset >= CTYPE_SJIS) { // SJIS, JIS, EUC, UTF8, ...
		// TODO: 適当すぎる作りなので、そのうち書き直す。
		CHAR mbs[4];
		int bytes;

		// とりあえずSJISにして、後はConvertCharSetに任せる。
		bytes = WideCharToMultiByte(932, 0, (LPCWCH)&nChar, 1, mbs, sizeof(mbs), "?", NULL);
		mbs[bytes] = 0;

		// TODO: ほぼコピペなのを何とかする…
		char *pb = mbs;
		LPBYTE buffer = NULL;
		int len;
		if(m_charset == CTYPE_SJIS) {
			buffer = (LPBYTE)MemAlloc(bytes);
			memcpy(buffer, mbs, bytes);
			len = bytes;
		} else {
			len = ConvertCharSet(m_charset, mbs, buffer);
		}
		if(len) {
			//if(m_charset == CTYPE_UNICODE) len *= 2; // ここでは意味ない。
			pb = (char*)buffer;
			m_pDoc->memcpyMem2Filemap(m_dwCaret, pb, len);//for_to(i, len) *p++ = *pb++;
			MemFree(buffer);
			Invalidate(FALSE);
			if(!m_bEnterVal) 
				MoveCaretTo(m_dwCaret + len);
		}
		return;
#else
	} else if(m_charset >= CTYPE_UNICODE) {
		char  mbs[4];
		char* pb = mbs;
		if(preChar) {
			*pb++ = preChar;
			preChar = 0;
		} else if(_ismbblead((BYTE)nChar)) {
			preChar = nChar;
			return;
		}
		*pb++ = (char)nChar;
		*pb++ = 0;
		LPBYTE buffer = NULL;
		int len = ConvertCharSet(m_charset, mbs, buffer);
		if(len) {
			if(m_charset == CTYPE_UNICODE) len *= 2;
			pb = (char*)buffer;
			m_pDoc->memcpyMem2Filemap(m_dwCaret, pb, len);//for_to(i, len) *p++ = *pb++;
			MemFree(buffer);
			Invalidate(FALSE);
			if(!m_bEnterVal) 
				MoveCaretTo(m_dwCaret + len);
		}
		return;
#endif
	}
	m_pDoc->memcpyMem2Filemap(m_dwCaret, &nChar, 1);//*p = (BYTE)nChar;
	Invalidate(FALSE);
	if(!m_bEnterVal) {
		MoveCaretTo(m_dwCaret+1);
	}
	return;
Error:
	MessageBeep(MB_NOFOCUS);
	return;
}

void CBZView::MoveCaretTo(DWORD dwNewCaret)
{
	if(dwNewCaret > m_dwTotal) {
		dwNewCaret = m_dwTotal;
	}

	int dy = dwNewCaret/16 - m_dwCaret/16;
	m_dwCaret = dwNewCaret;
	if(!DrawCaret()) {
		ScrollBy(0, dy, !m_bBlock);
	}
	if(m_bBlock)
		Invalidate(FALSE);

	
}

void CBZView::UpdateDocSize()
{
	m_dwTotal = m_pDoc->GetDocSize();
	SIZE cTotal;
	cTotal.cx = VIEWCOLUMNS;
	cTotal.cy = m_dwTotal / 16 + 2;
	SetTextSize(cTotal, PAGESKIP);
	Invalidate(FALSE);
}

void CBZView::OnViewFont() 
{
	// TODO: Add your command handler code here
	LOGFONT logFont;
	m_pFont->GetLogFont(&logFont);
	WTL::CFontDialog aFontDlg(&logFont, CF_SCREENFONTS | CF_FIXEDPITCHONLY | CF_NOVERTFONTS | CF_ANSIONLY); // ###1.5
	int rDlg = aFontDlg.DoModal();
	if(rDlg == IDOK) {
		options.sFontName = aFontDlg.GetFaceName();
		options.fFontStyle = aFontDlg.IsBold() + aFontDlg.IsItalic()*2;
		options.nFontSize = aFontDlg.GetSize();
		ChangeFont(logFont);
		CBZView* pView = GetBrotherView();
		if(pView) pView->ChangeFont(logFont);
	}
}

void CBZView::ChangeFont(LOGFONT& logFont)
{
	delete m_pFont;
	m_pFont = new CFont;
	m_pFont->CreateFontIndirect(&logFont);
	OnChangeFont();
	m_bResize = FALSE;
	Invalidate(TRUE);
}

void CBZView::OnViewColor() 
{
	// TODO: Add your command handler code here
	CSetupColorDialog dlg;
	dlg.m_pSampleFont = m_pFont;
	if(dlg.DoModal() == IDOK) 
		GetMainFrame()->Invalidate(TRUE);
}

/////////////////////////////////////////////////////////////////////////////
// CBZView Update UI

void CBZView::OnCharMode(UINT nID) 
{
	// TODO: Add your command handler code here
	m_charset = options.charset = (CharSet)(nID - ID_CHAR_ASCII);
	if(m_charset == CTYPE_EBCDIC)
		LoadEbcDicTable();
	options.Touch();
	Invalidate(TRUE);
	CBZView* pView = GetBrotherView();
	if(pView) {
		pView->m_charset = options.charset;
		pView->Invalidate(TRUE);
	}
}

void CBZView::OnUpdateCharMode(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	pCmdUI->SetRadio(pCmdUI->m_nID == (UINT)(options.charset + ID_CHAR_ASCII));
//	if(m_charset != options.charset && pCmdUI->m_nID == (UINT)(m_charset + ID_CHAR_ASCII))
//		pCmdUI->SetCheck(TRUE);
}

void CBZView::OnCharAutoDetect() 
{
	// TODO: Add your command handler code here
	options.bAutoDetect = !options.bAutoDetect;
}

void CBZView::OnUpdateCharAutoDetect(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	pCmdUI->SetCheck(options.bAutoDetect);
}

void CBZView::OnUpdateStatusSize(CCmdUI* pCmdUI)
{
	if(m_dwTotal) {
		CString sResult;
		if(m_bHexSize)
			sResult.Format(_T("0x%X"), m_dwTotal);
		else
			sResult = SeparateByComma(m_dwTotal);
		sResult += _T(" bytes");
		pCmdUI->SetText(sResult);
		pCmdUI->Enable(TRUE);
	} else
		pCmdUI->Enable(FALSE);
}

void CBZView::OnUpdateStatusChar(CCmdUI* pCmdUI)
{
	static TCHAR *sCharSet[CTYPE_COUNT] = { _T("ASCII"), _T("S-JIS"), _T("UTF-16"), _T("JIS"), _T("EUC"), _T("UTF-8"), _T("EBCDIC"), _T("EPWING") };
	pCmdUI->SetText(sCharSet[m_charset]);
	pCmdUI->Enable(TRUE);
}

void CBZView::UpdateStatusInfo()
{
	GetMainFrame()->m_wndStatusBar.SetPaneText(1, GetStatusInfoText());
}

void CBZView::OnUpdateStatusInfo(CCmdUI* pCmdUI)
{
	if(GetMainFrame()->m_bDisableStatusInfo) return;
	if(m_dwTotal) {
		pCmdUI->SetText(GetStatusInfoText());
		pCmdUI->Enable(TRUE);
	} else
		pCmdUI->Enable(FALSE);
}

CString CBZView::GetStatusInfoText()
{
	CString sResult;
	if(m_dwTotal) {
		LPCTSTR pFmtHexa;
		int val;
		if(m_bBlock) {
			if(m_nColAddr > ADDRCOLUMNS) 
				sResult.Format(_T("%04X:%04X-%04X:%04X"), HIWORD(BlockBegin()), LOWORD(BlockBegin()), HIWORD(BlockEnd()), LOWORD(BlockEnd()));  // ###1.61
			else
				sResult.Format(_T("%06X-%06X"), BlockBegin(), BlockEnd());
			val = BlockEnd() - BlockBegin();
			pFmtHexa = _T(" 0x%X(%s) bytes");

			CString sValue;
			sValue.Format(pFmtHexa, val, (LPCTSTR)SeparateByComma(val));
			sResult += sValue;
		} else if(m_nBytes<=4) {
			static TCHAR szFmtHexa[] = _T(": 0x%02X (%s)");
			if(m_nColAddr > ADDRCOLUMNS)
				sResult.Format(_T("%04X:%04X"), HIWORD(m_dwCaret), LOWORD(m_dwCaret));
			else
				sResult.Format(_T("%06X"), m_dwCaret);
#ifdef FILE_MAPPING
			if(m_pDoc->IsOutOfMap(m_pDoc->GetDocPtr() + m_dwCaret)) return sResult;
#endif //FILE_MAPPING
			val = GetValue(m_dwCaret, m_nBytes);
			szFmtHexa[6] = '0'+ m_nBytes * 2;
			pFmtHexa = szFmtHexa;

			CString sValue;
			sValue.Format(pFmtHexa, val, (LPCTSTR)SeparateByComma(val));
			sResult += sValue;
		} else if(m_nBytes==8) {
			if(m_nColAddr > ADDRCOLUMNS)
				sResult.Format(_T("%04X:%04X"), HIWORD(m_dwCaret), LOWORD(m_dwCaret));
			else
				sResult.Format(_T("%06X"), m_dwCaret);
#ifdef FILE_MAPPING
			if(m_pDoc->IsOutOfMap(m_pDoc->GetDocPtr() + m_dwCaret)) return sResult;
#endif //FILE_MAPPING
			ULONGLONG qval = GetValue64(m_dwCaret);

			CString sValue;
			sValue.Format(_T(": 0x%016I64X (%s)"), qval, (LPCTSTR)SeparateByComma64(qval));
			sResult += sValue;
		}
	}
	return sResult;
}

void CBZView::OnUpdateStatusIns(CCmdUI* pCmdUI)
{
	CString sText;
	sText.LoadString(m_pDoc->m_bReadOnly ? IDS_EDIT_RO : IDS_EDIT_OVR + m_bIns);
	pCmdUI->SetText(sText);
}

void CBZView::OnStatusInfo()
{
	if((m_nBytes*=2) == 8)
		m_nBytes = 1;	
}

void CBZView::OnStatusSize()
{
	m_bHexSize = !m_bHexSize;
}

void CBZView::OnStatusChar()
{
	m_charset = (CharSet)(m_charset + 1);
	if(m_charset >= CTYPE_COUNT) m_charset = CTYPE_ASCII;
	OnCharMode(m_charset + ID_CHAR_ASCII);
}

int CBZView::GetValue(DWORD dwOffset, int bytes)
{
	LPBYTE lpStart = m_pDoc->QueryMapViewTama2(dwOffset, bytes); //LPBYTE p  = m_pDoc->GetDocPtr();
	int val = 0;
	if(!lpStart || m_pDoc->GetMapRemain(dwOffset) < bytes)return 0;
	if(dwOffset + bytes > m_dwTotal)
		val = 0;
	else {
		switch(bytes) {
			case 1: val = *lpStart; break;
			case 2: val = SwapWord(*(WORD*)lpStart); break;
			case 4: val = SwapDword(*(DWORD*)lpStart); break;
		}
	}
	return val;
}

ULONGLONG CBZView::GetValue64(DWORD dwOffset)
{
	LPBYTE lpStart = m_pDoc->QueryMapViewTama2(dwOffset, 8); //LPBYTE p  = m_pDoc->GetDocPtr();
	ULONGLONG val = 0;
	if(!lpStart || m_pDoc->GetMapRemain(dwOffset) < 8)return 0;
	if(dwOffset + 8 > m_dwTotal)
		val = 0;
	else {
		val = SwapQword(*(ULONGLONG*)lpStart);
	}
	return val;
}

void CBZView::SetValue(DWORD dwOffset, int bytes, int val)
{
	LPBYTE lpStart = m_pDoc->QueryMapViewTama2(dwOffset, bytes); //LPBYTE p  = m_pDoc->GetDocPtr();
	if(!lpStart || m_pDoc->GetMapRemain(dwOffset) < bytes /*|| dwOffset + bytes > m_dwTotal*/)return;
	m_pDoc->StoreUndo(dwOffset, bytes, UNDO_OVR);
	SetValue(lpStart, bytes, val);
	Invalidate(FALSE);
	if(m_dwStruct) {
		CBZFormView* pView = (CBZFormView*)GetWindow(GW_HWNDFIRST);
		pView->OnSelchangeListTag();
	}
}

void  CBZView::SetValue(LPBYTE p, int bytes, int val)
{
	switch(bytes) {
		case 1: *p = val; break;
		case 2: *(WORD*)p = SwapWord(val); break;
		case 4: *(DWORD*)p = SwapDword(val); break;
	}
}

/////////////////////////////////////////////////////////////////////////////
// CBZView Jump


void CBZView::OnUpdateJump(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	pCmdUI->Enable(m_pDoc->GetDocSize());
}

void CBZView::OnJumpStart() 
{
	// TODO: Add your command handler code here
	JumpTo(0);
}

void CBZView::OnJumpEnd() 
{
	// TODO: Add your command handler code here
	JumpTo(m_dwTotal);
}

void CBZView::JumpTo(DWORD dwNewCaret)
{
	BOOL bShift = (::GetKeyState(VK_SHIFT) < 0);
	if(!m_bBlock && bShift) {
		m_dwBlock = m_dwCaret;
		m_bBlock = TRUE;
	} else if(m_bBlock && !bShift) {
		m_bBlock = FALSE;
		Invalidate(FALSE);
	}
	m_dwOldCaret = m_dwCaret;
	m_dwCaret = dwNewCaret;
	GotoCaret();
}

void CBZView::OnJumpOffset() 
{
	// TODO: Add your command handler code here
	int nBytes = m_nBytes;
	if(m_nBytes>4)
	{
//		MessageBox("m_nBytes>4", "Debug", MB_OK);
		nBytes = 4;
	}
	int ofs = GetValue(m_dwCaret, nBytes);
	ofs += nBytes;
	if(m_dwCaret + ofs <= m_dwTotal) 
		JumpTo(m_dwCaret + ofs);
}

void CBZView::OnJumpReturn() 
{
	// TODO: Add your command handler code here
	if(GotoCaret()) {
		Swap(m_dwCaret, m_dwOldCaret);
		GotoCaret();
	}
}

void CBZView::SetMark()
{
	m_pDoc->SetMark(m_dwCaret);
	Invalidate(FALSE);
}

void CBZView::JumpToMark()
{
	DWORD dwMark = m_pDoc->JumpToMark(m_dwCaret);
	if(dwMark != INVALID) 
		JumpTo(dwMark);
}

void CBZView::OnJumpFindnext() 
{
	CTBComboBox* pCombo = &GetMainFrame()->m_wndToolBar.m_combo;

	CStringA sFind;
	{
		LPSTR tmp = sFind.GetBufferSetLength(510);
		::GetWindowTextA(pCombo->m_hWnd, tmp, 509);
		sFind.ReleaseBuffer();
	}
	if(sFind.IsEmpty()) {
		if(GetBrotherView())
			OnJumpCompare();
		return;
	}

	CStringA sOrigFind = sFind; // 保存しておかないと、literal find時pCombo->AddTextで間違った文字列が保存されてしまう。

	int c1 = sFind[0]; // 検索文字1
	bool literal = false;
	if(c1 == '"') {
		literal = true;
		if(sFind.GetLength() < 2) {
			// too short.
			return;
		}
		c1 = sFind[1];
		sFind.Delete(0, 1);
	}
	int c2 = 0; // 検索文字2 (case-insensitive時のみ) !=0なら有効
	if(!literal && c1 == '=') {
		pCombo->SetText(_T("? "));
		return;
	}
	pCombo->AddText(sOrigFind);
	if(!literal && (c1 == '?' || c1 == '+' || c1 == '>'|| c1 == '<')) {
		DWORD dwNew = 0;
		long nResult;
		if(CalcHexa((LPCSTR)sFind + 1, nResult)) {
			switch(c1) {
			case '?': {
				CString sResult;
				sResult.Format(_T("= 0x%X (%d)"), nResult, nResult);
				pCombo->SetText(sResult);
				break;
			}
			case '+':
				dwNew = m_dwCaret + m_pDoc->m_dwBase;
			case '>':
				dwNew += nResult - m_pDoc->m_dwBase;	// ###1.63
				if(dwNew <= m_dwTotal) {
					m_dwOldCaret = m_dwCaret;
					m_dwCaret = dwNew;
					m_bCaretOnChar = FALSE;
					GotoCaret();
					SetFocus();
				} else
					AfxMessageBox(IDS_ERR_RANGE);
				break;
			case '<':
				if(m_pDoc->m_bReadOnly)
					AfxMessageBox(IDS_ERR_READONLY);
				else {
					if(m_bBlock)
						FillValue(nResult);
					else
						SetValue(m_dwCaret, m_nBytes, nResult);				
				}
				SetFocus();
				break;
			}
		}
		return;
	}

	//検索モード [# 00 AA BB FF] または [abcdef]

	CWaitCursor wait;	// ###1.61

	DWORD dwStart = m_dwCaret + 1;
	if(dwStart >= m_dwTotal)return;

	DWORD dwRetAddress = 0xFFFFffff;//err

	int nFind = 0;
	LPBYTE pFind = NULL;
	CharSet charset = m_charset;
	bool negativeFind = false;
	if(!literal && c1 == '#') {//検索モード [# 00 AA BB FF]
		if(sFind[1] == '!') {
			negativeFind = true;
			sFind.Delete(0, 2); // 2 = strlen("#!")
		}
		nFind = ReadHexa(sFind, pFind);
		if(!nFind) return;
		if(negativeFind && nFind != 1) {
			// 2文字以上のnegative findは今のところ未実装(word、dwordあたりは対応させたいかも。)
			AfxMessageBox(IDS_ERR_TOOLONG_NEGFIND, MB_OK | MB_ICONERROR);
			return;
		}
		if(!negativeFind)
			dwRetAddress = strstrBinary(pFind, nFind, dwStart);//charset = CTYPE_BINARY;
		else {
			// negativeFind
			DWORD dwOffset = dwStart;
			while(dwOffset < m_dwTotal) {
				LPBYTE p = m_pDoc->QueryMapViewTama2(dwOffset, m_dwTotal - dwOffset);
				if(!p) {
					dwRetAddress = 0xFFFFffff;
					break;
				}
				dwRetAddress = MemScanByteNeg(p, pFind[0], m_pDoc->GetMapRemain(dwOffset));
				if(dwRetAddress != 0xFFFFffff)
				{
					dwRetAddress += dwOffset;
					break;
				}
			}
		}
	} else {		//検索モード [abcdef]
		if(charset >= CTYPE_UNICODE) { //CTYPE_UNICODE, CTYPE_JIS, CTYPE_EUC, CTYPE_UTF8, CTYPE_EBCDIC, CTYPE_EPWING, CTYPE_COUNT, CTYPE_BINARY
			nFind = ConvertCharSet(charset, sFind, pFind);
			if(!pFind || !nFind) return;
			if(charset == CTYPE_UNICODE) {
				if(dwStart&1) {
					dwStart++;
				}
				dwRetAddress = stristrBinaryW((LPCWSTR)pFind, nFind, dwStart);
			} else {
				dwRetAddress = strstrBinary(pFind, nFind, dwStart);//charset = CTYPE_BINARY;
			}
		} else { //CTYPE_ASCII, CTYPE_SJIS
			nFind = sFind.GetLength();
			if(m_charset == CTYPE_ASCII)
				dwRetAddress = stristrBinaryA(sFind, dwStart);
			else
				dwRetAddress = strstrBinary((LPBYTE)((LPCSTR)sFind), nFind, dwStart);
		}
	}
	if(dwRetAddress==0xFFFFffff)
	{
		AfxMessageBox(IDS_ERR_FINDSTRING);
		pCombo->SetFocus();
		if(pFind) MemFree(pFind);
		return;
	}

	m_dwOldCaret = m_dwCaret;
	m_dwCaret = dwRetAddress;
	m_bCaretOnChar = !pFind;
	GotoCaret();
	SetFocus();

	if(pFind) MemFree(pFind);
}

//戻り値
//全部一緒だと0xFFFFffff
//違うところがあるとオフセットを返す
static DWORD MemCompByte2(const BYTE *p1, const BYTE *p2, DWORD len)
{
	const BYTE *p3 = p1;
	while(*p3++ == *p2++)
	{
		if(--len == 0)return 0xFFFFffff;
	}
	return p3-p1;
}

static DWORD MemScanByteNeg(const BYTE *p, BYTE c, DWORD len)
{
	const BYTE *pstart = p;
	const BYTE *pend = p + len;
	for(;p<pend;p++)
	{
		if(*p != c)return p - pstart;
	}
	return 0xFFFFffff;
}

BOOL CBZView::CalcHexa(LPCSTR sExp, long& n1)
{
	LPCSTR p = sExp;
	n1 = 0;
	long n2;
	char op = 0;
	int  base = 16;

	for(;;) {
		while(*p > 0 && *p <= ' ') p++;
		if(!*p) break;
		LPCSTR p0 = p;
		if(*p == '-')
			n2 = strtol(p, (char**)&p, base);
		else
			n2 = strtoul(p, (char**)&p, base);
		base = 16;
		if(p != p0) {
			switch (op) {
			case 0:
			case '+': n1 += n2; break;
			case '-': n1 -= n2; break;
			case '*': n1 *= n2; break;
			case '/': n1 /= n2; break;
			case '&': n1 &= n2; break;
			case '|': n1 |= n2; break;
			case '^': n1 ^= n2; break;
			}
			op = 0;
		} else {
			char c = *p++;
			if(c == '%')
				base = 10;
			else
				op = c;
		}
	}
	if(op) {
		CString sMsg;
		sMsg.Format(IDS_ERR_SYNTAX, sExp);
		AfxMessageBox(sMsg);
		return FALSE;
	}
	return TRUE;
}
// TODO:isxdigitがあるなら、これくらいの処理はライブラリにありそうだけど、知らないので再実装…。
static BYTE hexCharToByte(CHAR c) {
	if('0' <= c && c <= '9') {
		return c - '0';
	}
	if('A' <= c && c <= 'F') {
		return c - 'A' + 10;
	}
	if('a' <= c && c <= 'f') {
		return c - 'a' + 10;
	}
	return 0;
}
// HEXadecimal STRing max-2-chars TO Byte
// strtolの16進固定で最大長2文字までしか見ない版
static BYTE hexStr2ToB(LPCSTR& pstr, LPCSTR* endptr) {
	const int maxlen = 2;
	BYTE res = 0;
	LPCSTR p = pstr;

	while(p - pstr < maxlen) {
		if(!*p || !isxdigit(*p)) {
			break;
		}
		res *= 16;
		res += hexCharToByte(*p);
		p++;
	}

	if(endptr) {
		*endptr = p;
	}

	return res;
}
int CBZView::ReadHexa(LPCSTR sHexa, LPBYTE& buffer)
{
	LPCSTR p = sHexa;
	LPBYTE pFind = NULL;
	int nFind = 0;

	for(;;) {
		while(*p && !isalnum(*p)) p++;
		if(!*p) break;
		LPCSTR p0 = p;
		BYTE n = hexStr2ToB(p, &p);
		if(n == 0 && p == p0) break;				// ### 1.54
		nFind ++;
		if(!pFind) pFind = (LPBYTE)MemAlloc(nFind);
		else pFind = (LPBYTE)MemReAlloc(pFind, nFind);
		pFind[nFind-1] = n;
	}
	buffer = pFind;
	return nFind;
}

/////////////////////////////////////////////////////////////////////////////
// CBZView Edit


void CBZView::OnUpdateEditCut(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_bBlock && !m_pDoc->m_bReadOnly && !m_pDoc->IsFileMapping());
}

void CBZView::OnUpdateEditCopy(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_bBlock);
	
}

void CBZView::OnUpdateEditPaste(CCmdUI* pCmdUI) 
{
	OpenClipboard();
	UINT cf = EnumClipboardFormats(0);
	CloseClipboard();
	pCmdUI->Enable(!m_pDoc->m_bReadOnly && cf);
}

void CBZView::OnEditCut() 
{
	// TODO: Add your command handler code here
	CutOrCopy(EDIT_CUT);
}

void CBZView::OnEditCopy() 
{
	// TODO: Add your command handler code here
	CutOrCopy(EDIT_COPY);
}

void CBZView::CutOrCopy(CutMode mode)
{
	DWORD dwPtr  = BlockBegin();
	DWORD dwSize = BlockEnd() - dwPtr;
	if(mode == EDIT_COPYHEX)
		m_pDoc->CopyToClipboardWithHexalize(dwPtr, dwSize);
	if(mode == EDIT_COPY || mode == EDIT_CUT)
		m_pDoc->CopyToClipboard(dwPtr, dwSize);
	if(mode == EDIT_DELETE || mode == EDIT_CUT) {
		m_pDoc->StoreUndo(dwPtr, dwSize, UNDO_INS);
		m_pDoc->DeleteData(dwPtr, dwSize);
		m_dwCaret = m_dwOldCaret = dwPtr;
		m_bBlock = FALSE;
		GotoCaret();
		UpdateDocSize();
	}
}

void CBZView::FillValue(int val)
{
	DWORD dwStart  = BlockBegin();
	DWORD dwEnd = BlockEnd();
	DWORD dwSize = dwEnd - dwStart;
	m_pDoc->StoreUndo(dwStart, dwSize, UNDO_OVR);
	
	DWORD dwCurrent = dwStart;
	while(dwCurrent < dwEnd) {
		LPBYTE p = m_pDoc->QueryMapViewTama2(dwCurrent, m_nBytes);
		if(!p || m_pDoc->GetMapRemain(dwCurrent) < m_nBytes)return;
		SetValue(p, m_nBytes, val);
		dwCurrent += m_nBytes;
	}
	Invalidate(FALSE);
}

void CBZView::OnEditPaste() 
{
	// TODO: Add your command handler code here
	DWORD dwPaste;
	if(dwPaste = m_pDoc->PasteFromClipboard(m_dwCaret, m_bIns)) {
		m_dwOldCaret = m_dwCaret;
		m_dwCaret = dwPaste;
		UpdateDocSize();
	}
}

void CBZView::OnEditUndo() 
{
	// TODO: Add your command handler code here
	m_dwCaret = m_pDoc->DoUndo();
	GotoCaret();
	UpdateDocSize();
}

void CBZView::OnUpdateEditValue(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(!m_pDoc->m_bReadOnly);
}


void CBZView::OnEditSelectAll() 
{
	// TODO: Add your command handler code here
	m_bBlock = TRUE;
	m_dwBlock = 0;
	m_dwOldCaret = m_dwCaret;
	m_dwCaret = m_dwTotal;
	GotoCaret();
}

void CBZView::OnUpdateEditSelectAll(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	pCmdUI->Enable(m_pDoc->GetDocSize());
}

// ### 1.63

void CBZView::OnEditCopyDump() 
{
	// TODO: Add your command handler code here
	CMemFile memFile(MEMFILE_GROWBY);
	DrawToFile(&memFile);

	ULONGLONG ulSize = memFile.GetLength();
	DWORD dwSize = (ulSize>0xffffffff) ? 0xffffffff : ulSize;
	HGLOBAL hMemTxt = ::GlobalAlloc(GMEM_MOVEABLE, dwSize + 1);
	LPBYTE pMemTxt  = (LPBYTE)::GlobalLock(hMemTxt);
	memFile.SeekToBegin();
	memFile.Read(pMemTxt, dwSize);
	*(pMemTxt + dwSize) = '\0';
	::GlobalUnlock(hMemTxt);
	AfxGetMainWnd()->OpenClipboard();
	::EmptyClipboard();
	::SetClipboardData(CF_TEXT, hMemTxt);
	::CloseClipboard();
	return;
}

void CBZView::OnUpdateEditCopyDump(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	pCmdUI->Enable(m_bBlock);
}

void CBZView::OnEditCopyHexstring()
{
	// TODO: Add your command handler code here
	CutOrCopy(EDIT_COPYHEX);
}

void CBZView::OnUpdateEditCopyHexstring(CCmdUI* pCmdUI)
{
	// TODO: Add your command update UI handler code here
	pCmdUI->Enable(m_bBlock);
}

void CBZView::OnEditPasteHexstring() 
{
	// TODO: Add your command handler code here
	DWORD dwPaste;
	if(dwPaste = m_pDoc->PasteHexstringFromClipboard(m_dwCaret, m_bIns)) {
		m_dwOldCaret = m_dwCaret;
		m_dwCaret = dwPaste;
		UpdateDocSize();
	}
}

void CBZView::OnUpdateEditPasteHexstring(CCmdUI* pCmdUI) 
{
	OpenClipboard();
	UINT cf = EnumClipboardFormats(0);
	CloseClipboard();
	pCmdUI->Enable(!m_pDoc->m_bReadOnly && cf);
}

void CBZView::OnPasteType() 
{
	int result = CPasteType().DoModal();
	if(result != 0) {
		DWORD dwPaste;
		if(dwPaste = m_pDoc->PasteFromClipboard(m_dwCaret, m_bIns, result)) { // TODO: int result != UINT arg3
			m_dwOldCaret = m_dwCaret;
			m_dwCaret = dwPaste;
			UpdateDocSize();
		}
	}
}

void CBZView::OnUpdatePasteType(CCmdUI* pCmdUI) 
{
	OpenClipboard();
	UINT cf = EnumClipboardFormats(0);
	CloseClipboard();
	pCmdUI->Enable(!m_pDoc->m_bReadOnly && cf);
}


/////////////////////////////////////////////////////////////////////////////
// CBZView Double View

void CBZView::OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView) 
{
	// TODO: Add your specialized code here and/or call the base class

	CMainFrame* pFrame = GetMainFrame();
	if(pFrame) {
		if(bActivate) {
			CBZDocTemplate* pTmpl = (CBZDocTemplate*)m_pDoc->GetDocTemplate();
			pTmpl->SetDocument(m_pDoc);
			pFrame->OnUpdateFrameTitle();
		}
		if(pFrame->m_nSplitView)
			Invalidate(FALSE);
	}
	CTextView::OnActivateView(bActivate, pActivateView, pDeactiveView);
}

void CBZView::OnJumpCompare() 
{
	// TODO: Add your command handler code here
	CBZView* pView1 = GetBrotherView();
	if(!pView1) return;
	CBZDoc *pDoc1 = pView1->m_pDoc;
	GetMainFrame()->m_wndToolBar.m_combo.SetWindowText(_T(""));

	DWORD len = min(m_dwTotal - m_dwCaret, pView1->m_dwTotal - pView1->m_dwCaret);
	if(len <= 1) return;
	len--;
	DWORD dwCurrent0 =         m_dwCaret+1;
	DWORD dwCurrent1 = pView1->m_dwCaret+1;
	LPBYTE pData0, pData1;
	do
	{
		DWORD maxMapSize = min(len, options.dwMaxMapSize);
		pData0 = m_pDoc->QueryMapViewTama2(dwCurrent0, maxMapSize);
		pData1 = pDoc1->QueryMapViewTama2(dwCurrent1, maxMapSize);
		if(!pData0 || !pData1)
		{//ERR: Mapping failed
			MessageBox(_T("File Mapping Error!"), _T("Error"), MB_OK);
			return;
		}
		DWORD dwRemain0 = m_pDoc->GetMapRemain(dwCurrent0); //minmax内で直接callすると何度も実行される
		DWORD dwRemain1 = pDoc1->GetMapRemain(dwCurrent1); //minmax内で直接callすると何度も実行される
		DWORD minMapSize = min(min(dwRemain0, dwRemain1), len); //minmax内で直接callすると何度も実行される
		if(minMapSize==0) return;
		DWORD ofs = MemCompByte2(pData0, pData1, minMapSize);
		if(ofs!=0xFFFFffff)
		{	//Data0 != Data1
			ofs--;
			dwCurrent0 += ofs;
			dwCurrent1 += ofs;
			goto founddiff;
		}
		len -= minMapSize;
		dwCurrent0 += minMapSize;
		dwCurrent1 += minMapSize;
	} while(len > 0);

	if(dwCurrent0==m_dwTotal && dwCurrent1==pView1->m_dwTotal)
	{
		AfxMessageBox(IDS_COMPARE_OK, MB_ICONINFORMATION);
		return;
	}
founddiff:
		m_dwOldCaret = m_dwCaret;
		m_dwCaret = dwCurrent0;
		pView1->m_dwOldCaret = pView1->m_dwCaret;
		pView1->m_dwCaret = dwCurrent1;
		GotoCaret();
		pView1->GotoCaret();
		Invalidate(FALSE);
		pView1->Invalidate(FALSE);
		return;
}

void CBZView::OnUpdateJumpCompare(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	pCmdUI->Enable(GetMainFrame()->m_nSplitView);
}

CBZView* CBZView::GetBrotherView()
{
	CMainFrame *frame= GetMainFrame();
	if(frame)
		return (CBZView*)frame->GetBrotherView(this);
	else
		return NULL;
}

void CBZView::OnByteOrder(UINT nID) 
{
	// TODO: Add your command handler code here
	if(nID != (UINT)options.bByteOrder + ID_BYTEORDER_INTEL) {
		options.bByteOrder = !options.bByteOrder;
		Invalidate(FALSE);
	}
	
	GetMainFrame()->UpdateInspectViewChecks();
}

void CBZView::OnUpdateByteOrder(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	pCmdUI->SetRadio(pCmdUI->m_nID == (UINT)options.bByteOrder + ID_BYTEORDER_INTEL);
}

/////////////////////////////////////////////////////////////////////////////
// CBZView Character code

// TODO: IsMBS is too slow (without pTop=max(...)), stay limit or cache it.
BOOL CBZView::IsMBS(LPBYTE pTop, DWORD ofs, BOOL bTrail)
{
	LPBYTE p, p1;
	p = p1 = pTop+ofs;
	pTop = max(pTop, pTop+ofs-16);
	while(pTop < p) {
		if(*(p-1) < 0x81) break;
		p--;
	}
	return bTrail ? _ismbstrail(p, p1) : _ismbslead(p, p1);
}

void CBZView::InitCharMode(LPBYTE pTop, DWORD ofs)
{
	if(!pTop) return;
	DWORD n = ofs;
	LPBYTE p = pTop + n;
	if(m_charset == CTYPE_JIS) {
		GetCharCode(0x0F);
		while(n) {
			n--;
			BYTE c = *(--p);
			if(c < 0x21 || c > 0x7E) break;
		}
		while(n++ < ofs) {
			GetCharCode(*p++);
		}
	} else if(m_charset == CTYPE_EUC) {
		GetCharCode(0);
		while(n) {
			n--;
			BYTE c = *(--p);
			if(c < 0xA1 || c > 0xFE) break;
		}
		while(n++ < ofs) {
			GetCharCode(*p++);
		}
	}
}

WORD CBZView::GetCharCode(WORD c, DWORD ofs)
{
	enum CharMode { CMODE_ASCII, CMODE_ESC, CMODE_ESC1, CMODE_ESC2, CMODE_ESC3, CMODE_KANJI1, CMODE_KANJI2, CMODE_KANA };
	static CharMode mode;
	static WORD c0;
	if(m_charset == CTYPE_JIS) {
		switch(c) {
		case 0x1B:
			mode = CMODE_ESC;
			break;
		case 0x0E:
			mode = CMODE_KANA;
			break;
		case 0x0F:
			mode = CMODE_ASCII;
			break;
		}
		if(c < 0x21 || c > 0x7E || mode == CMODE_ASCII) return c;
		switch(mode) {
		case CMODE_ESC:
			if(c == '(') mode = CMODE_ESC1;
			else if(c == '$') mode = CMODE_ESC2;
			else mode = CMODE_ASCII;
			break;
		case CMODE_ESC1:
			if(c == 'I') mode = CMODE_KANA;
			else mode = CMODE_ASCII;
			break;
		case CMODE_ESC2:
			mode = CMODE_KANJI1;
			break;
		case CMODE_KANJI1:
			c <<= 8;
			c0 = c;
			mode = CMODE_KANJI2;
			break;
		case CMODE_KANJI2:
			c = ' ';
			mode = CMODE_KANJI1;
			break;
		case CMODE_KANA:
			c |= 0x80;
			break;
		}
	} else if(m_charset == CTYPE_EUC) {
		if(c < 0x80) {
			mode = CMODE_ASCII;
		} else {
			switch(mode) {
			case CMODE_ASCII:
				if(c >= 0xA1 && c <= 0xFE) {
					mode = CMODE_KANJI1;
					c <<= 8;
				} else if(c == 0x8E) {
					mode = CMODE_KANA;
					c = CHAR_NG;
				} else if(c > 0x7E)
					c = CHAR_NG;
				break;
			case CMODE_KANJI1:
				c = ' ';
				mode = CMODE_ASCII;
				break;
			case CMODE_KANA:
				mode = CMODE_ASCII;
				break;
			}
		}
	} else if(m_charset == CTYPE_EPWING) {
		if(c == 0x1F) mode = CMODE_ESC3;
		else if(mode == CMODE_ESC3) {
			mode = CMODE_KANJI1;
			c = CHAR_NG;
		} else if(c < 0x21 || c > 0x7E) {
			mode = CMODE_KANJI1;
			c = CHAR_NG;
		} else if(mode == CMODE_KANJI1) {
			if(ofs == 0 || (ofs & 1) == 0) {
				c <<= 8;
				c0 = c;
				mode = CMODE_KANJI2;
			} else
				c = CHAR_NG;
		} else if(CMODE_KANJI2) {
			c = ' ';
			mode = CMODE_KANJI1;
		}
	}
	return c;
}

// WCHARな1文字をUTF-8な1文字にする。
// 第2引数、WCHARと書きたいところだけど、周りと合わせてWORDにしておく。
// TODO: サロゲートペア対応 (4バイトUTF-8パターン含む) (Note: U+10FFFFまでなら5〜6バイトまで行かない。)
// TODO: BZView.cppよりもっとユーティリティ側に追い出す。それを考慮して、あえてstaticを付けていない。
// TODO: ASCII側入力もこれを使うようにする。 出力側は? (最後の文字が欠ける)
int convertWCHARtoUTF8(LPBYTE buffer, WORD w)
{
	if(w < 0x80) {
		*buffer = (BYTE)w;
		return 1;
	} else if(w < 0x800) {
		*buffer = (BYTE)((w >> 6) | 0xC0);
		*(buffer + 1) = (BYTE)((w & 0x3F) | 0x80);
		return 2;
	} else {
		*buffer = (BYTE)((w >> 12) & 0x0F | 0xE0);
		*(buffer + 1) = (BYTE)((w >> 6) & 0x3F | 0x80);
		*(buffer + 2) = (BYTE)(w & 0x3F | 0x80);
		return 3;
	}
}

// TODO: MBCSからしか変換できないので、Unicodeビルド時にUnicode入力から変換する時には使えない。関数名を改めるべき。
// SJISからSJISに変換しようとすると、JISになる。その時は「そもそも呼ばない」という運用でカバー。
int CBZView::ConvertCharSet(CharSet charset, LPCSTR sFind, LPBYTE &buffer)
{
	int nFind = 0;
	if(charset == CTYPE_UNICODE || charset == CTYPE_UTF8) {
		nFind = ::MultiByteToWideChar(CP_ACP, 0, sFind, -1, NULL, 0);
		if(nFind) {
			buffer = (LPBYTE)MemAlloc(nFind * sizeof(WORD));
			nFind = ::MultiByteToWideChar(CP_ACP, 0, sFind, -1, (LPWSTR)buffer, nFind);
			if(nFind) nFind--;
			if(charset == CTYPE_UTF8) {			// ### 1.54b
				const int nFindUtf8 = ::WideCharToMultiByte(CP_UTF8, 0, (LPWSTR)buffer, nFind, NULL, 0, 0, 0);
				char *bufferNew = (char *)MemAlloc(nFindUtf8*2);
				::WideCharToMultiByte(CP_UTF8, 0, (LPWSTR)buffer, nFind, bufferNew, nFindUtf8*2, 0, 0);
				MemFree(buffer);
				buffer = (LPBYTE)bufferNew;
				nFind = nFindUtf8;
			}
		}
	} else {
		buffer = (LPBYTE)MemAlloc((strlen(sFind) + 1) * sizeof(WORD));
		BYTE *p = (LPBYTE)sFind;
		BYTE *q = buffer;
		BYTE c;
		while(c = *p++) {
			if(_ismbblead(c)) {
				if(charset == CTYPE_EBCDIC) {
					*q++ = CHAR_EBCDIC_NG;
					c = *p++;
					if(!c) break;
					*q++ = CHAR_EBCDIC_NG;
					continue;
				}
				WORD cjis = _mbcjmstojis(MAKEWORD(*p++, c));
				if(cjis) {
					if(charset == CTYPE_EUC) cjis |= 0x8080;
					*q++ = HIBYTE(cjis);
					*q++ = LOBYTE(cjis);
				}
			} else {
				if(charset == CTYPE_EBCDIC) {	// ### 1.63
					c = ConvertEbcDic(c);
					if(c == 0) continue;
				} else if(charset == CTYPE_EUC && c >= 0xA1 && c <= 0xDF) *q++ = 0x8E;
				*q++ = c;
			}
		}
		*q++ = '\0';
		nFind = q - buffer - 1;
	}
	return nFind;
}

CharSet CBZView::AutoDetectCharSet()
{
	LPBYTE p  = m_pDoc->GetDocPtr();
	if(!p) return options.charset;
	DWORD dwSize = m_dwTotal;
	if(dwSize > options.dwDetectMax) dwSize = options.dwDetectMax;
	CharSet charset = DetectCodeType(0, dwSize);//(p, p + dwSize);
	if(charset == CTYPE_BINARY)
		charset = options.charset;
	return charset;
}

#define DT_SJIS	1
#define DT_JIS  2
#define DT_UNICODE 4
#define DT_EUC 8
#define DT_UTF8 16		// ### 1.54b

CharSet CBZView::DetectCodeType(DWORD dwStart, DWORD dwMaxSize)//(LPBYTE p, LPBYTE pEnd)
{
	LPBYTE p = m_pDoc->QueryMapViewTama2(dwStart, dwMaxSize); //LPBYTE p  = m_pDoc->GetDocPtr();
	DWORD dwRemain = m_pDoc->GetMapRemain(dwStart);
	if(!p || dwRemain < 2)return CTYPE_BINARY;

	if(*(WORD*)p == 0xFEFF) {
		options.bByteOrder = FALSE;		// ### 1.54a
		return CTYPE_UNICODE;
	}
	if(*(WORD*)p == 0xFFFE) {
		options.bByteOrder = TRUE;
		return CTYPE_UNICODE;
	}
	if(*(WORD*)p == 0xBBEF && dwRemain > 3 && *(p+2) == 0xBF) {
		return CTYPE_UTF8;
	}

	DWORD dwCurrent = dwStart;
	DWORD flag = DT_SJIS | DT_JIS | DT_EUC | DT_UTF8;
	while(dwCurrent < dwStart + dwMaxSize)//(p < pEnd)
	{
		p = m_pDoc->QueryMapViewTama2(dwCurrent, 2);
		dwRemain = m_pDoc->GetMapRemain(dwCurrent);
		BYTE c = (BYTE)*p++; dwCurrent++;//BYTE c = (BYTE)*p++;
		if(c == '\n') {
			if(flag == DT_SJIS || flag == DT_EUC || flag & DT_UNICODE) break;
		}
		if(c >= 0x80) {
			flag &= ~DT_JIS;
			if(flag & DT_EUC) {
				if(c != 0x8E && (c < 0xA1 || c > 0xFE))
					flag &= ~DT_EUC;
			}
			if(flag & DT_UTF8) {	// ### 1.54b
				if(c >= 0xC0)
				{
					if     ((c & 0xE0) == 0xC0 && dwRemain >= 2) { //UTF-8: 110yyy yx, 10xx xxxx
						if((*p & 0xC0/*1100 0000*/) != 0x80/*1000 0000*/) // check 10xx xxxx
							flag &= ~DT_UTF8;
					}
					else if((c & 0xF0) == 0xE0 && dwRemain >= 3) { //UTF-8: 1110yyyy, 10yxxx xx, 10xx xxxx
						if((*p & 0xC0/*1100 0000*/) != 0x80/*1000 0000*/ || (*(p+1) & 0xC0/*1100 0000*/) != 0x80/*1000 0000*/) // check 10xxxxxx
							flag &= ~DT_UTF8;
					}
					else if((c & 0xF8) == 0xF0 && dwRemain >= 4) { //UTF-8: 11110y yy, 10yy xxxx, 10xxxx xx, 10xx xxxx
						if((*p & 0xC0/*1100 0000*/) != 0x80/*1000 0000*/ || (*(p+1) & 0xC0/*1100 0000*/) != 0x80/*1000 0000*/ || (*(p+2) & 0xC0/*1100 0000*/) != 0x80/*1000 0000*/) // check 10xxxxxx
							flag &= ~DT_UTF8;
					}
				}
				else if(flag == (DT_UTF8 | DT_SJIS) && dwRemain >= 5) // c == [0x80-0xBF]
				{
					int i=0;
					for(;i < 3;i++)
					{
						if((*(p+i) & 0xC0) != 0x80)break;
					}
					if(i==3 || ((*(p+i)) >= 0x80 && (*(p+i)) < 0xC0))
					{
						return CTYPE_SJIS;//flag &= ~CTYPE_SJIS;
					}
				}
			}
		}
		if((c >= 0x08 && c <= 0x0D) || (c >= 0x20 && c <= 0x7E) || (c >= 0x81 && c <= 0xFC)) {
			if(_ismbblead(c)) {
				c = *p++; dwCurrent++;//c = (BYTE)*p++;
				if(!_ismbbtrail(c)) {
					flag &= ~DT_SJIS;
					if((c == 0 || c == 0xFF) && ((DWORD)p & 1) == 0)
						flag |= DT_UNICODE;
				}
				if(c >= 0x80 && c != 0x8E && (c < 0xA1 || c > 0xFE))
					flag &= ~DT_EUC;
			}
		} else {
			if(c == 0x1B) {
				if(*p == '$' || *p == '(') 
					return CTYPE_JIS;
			} else if((c == 0 || c == 0xFF) && ((DWORD)p & 1) == 0)
				flag |= DT_UNICODE;
			else if(c < 0x20)
				return CTYPE_BINARY;
		}
		if(flag == 0) return CTYPE_BINARY;
	}
	return (flag & DT_UNICODE) ? CTYPE_UNICODE
		 : (flag & DT_JIS) ? CTYPE_BINARY		// ### 1.54b
		 : (flag & DT_EUC) ? CTYPE_EUC : (flag & DT_UTF8) ? CTYPE_UTF8 : CTYPE_SJIS;
}

/////////////////////////////////////////////////////////////////////////////
// EBCDIC ###1.63

BOOL CBZView::LoadEbcDicTable()
{
	if(m_bLoadEbcDic) return m_pEbcDic != NULL;
	m_bLoadEbcDic = TRUE;
	CString sPath;
	sPath = GetStructFilePath(IDS_EBCDIC_FILE);
	LPSTR pTable = (LPSTR)ReadFile(sPath);
	if(!pTable) return FALSE;
	LPSTR p = pTable;
	m_pEbcDic = new char[EBCDIC_COUNT + 1];
	memset(m_pEbcDic, CHAR_NG, EBCDIC_COUNT);
	LPSTR q = m_pEbcDic;
	while(*p &&	q < m_pEbcDic + EBCDIC_COUNT) {
		if(*p == ';') {
			while(*p++ != '\n') ;
			continue;
		}
		while(*p++ != '\t') ;
		for_to(i, 16)
			*q++ = *p++;
		while(*p++ != '\n') ;
	}
	*q++ = '\0';
	delete pTable;
	return TRUE;
}

UCHAR CBZView::ConvertEbcDic(UCHAR c)
{
	if(m_pEbcDic) {
		LPSTR p = strchr(m_pEbcDic, c);
		if(p)
			return p - m_pEbcDic + EBCDIC_BASE;
	}
	return 	CHAR_EBCDIC_NG;
}

void CBZView::OnJumpBase() 
{
	// TODO: Add your command handler code here
	CInputDlg dlg;
	dlg.m_sValue.Format(_T("%X"), m_pDoc->m_dwBase);
	if(dlg.DoModal() == IDOK) {
		m_pDoc->m_dwBase = _tcstol(dlg.m_sValue, NULL, 16);
		Invalidate(FALSE);
	}
}

void CBZView::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	// TODO: Add your message handler code here and/or call default
	TRACE("KeyUp: %X %d %X\n", nChar, nRepCnt, nFlags);
	CTextView::OnKeyUp(nChar, nRepCnt, nFlags);
}

void CBZView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	// TODO: ここにメッセージ ハンドラ コードを追加するか、既定の処理を呼び出します。

	CTextView::OnVScroll(nSBCode, nPos, pScrollBar);

	CBZView *pActiveView = (CBZView*)(GetMainFrame()->GetActiveView());
	if(options.bSyncScroll && pActiveView==this)
	{
		CBZView* pView1 = GetBrotherView();
		if(pView1)
		{
			POINT pt = GetScrollPos();
			pView1->ScrollToPos(pt);
			pView1->Invalidate();
			//pView1->OnVScroll(nSBCode, nPos, NULL);
	//		TRACE("OnVScroll: m_dwCaret=%d\n", m_dwCaret);
		}
	}
}

BOOL CBZView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	// TODO: ここにメッセージ ハンドラ コードを追加するか、既定の処理を呼び出します。

	BOOL ret = CTextView::OnMouseWheel(nFlags, zDelta, pt);

	
	CBZView *pActiveView = (CBZView*)(GetMainFrame()->GetActiveView());
	if(options.bSyncScroll && pActiveView==this)
	{
		CBZView* pView1 = GetBrotherView();
		if(pView1)
		{
			POINT pt = GetScrollPos();
			pView1->ScrollToPos(pt);
			pView1->Invalidate();
			//pView1->OnVScroll(nSBCode, nPos, NULL);
		//	TRACE("OnVScroll: m_dwCaret=%d\n", m_dwCaret);
		}
	}

	return ret;
}

void CBZView::OnViewGrid1()
{
	// TODO: ここにコマンド ハンドラ コードを追加します。
	options.iGrid = (options.iGrid==0)?1:0;
	Invalidate();
}

void CBZView::OnUpdateViewGrid1(CCmdUI *pCmdUI)
{
	// TODO: ここにコマンド更新 UI ハンドラ コードを追加します。
	pCmdUI->SetCheck(options.iGrid==1);
}

void CBZView::ReCreateBackup()
{
	if(m_pDoc)
	{
		m_pDoc->m_restoreCaret = m_dwCaret;
		m_pDoc->m_restoredwBlock = m_dwBlock;
		m_pDoc->m_restorebBlock = m_bBlock;
		m_pDoc->m_restoreScroll = this->GetScrollPos();
		m_pDoc->m_restoreScroll.x = 0;
	}
}
void CBZView::ReCreateRestore()
{
	if(m_pDoc)
	{
		m_dwCaret = m_pDoc->m_restoreCaret;
		m_dwBlock = m_pDoc->m_restoredwBlock;
		m_bBlock = m_pDoc->m_restorebBlock;
		DrawCaret();
		ScrollToPos(m_pDoc->m_restoreScroll);
	}
}




// Quick Search Algorithm
void CBZView::preQuickSearchWI1(LPCWSTR searchTextW, BYTE nSearchTextW, BYTE *skipTable)
{
	for(int j=0;j<0x10000;j++)skipTable[j] = nSearchTextW+1;
	WORD wc;
	for(DWORD i=0;i<nSearchTextW;i++)
	{
		wc = searchTextW[i];
		skipTable[wc] = nSearchTextW - i;
		if('A' <= wc && wc <= 'Z')		skipTable[wc+0x20] = nSearchTextW - i;
		else if('a' <= wc && wc <= 'z')	skipTable[wc-0x20] = nSearchTextW - i;
	}
}
DWORD CBZView::stristrBinaryW1(LPCWSTR searchTextW, BYTE nSearchTextW, DWORD dwStart)
{
	BYTE skipTable[0x10000];
	preQuickSearchWI1(searchTextW, nSearchTextW, skipTable);

	DWORD dwNeedSize = (nSearchTextW+1)*2;
	DWORD dwCurrent=dwStart;
	while(dwCurrent < m_dwTotal-2)
	{
		LPWORD p = (LPWORD)m_pDoc->QueryMapViewTama2(dwCurrent, dwNeedSize);
		if(!p || m_pDoc->GetMapRemain(dwCurrent) < dwNeedSize)break;//err
		if(wcsnicmp(searchTextW, (const wchar_t*)p, nSearchTextW)==0)return dwCurrent;
		dwCurrent += skipTable[p[nSearchTextW]]*2;
	}
	return 0xFFFFffff; // error
}
void CBZView::preQuickSearchWI4(LPCWSTR searchTextW, DWORD nSearchTextW, DWORD *skipTable)
{
	for(int j=0;j<0x10000;j++)skipTable[j] = nSearchTextW+1;
	WORD wc;
	for(DWORD i=0;i<nSearchTextW;i++)
	{
		wc = searchTextW[i];
		skipTable[wc] = nSearchTextW - i;
		if('A' <= wc && wc <= 'Z')		skipTable[wc+0x20] = nSearchTextW - i;
		else if('a' <= wc && wc <= 'z')	skipTable[wc-0x20] = nSearchTextW - i;
	}
}
DWORD CBZView::stristrBinaryW4(LPCWSTR searchTextW, DWORD nSearchTextW, DWORD dwStart)
{
	DWORD skipTable[0x10000];
	preQuickSearchWI4(searchTextW, nSearchTextW, skipTable);

	DWORD dwNeedSize = (nSearchTextW+1)*2;
	DWORD dwCurrent=dwStart;
	while(dwCurrent < m_dwTotal-2)
	{
		LPWORD p = (LPWORD)m_pDoc->QueryMapViewTama2(dwCurrent, dwNeedSize);
		if(!p || m_pDoc->GetMapRemain(dwCurrent) < dwNeedSize)break;//err
		if(wcsnicmp(searchTextW, (const wchar_t*)p, nSearchTextW)==0)return dwCurrent;
		dwCurrent += skipTable[p[nSearchTextW]]*2;
	}
	return 0xFFFFffff; // error
}
DWORD CBZView::stristrBinaryW(LPCWSTR searchTextW, DWORD nSearchTextW, DWORD dwStart)
{
	if(nSearchTextW < 0xFF)
		return stristrBinaryW1(searchTextW, nSearchTextW, dwStart);
	else
		return stristrBinaryW4(searchTextW, nSearchTextW, dwStart);
}
void CBZView::preQuickSearchAI(LPCSTR searchText, DWORD nSearchText, DWORD *skipTable)
{
	for(int j=0;j<0x100;j++)skipTable[j] = nSearchText+1;
	BYTE ch;
	for(DWORD i=0;i<nSearchText;i++)
	{
		ch = searchText[i];
		skipTable[ch] = nSearchText - i;
		if('A' <= ch && ch <= 'Z')		skipTable[ch+0x20] = nSearchText - i;
		else if('a' <= ch && ch <= 'z')	skipTable[ch-0x20] = nSearchText - i;
	}
}
DWORD CBZView::stristrBinaryA(LPCSTR searchText, DWORD dwStart)
{
	DWORD nSearchText = strlen(searchText);
	DWORD skipTable[0x100];
	preQuickSearchAI(searchText, nSearchText, skipTable);
	DWORD dwCurrent=dwStart;
	while(dwCurrent < m_dwTotal-1)
	{
		LPBYTE p = m_pDoc->QueryMapViewTama2(dwCurrent, nSearchText+1);
		if(!p || m_pDoc->GetMapRemain(dwCurrent) < nSearchText+1)break;//err
		if(strnicmp(searchText, (const char*)p, nSearchText)==0)return dwCurrent;
		dwCurrent += skipTable[p[nSearchText]];
	}
	return 0xFFFFffff; // error
}
void CBZView::preQuickSearch(LPBYTE searchByte, unsigned int nSearchByte, DWORD* skipTable)
{
	for(int j=0;j<0x100;j++)skipTable[j] = nSearchByte+1;
	for(DWORD i=0;i<nSearchByte;i++)skipTable[searchByte[i]] = nSearchByte - i;
}
DWORD CBZView::strstrBinary(LPBYTE searchByte, unsigned int nSearchByte, DWORD dwStart)
{
	DWORD skipTable[0x100];
	preQuickSearch(searchByte, nSearchByte, skipTable);
	DWORD dwCurrent=dwStart;
	while(dwCurrent < m_dwTotal-1)
	{
		LPBYTE p = m_pDoc->QueryMapViewTama2(dwCurrent, nSearchByte+1);
		if(!p || m_pDoc->GetMapRemain(dwCurrent) < nSearchByte+1)break;//err
		if(memcmp(searchByte, p, nSearchByte)==0)return dwCurrent;
		dwCurrent += skipTable[p[nSearchByte]];
	}
	return 0xFFFFffff; // error
}