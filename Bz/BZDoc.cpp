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
#include "Mainfrm.h"
#include "zlib.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#ifdef FILE_MAPPING
  //#define FILEOFFSET_MASK 0xFFF00000	// by 1MB
  #define MAX_FILELENGTH  0xFFFFFFF0
#endif //FILE_MAPPING

/////////////////////////////////////////////////////////////////////////////
// CBZDoc

IMPLEMENT_DYNCREATE(CBZDoc, CDocument)

BEGIN_MESSAGE_MAP(CBZDoc, CDocument)
	//{{AFX_MSG_MAP(CBZDoc)
	ON_COMMAND(ID_EDIT_READONLY, OnEditReadOnly)
	ON_UPDATE_COMMAND_UI(ID_EDIT_READONLY, OnUpdateEditReadOnly)
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateEditUndo)
	ON_COMMAND(ID_EDIT_READONLYOPEN, OnEditReadOnlyOpen)
	ON_UPDATE_COMMAND_UI(ID_EDIT_READONLYOPEN, OnUpdateEditReadOnlyOpen)
	//}}AFX_MSG_MAP
	ON_COMMAND(ID_INDICATOR_INS, OnEditReadOnly)
	ON_UPDATE_COMMAND_UI_RANGE(ID_FILE_SAVE, ID_FILE_SAVE_AS, OnUpdateFileSave)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBZDoc construction/destruction

CBZDoc::CBZDoc() : m_restoreScroll()
{
	// TODO: add one-time construction code here
	m_pData = NULL;
	m_dwTotal = 0;
	m_bReadOnly = TRUE;
#ifdef FILE_MAPPING
	m_hMapping = NULL;
	m_pFileMapping = NULL;
	m_pDupDoc = NULL;
	m_pMapStart = NULL;
	m_dwFileOffset = 0;
	m_dwMapSize = 0;
#endif //FILE_MAPPING
	m_dwBase = 0;

	//Undo
	m_pUndo = NULL;
	m_dwUndo = 0;
	m_dwUndoSaved = 0;

	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	m_dwAllocationGranularity = sysinfo.dwAllocationGranularity;

	//ReCreate restore
	m_restoreCaret = 0;
	//m_restoreScroll = {0};
}

CBZDoc::~CBZDoc()
{
//	TRACE("DestructDoc:%X\n",this);
}

LPBYTE	CBZDoc::GetDocPtr()
{
	return m_pData;
}

void CBZDoc::DeleteContents() 
{
	// TODO: Add your specialized code here and/or call the base class

	if(m_pData) {
#ifdef FILE_MAPPING
		if(IsFileMapping()) {
			VERIFY(::UnmapViewOfFile(m_pMapStart ? m_pMapStart : m_pData));
			m_pMapStart = NULL;
			m_dwFileOffset = 0;
			m_dwMapSize = 0;
		}
		else
#endif //FILE_MAPPING
			MemFree(m_pData);
		m_pData = NULL;
		m_dwTotal = 0;
		m_dwBase = 0;
		UpdateAllViews(NULL);
	}
#ifdef FILE_MAPPING
	if(IsFileMapping()) {
		if(m_pDupDoc) {
			m_pDupDoc->m_pDupDoc = NULL;
			m_pDupDoc = NULL;
			m_hMapping = NULL;
			m_pFileMapping = NULL;
		} else {
			VERIFY(::CloseHandle(m_hMapping));
			m_hMapping = NULL;
			if(m_pFileMapping) {
				ReleaseFile(m_pFileMapping, FALSE);
				m_pFileMapping = NULL;
			}
		}
	}
#endif //FILE_MAPPING

	if(m_pUndo) {
		MemFree(m_pUndo);
		m_pUndo = NULL;
	}	
	m_bReadOnly = FALSE;
	m_arrMarks.RemoveAll();
	CDocument::DeleteContents();
}

/////////////////////////////////////////////////////////////////////////////
// CBZDoc serialization

void CBZDoc::Serialize(CArchive& ar)
{
	MEMORYSTATUS ms;
	GlobalMemoryStatus(&ms);

	CFile *pFile = ar.GetFile();
	ar.Flush();

	if (ar.IsLoading())	{
		// TODO: add loading code here
		m_dwTotal = GetFileLength(pFile);
#ifdef FILE_MAPPING
		if(IsFileMapping()) {
			if(!MapView()) return;
		} else
#endif //FILE_MAPPING
		{
			if(!(m_pData = (LPBYTE)MemAlloc(m_dwTotal))) {
				AfxMessageBox(IDS_ERR_MALLOC);
				return;
			}
			DWORD len = pFile->Read(m_pData, m_dwTotal);
			if(len < m_dwTotal) {
				AfxMessageBox(IDS_ERR_READFILE);
				MemFree(m_pData);	// ###1.61
				m_pData = NULL;
				return;
			}
			m_bReadOnly = options.bReadOnlyOpen;
		}
	} else {
		// TODO: add storing code here
#ifdef FILE_MAPPING
		if(IsFileMapping()) {
			BOOL bResult = (m_pMapStart) ? ::FlushViewOfFile(m_pMapStart, m_dwMapSize) : ::FlushViewOfFile(m_pData, m_dwTotal);
			if(!bResult) {
				ErrorMessageBox();
			}
		} else
#endif //FILE_MAPPING
			pFile->Write(m_pData, m_dwTotal);
		m_dwUndoSaved = m_dwUndo;		// ###1.54
		TouchDoc();
/*		if(m_pUndo) {
			MemFree(m_pUndo);
			m_pUndo = NULL;
		}	
*/	}
}

void CBZDoc::SavePartial(CFile& file, DWORD offset, DWORD size)
{
#ifdef FILE_MAPPING
	if(IsFileMapping()) {
		// QueryMapViewは使いっぱなしで問題ない。(draw時などにQueryMapViewしなおしてくれる。)
		while(size > 0) {
			LPBYTE pData = QueryMapViewTama2(offset, size); // 書き込みたいデータを見えるようにする。
			DWORD writesize = min(GetMapRemain(offset), size); // 書き込めるだけのデータ量を見積もる。
			file.Write(pData, writesize); // 書き込む。

			// 書き込めた分、offset進めてsize減らす。
			offset += writesize;
			size -= writesize;
		}
	} else
#endif
		file.Write(m_pData + offset, size);
}

static int inflateBlock(CFile& file, LPBYTE buf, const DWORD bufsize, z_stream& z, LPBYTE pData, DWORD dataSize) {
	int ret;

	z.next_in = pData;
	z.avail_in = dataSize;

	do {
		z.next_out = buf;
		z.avail_out = bufsize;
		ret = inflate(&z, Z_NO_FLUSH);

		// 展開後のデータを書き込む。
		// エラーでもできた分は書く。
		file.Write(buf, bufsize - z.avail_out);

		if(ret != Z_OK && ret != Z_STREAM_END) {
			// Error! 即リターン。
			return ret;
		}
	} while(z.avail_out == 0);

	return ret;
}

void CBZDoc::SavePartialInflated(CFile& file, DWORD offset, DWORD size, CBZView& view)
{
	z_stream z = {0};
	const DWORD bufsize = 32768; // TODO このサイズで大丈夫か?
	BYTE *buf = new BYTE[bufsize];
	int ret;

	if(inflateInit(&z) != Z_OK) {
		AfxMessageBox(IDS_ERR_INFLATEINIT, MB_OK | MB_ICONERROR);
		return;
	}

#ifdef FILE_MAPPING
	if(IsFileMapping()) {
		// QueryMapViewは使いっぱなしで問題ない。(draw時などにQueryMapViewしなおしてくれる。)
		while(size > 0) {
			LPBYTE pData = QueryMapViewTama2(offset, size); // 展開したいデータを見えるようにする。
			DWORD dataSize = min(GetMapRemain(offset), size); // 展開できるだけのデータ量を見積もる。
			ret = inflateBlock(file, buf, bufsize, z, pData, dataSize);

			if(ret != Z_OK) { // Z_STREAM_END もしくはエラー時はループを抜ける。
				// offset/sizeをz.avail_inから再計算する。どれくらい余分であったかがわかる。
				offset += dataSize - z.avail_in; // 意味ないけど一応offsetも調整しておく…
				size -= dataSize - z.avail_in;
				break;
			}

			// 展開できた分、offset進めてsize減らす。
			offset += dataSize;
			size -= dataSize;
		}
	} else
#endif
	{
		ret = inflateBlock(file, buf, bufsize, z, m_pData + offset, size);
		// offset/sizeをz.avail_inから再計算する。どれくらい余分であったかがわかる。
		offset += size - z.avail_in; // 意味ないけど一応offsetも調整しておく…
		size = z.avail_in;
	}

	if(ret == Z_STREAM_END) {
		if(size > 0) {
			// 余ったバイト数を表示。
			CString sMsg;
			sMsg.Format(IDS_INFLATE_REMAIN_BYTES, size);
			if(AfxMessageBox(sMsg, MB_YESNO | MB_ICONQUESTION) == IDYES) {
				// 既に選択している範囲が正しいという仮定。(実際正しくないとそれはバグ。)
				view.setBlock(view.BlockBegin(), view.BlockEnd() - size);
			}
		} else {
			/* do nothing */
		}
	} else if(ret == Z_OK) {
		AfxMessageBox(IDS_ERR_INFLATE_RESULT_OK, MB_OK | MB_ICONERROR);
	} else {
		AfxMessageBox(IDS_ERR_INFLATE_RESULT_ERROR, MB_OK | MB_ICONERROR);
	}

	inflateEnd(&z); // 念のため、z.avail_inを参照などした後にinflateEndする。
	delete buf;
}

#ifdef FILE_MAPPING

BOOL CBZDoc::MapView()
{
	m_dwMapSize = m_dwTotal;
	m_pData = (LPBYTE)::MapViewOfFile(m_hMapping, m_bReadOnly ? FILE_MAP_READ : FILE_MAP_WRITE, 0, 0, m_dwMapSize);
	if(!m_pData) {
		m_dwMapSize = options.dwMaxMapSize;
		m_pData = (LPBYTE)::MapViewOfFile(m_hMapping, m_bReadOnly ? FILE_MAP_READ : FILE_MAP_WRITE, 0, 0, m_dwMapSize);
		if(m_pData) {
			m_pMapStart = m_pData;
			m_dwFileOffset = 0;
		}
		else {
			ErrorMessageBox();
			AfxThrowMemoryException();	// ###1.61
			return FALSE;
		}
	}
	return TRUE;
}

LPBYTE CBZDoc::QueryMapView1(LPBYTE pBegin, DWORD dwOffset)
{
	TRACE("QueryMapView1 m_pData:%X, pBegin:%X, dwOffset:%X\n", m_pData, pBegin, dwOffset);
	LPBYTE p = pBegin + dwOffset;
	//if(IsOutOfMap1(p)) {QueryMapView()内に移動
		if(p == m_pData + m_dwTotal && p == m_pMapStart + m_dwMapSize) return pBegin;	// ###1.61a
		DWORD dwBegin = GetFileOffsetFromFileMappingPointer(pBegin);//DWORD dwBegin = pBegin - m_pData;
		VERIFY(::UnmapViewOfFile(m_pMapStart));//ここで書き込まれちゃうけどOK?
		//m_dwFileOffset = GetFileOffsetFromFileMappingPointer(p)/*(p - m_pDate)*/ & FILEOFFSET_MASK;
		DWORD dwTmp1 = GetFileOffsetFromFileMappingPointer(p);
		m_dwFileOffset = dwTmp1 - (dwTmp1 % m_dwAllocationGranularity);
		m_dwMapSize = min(options.dwMaxMapSize, m_dwTotal - m_dwFileOffset); //どうもここが引っかかる。全領域をマッピングできる？CBZDoc::MapView()のブロック２をコメントアウトしたほうがいいかも。ただ32ビット＆4GBオーバー対応の際に問題になりそう by tamachan(20120907)
		if(m_dwMapSize == 0) {	// ###1.61a
			//m_dwFileOffset = (m_dwTotal - (~FILEOFFSET_MASK + 1)) & FILEOFFSET_MASK;
			dwTmp1 = (m_dwTotal - m_dwAllocationGranularity);
			m_dwFileOffset = dwTmp1 - (dwTmp1 % m_dwAllocationGranularity);
			m_dwMapSize = m_dwTotal - m_dwFileOffset;
		}
		int retry = 3;
		m_pMapStart = NULL;
		do
		{
			m_pMapStart = (LPBYTE)::MapViewOfFile(m_hMapping, m_bReadOnly ? FILE_MAP_READ : FILE_MAP_WRITE, 0, m_dwFileOffset, m_dwMapSize);
		} while(m_pMapStart==NULL && --retry > 0);
		TRACE("MapViewOfFile Doc=%X, %X, Offset:%X, Size:%X\n", this, m_pMapStart, m_dwFileOffset, m_dwMapSize);
		if(!m_pMapStart) {
			ErrorMessageBox();
			AfxPostQuitMessage(0);
			return NULL;
		}
		m_pData = m_pMapStart - m_dwFileOffset; //バグ?：仮想的なアドレス（ファイルのオフセット0にあたるメモリアドレス）を作り出している。m_pMapStart<m_dwFileOffsetだった場合、0を割ることがあるのではないだろうか？そういった場合まずい？？IsOutOfMapは正常に動きそう？ by tamachan(20121004)
//		ASSERT(m_pMapStart > m_pData);
		pBegin = GetFileMappingPointerFromFileOffset(dwBegin);//pBegin = m_pData + dwBegin;
	//}
	return pBegin;
}

void CBZDoc::AlignMapSize(DWORD dwStartOffset, DWORD dwIdealMapSize)
{
	m_dwFileOffset = dwStartOffset - (dwStartOffset % m_dwAllocationGranularity);
	m_dwMapSize = min(max(options.dwMaxMapSize, dwIdealMapSize),  m_dwTotal - m_dwFileOffset);
	if(m_dwMapSize == 0)
	{
		DWORD dwTmp1 = (m_dwTotal - m_dwAllocationGranularity);
		m_dwFileOffset = dwTmp1 - (dwTmp1 % m_dwAllocationGranularity);
		m_dwMapSize = m_dwTotal - m_dwFileOffset;
	}//バグ：ファイルサイズが極端に小さい場合バグる
}
LPBYTE CBZDoc::_QueryMapViewTama2(DWORD dwStartOffset, DWORD dwIdealMapSize)
{
	if(dwStartOffset == m_dwTotal && dwStartOffset == m_dwFileOffset + m_dwMapSize) return GetFileMappingPointerFromFileOffset(dwStartOffset);//バグ？そもそもここに来るのはおかしいのではないか？
	VERIFY(::UnmapViewOfFile(m_pMapStart));//ここでマッピング空間への変更が実ファイルへ書き込まれる。後に保存せず閉じる場合はアンドゥで戻す。

	AlignMapSize(dwStartOffset, dwIdealMapSize);

	m_pMapStart = (LPBYTE)::MapViewOfFile(m_hMapping, m_bReadOnly ? FILE_MAP_READ : FILE_MAP_WRITE, 0, m_dwFileOffset, m_dwMapSize);
	TRACE("MapViewOfFile Doc=%X, %X, Offset:%X, Size:%X\n", this, m_pMapStart, m_dwFileOffset, m_dwMapSize);
	if(!m_pMapStart) {
		ErrorMessageBox();
		AfxPostQuitMessage(0);
		return NULL;
	}
	m_pData = m_pMapStart - m_dwFileOffset;
	return GetFileMappingPointerFromFileOffset(dwStartOffset);
}

BOOL CBZDoc::IsOutOfMap1(LPBYTE p)
{
	return ((int)p < (int)m_pMapStart || (int)p >= (int)(m_pMapStart + m_dwMapSize));
}

#endif //FILE_MAPPING

/////////////////////////////////////////////////////////////////////////////
// CBZDoc diagnostics

#ifdef _DEBUG
void CBZDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CBZDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CBZDoc commands


void CBZDoc::OnEditReadOnly() 
{
#ifdef FILE_MAPPING
	if(IsFileMapping()) {
		AfxMessageBox(IDS_ERR_MAP_RO);
		return;
	}
#endif //FILE_MAPPING
	m_bReadOnly = !m_bReadOnly;	
}

void CBZDoc::OnUpdateEditReadOnly(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_bReadOnly);
}

void CBZDoc::OnEditReadOnlyOpen() 
{
	// TODO: Add your command handler code here
	options.bReadOnlyOpen = !options.bReadOnlyOpen;
}

void CBZDoc::OnUpdateEditReadOnlyOpen(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	pCmdUI->SetCheck(options.bReadOnlyOpen);	
}

// scripting向けにデータ取得とクリップボード操作を分離している。
// TODO: CBZDocにある必要がない(func2に相当する部分だけCBZDocにあればよい)。
BOOL CBZDoc::DoCopyToClipboard(DWORD dwStart, DWORD dwSize, BOOL (*func2)(void*,void*,void*,DWORD,DWORD), void *param)
{
	HGLOBAL hMemTxt = ::GlobalAlloc(GMEM_MOVEABLE, dwSize + 1);
	HGLOBAL hMemBin = ::GlobalAlloc(GMEM_MOVEABLE, dwSize + sizeof(dwSize));
	LPBYTE pMemTxt  = (LPBYTE)::GlobalLock(hMemTxt);
	LPBYTE pMemBin  = (LPBYTE)::GlobalLock(hMemBin);

	if(!func2(param, pMemTxt, pMemBin + sizeof(dwSize), dwStart, dwSize))
	{
		AfxMessageBox(IDS_ERR_COPY);
		::GlobalUnlock(hMemTxt);
		::GlobalUnlock(hMemBin);
		::GlobalFree(hMemTxt);
		::GlobalFree(hMemBin);
		return FALSE;
	}
	*(pMemTxt + dwSize) = '\0';
	*((DWORD*)(pMemBin)) = dwSize;

	::GlobalUnlock(hMemTxt);
	::GlobalUnlock(hMemBin);
	AfxGetMainWnd()->OpenClipboard();
	::EmptyClipboard();
	::SetClipboardData(CF_TEXT, hMemTxt); // TODO: 選択されている文字コードに応じてエンコードしてからコピー? 場合によってはCF_UNICODETEXTもセットする。
	::SetClipboardData(RegisterClipboardFormat(_T("BinaryData2")), hMemBin);
	::CloseClipboard();
	return TRUE;
}

static BOOL cb_memcpyFilemap2Mem(void *param, void *ptr1, void *ptr2, DWORD dwStart, DWORD dwSize)
{
	CBZDoc *doc = (CBZDoc*)param;

	return doc->memcpyFilemap2Mem(ptr1, ptr2, dwStart, dwSize);
}

BOOL CBZDoc::CopyToClipboard(DWORD dwStart, DWORD dwSize)	// ###1.5
{
	return DoCopyToClipboard(dwStart, dwSize, cb_memcpyFilemap2Mem, this);
}

// TODO: CopyToClipboardのほぼコピペなので、何とかして統一したい…。
//       適当に統一するとif文だらけで見にくくなる上、merge時にconflictしやすい。
BOOL CBZDoc::CopyToClipboardWithHexalize(DWORD dwStart, DWORD dwSize)	// ###1.5
{
	HGLOBAL hMemTxt = ::GlobalAlloc(GMEM_MOVEABLE, dwSize * 2 + 1);
	LPBYTE pMemTxt  = (LPBYTE)::GlobalLock(hMemTxt);

	if(!memcpyFilemap2Mem(pMemTxt, dwStart, dwSize))
	{
		AfxMessageBox(IDS_ERR_COPY);
		::GlobalUnlock(hMemTxt);
		::GlobalFree(hMemTxt);
		return FALSE;
	}
	*(pMemTxt + dwSize * 2) = '\0';

	// 16進文字列化: (w)sprintf使うより自前でやるほうが速い。
	static const char hextab[] = "0123456789ABCDEF";
	for(int i = dwSize - 1; 0 <= i; i--)
	{
		pMemTxt[i * 2 + 1] = hextab[pMemTxt[i] % 16];
		pMemTxt[i * 2] = hextab[pMemTxt[i] / 16];
	}

	::GlobalUnlock(hMemTxt);
	AfxGetMainWnd()->OpenClipboard();
	::EmptyClipboard();
	::SetClipboardData(CF_TEXT, hMemTxt);
	::CloseClipboard();
	return TRUE;
}

DWORD CBZDoc::ClipboardReadOpen(HGLOBAL &hMem, LPBYTE &pMem, LPBYTE &pWorkMem, UINT format)
{
	AfxGetMainWnd()->OpenClipboard();
	DWORD dwSize;
	pWorkMem = NULL;
	if(format != 0 && (hMem = ::GetClipboardData(format)) != NULL) {
		pMem = (LPBYTE)::GlobalLock(hMem);
		dwSize = GlobalSize(hMem);
	} else if(hMem = ::GetClipboardData(RegisterClipboardFormat(_T("BinaryData2")))) {
		pMem = (LPBYTE)::GlobalLock(hMem);
		dwSize = *((DWORD*)(pMem));
		pMem += sizeof(DWORD);
	} else if((options.charset == CTYPE_UTF8 || options.charset == CTYPE_UNICODE) && (hMem = GetClipboardData(CF_UNICODETEXT))) {
		int nchars;

		pMem = (LPBYTE)::GlobalLock(hMem);
		nchars = lstrlenW((LPWSTR)pMem);
		dwSize = nchars * 2;

		if(options.charset == CTYPE_UTF8) {
			// UTF-8なら、UTF-16からUTF-8に変換する。
			dwSize = WideCharToMultiByte(CP_UTF8, 0, (LPCTCH)pMem, nchars, NULL, 0, NULL, NULL);
			pWorkMem = (LPBYTE)MemAlloc(dwSize);
			WideCharToMultiByte(CP_UTF8, 0, (LPCTCH)pMem, nchars, (LPSTR)pWorkMem, dwSize, NULL, NULL);

			pMem = pWorkMem;
		} else {
			// UTF-16なら、バイトオーダーを調整する。
			pWorkMem = (LPBYTE)MemAlloc(dwSize);
			for(int i = 0; i < nchars; i++) {
				((LPWORD)pWorkMem)[i] = SwapWord(((LPWORD)pMem)[i]);
			}

			pMem = pWorkMem;
		}
	} else if(hMem = GetClipboardData(CF_TEXT)) {
		pMem = (LPBYTE)::GlobalLock(hMem);
		dwSize = lstrlenA((LPCSTR)pMem);
	} else {
/*		UINT uFmt = 0;
		while(uFmt = ::EnumClipboardFormats(uFmt)) {
			CString sName;
			::GetClipboardFormatName(uFmt, sName.GetBuffer(MAX_PATH), MAX_PATH);
			sName.ReleaseBuffer();
			TRACE("clip 0x%X:%s\n", uFmt, sName);
		}

		return 0;
*/		if(!(hMem = ::GetClipboardData(::EnumClipboardFormats(0))))
			return 0;
		// HBITMAPで(デバッグ時は問題ないがリリースだと)例外発生して落ちるので、__tryで囲む。
		// TODO: HBITMAPなどハンドル系はHGLOBALではなくハンドルそのものだから死ぬ? どうやってHGLOBALかどうか確認する?
		__try
		{
			pMem = (LPBYTE)::GlobalLock(hMem);
			dwSize = ::GlobalSize(hMem);
		} __except(EXCEPTION_CONTINUE_EXECUTION)
		{
			// Do nothing
		}
	}
	return dwSize;
}

void CBZDoc::ClipboardReadClose(HGLOBAL hMem, LPBYTE pMem, LPBYTE pWorkMem)
{
	::GlobalUnlock(hMem);
	::CloseClipboard();
	if(pWorkMem) {
		MemFree(pWorkMem);
	}
}

DWORD CBZDoc::PasteFromClipboard(DWORD dwStart, BOOL bIns, UINT format)
{
	HGLOBAL hMem;
	LPBYTE pMem;
	LPBYTE pWorkMem;
	DWORD dwSize;
	dwSize = ClipboardReadOpen(hMem, pMem, pWorkMem, format);
	if(!dwSize) return 0;
#ifdef FILE_MAPPING
	if(IsFileMapping()) {
		// FileMapping時、貼り付けるとファイルサイズが大きくなってしまうようであれば、事前に張り付けサイズを切り詰める。
		//int nGlow = dwSize - (m_dwTotal - dwPtr);
		DWORD nGlow = dwSize - (m_dwTotal - dwStart);
		if(nGlow <= dwSize/*overflow check*/ && nGlow > 0)
			dwSize -= nGlow;
	}
#endif //FILE_MAPPING
	if(bIns || dwStart == m_dwTotal)
		StoreUndo(dwStart, dwSize, UNDO_DEL);
	else
		StoreUndo(dwStart, dwSize, UNDO_OVR);
	InsertData(dwStart, dwSize, bIns);
	memcpyMem2Filemap(dwStart, pMem, dwSize);
	ClipboardReadClose(hMem, pMem, pWorkMem);
	return dwStart+dwSize;
}

// strの中からhexstringを構成する文字を探しだし、バイナリに変換した後のバッファのポインタを返す。
// バッファはMemAllocによって確保されているので、MemFreeすること。
// charsは文字数なことに注意。
// bytesは実際のバイナリの長さ(バイト数)で置き換えられる。
// charsとbytesは同じ変数を指定できる。
// hexstringの長さが奇数の場合、最後の文字は切り捨てられる。
template<typename T>
LPBYTE pickupHexstringAndToBinary(T* str, DWORD chars, DWORD &bytes)
{
	LPBYTE pMem = (LPBYTE)MemAlloc(sizeof(T) * (chars + 1));
	LPBYTE p = pMem;
	int tmp = 0;
	int nibble = 0;
	for(DWORD i = 0; i < chars; i++) {
		int num = -1;
		if('0' <= str[i] && str[i] <= '9') num = str[i] - '0';
		if('A' <= str[i] && str[i] <= 'F') num = str[i] - 'A' + 10;
		if('a' <= str[i] && str[i] <= 'f') num = str[i] - 'a' + 10;
		if(num != -1) {
			tmp <<= 4;
			tmp |= num;
			nibble++;
			if(nibble == 2) {
				*p++ = (BYTE)tmp;
				tmp = 0;
				nibble = 0;
			}
		}
	}
	*p = 0;
	bytes = p - pMem;
	return pMem;
}

DWORD CBZDoc::PasteHexstringFromClipboard(DWORD dwPtr, BOOL bIns)
{
	AfxGetMainWnd()->OpenClipboard();
	HGLOBAL hMem;
	DWORD dwSize;
	LPBYTE pMem;
	LPBYTE pWorkMem = NULL;
	if(hMem = GetClipboardData(CF_UNICODETEXT)) {
		int nchars;

		pMem = (LPBYTE)::GlobalLock(hMem);
		nchars = lstrlenW((LPWSTR)pMem);
		dwSize = nchars * 2;

		pMem = pWorkMem = (LPBYTE)pickupHexstringAndToBinary((LPWSTR)pMem, nchars, dwSize);
	} else if(hMem = GetClipboardData(CF_TEXT)) {
		pMem = (LPBYTE)::GlobalLock(hMem);
		dwSize = lstrlenA((LPCSTR)pMem);

		pMem = pWorkMem = pickupHexstringAndToBinary((LPBYTE)pMem, dwSize, dwSize);
	} else {
		return 0;
	}
	if(!dwSize) return 0;
#ifdef FILE_MAPPING
	if(IsFileMapping()) {
		// FileMapping時、貼り付けるとファイルサイズが大きくなってしまうようであれば、事前に張り付けサイズを切り詰める。
		//int nGlow = dwSize - (m_dwTotal - dwPtr);
		DWORD nGlow = dwSize - (m_dwTotal - dwPtr);
		if(nGlow <= dwSize/*overflow check*/ && nGlow > 0)
			dwSize -= nGlow;
	}
#endif //FILE_MAPPING
	if(bIns || dwPtr == m_dwTotal)
		StoreUndo(dwPtr, dwSize, UNDO_DEL);
	else
		StoreUndo(dwPtr, dwSize, UNDO_OVR);
	InsertData(dwPtr, dwSize, bIns);
	memcpy(m_pData+dwPtr, pMem, dwSize);
	::GlobalUnlock(hMem);
	::CloseClipboard();
	if(pWorkMem) {
		MemFree(pWorkMem);
	}
	return dwPtr+dwSize;
}

void CBZDoc::InsertData(DWORD dwStart, DWORD dwSize, BOOL bIns)
{
	BOOL bGlow = false;
	DWORD nGlow = dwSize - (m_dwTotal - dwStart);
	if(nGlow <= dwSize/*overflow check*/ && nGlow > 0)
		bGlow=true;
	if(!m_pData) {
		m_pData = (LPBYTE)MemAlloc(dwSize);
		m_dwTotal = dwSize;
	} else if(bIns || dwStart == m_dwTotal) {
			m_pData = (LPBYTE)MemReAlloc(m_pData, m_dwTotal+dwSize);
			m_dwTotal += dwSize;
			ShiftFileDataR(dwStart, dwSize);//memmove(m_pData+dwStart+dwSize, m_pData+dwStart, m_dwTotal - dwStart);
	} else if(bGlow) {
			m_pData = (LPBYTE)MemReAlloc(m_pData, m_dwTotal+nGlow);
			m_dwTotal += nGlow;
	}
	ASSERT(m_pData != NULL);
}

void CBZDoc::DeleteData(DWORD dwDelStart, DWORD dwDelSize)
{
	if(dwDelStart == m_dwTotal) return;
	ShiftFileDataL(dwDelStart, dwDelSize);//memmove(m_pData+dwPtr, m_pData+dwPtr+dwSize, m_dwTotal-dwPtr-dwSize);
	m_dwTotal -= dwDelSize;
#ifdef FILE_MAPPING
	if(!IsFileMapping())
#endif //FILE_MAPPING
		m_pData = (LPBYTE)MemReAlloc(m_pData, m_dwTotal);
	TouchDoc();
}

BOOL CBZDoc::isDocumentEditedSelfOnly()
{
	return m_dwUndo != m_dwUndoSaved;
}

void CBZDoc::TouchDoc()
{
	SetModifiedFlag(isDocumentEditedSelfOnly());
	GetMainFrame()->OnUpdateFrameTitle();
}

void CBZDoc::OnUpdateEditUndo(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(!!m_pUndo);	
}


//OVR
//dwPtr-4byte(file-offset), mode(byte), data(? byte), dwBlock-4byte(これも含めた全部のバイト)
//dwSizeはdwBlock-9。つまりdataのサイズ

BOOL CBZDoc::StoreUndo(DWORD dwStart, DWORD dwSize, UndoMode mode)
{
	if(mode == UNDO_OVR && dwStart+dwSize >= m_dwTotal)
		dwSize = m_dwTotal - dwStart;
	if(dwSize == 0) return FALSE;
	DWORD dwBlock = dwSize + 9;
	if(mode == UNDO_DEL)
		dwBlock = 4 + 9;
	if(!m_pUndo) {
		m_pUndo = (LPBYTE)MemAlloc(dwBlock);
		m_dwUndo = m_dwUndoSaved = 0;
	} else
		m_pUndo = (LPBYTE)MemReAlloc(m_pUndo, m_dwUndo+dwBlock);
	ASSERT(m_pUndo != NULL);
	LPBYTE p = m_pUndo + m_dwUndo;
	*((DWORD*&)p)++ = dwStart;
	*p++ = mode;
	if(mode == UNDO_DEL) {
		*((DWORD*&)p)++ = dwSize;
	} else {
		memcpyFilemap2Mem(p, dwStart, dwSize);
		p+=dwSize;
	}
	*((DWORD*&)p)++ = dwBlock;
	m_dwUndo += dwBlock;
	ASSERT(m_dwUndo >= dwBlock && m_dwUndo != 0xFFffFFff);//Overflow check
	ASSERT(p == m_pUndo+m_dwUndo);
	TouchDoc();
	return TRUE;
}

DWORD CBZDoc::DoUndo()
{
	DWORD dwSize = *((DWORD*)(m_pUndo+m_dwUndo-4));
	m_dwUndo -= dwSize;
	dwSize -= 9;
	LPBYTE p = m_pUndo + m_dwUndo;
	DWORD dwOffset = *((DWORD*&)p)++;
	UndoMode mode = (UndoMode)*p++;
	if(mode == UNDO_DEL) {
		DeleteData(dwOffset, *((DWORD*)p));
	} else {
		InsertData(dwOffset, dwSize, mode == UNDO_INS);
		memcpyMem2Filemap(dwOffset, p, dwSize);//memcpy(m_pData+dwPtr, p, dwSize);
	}
	if(m_dwUndo)
		m_pUndo = (LPBYTE)MemReAlloc(m_pUndo, m_dwUndo);
	else {				// ### 1.54
		MemFree(m_pUndo);
		m_pUndo = NULL;
		//if(m_dwUndoSaved)m_dwUndoSaved = UINT_MAX;
	}
	if(m_dwUndo < m_dwUndoSaved)m_dwUndoSaved = 0xFFffFFff;
	// if(!m_pUndo)
		TouchDoc();
	return dwOffset;
}

void CBZDoc::DuplicateDoc(CBZDoc* pDstDoc)
{
#ifdef FILE_MAPPING
	if(IsFileMapping()) {
		m_pDupDoc = pDstDoc;
		pDstDoc->m_pDupDoc = this;
		pDstDoc->m_hMapping = m_hMapping;
		pDstDoc->m_pFileMapping = m_pFileMapping;
		pDstDoc->m_dwTotal = m_dwTotal;
		pDstDoc->MapView();
	} else
#endif //FILE_MAPPING
	{
		pDstDoc->m_pData = (LPBYTE)MemAlloc(m_dwTotal);
		memcpy(pDstDoc->m_pData, m_pData, m_dwTotal);
		pDstDoc->m_dwTotal = m_dwTotal;
	}
	pDstDoc->m_dwBase = m_dwBase;
	pDstDoc->SetTitle(GetTitle());
	CString s = GetPathName();
	if(!s.IsEmpty())
		pDstDoc->SetPathName(s);
//	pDstDoc->UpdateAllViews(NULL);

	//Restore infomation
	pDstDoc->m_restoreCaret = m_restoreCaret;
	pDstDoc->m_restoredwBlock = m_restoredwBlock;
	pDstDoc->m_restorebBlock = m_restorebBlock;
	pDstDoc->m_restoreScroll = m_restoreScroll;
}

/////////////////////////////////////////////////////////////////////////////
// CBZDoc Mark

void CBZDoc::SetMark(DWORD dwPtr)
{
	for_to(i, m_arrMarks.GetSize()) {
		if(m_arrMarks[i] == dwPtr) {
			m_arrMarks.RemoveAt(i);
			return;
		} else if(m_arrMarks[i] >= m_dwTotal) {
			m_arrMarks.RemoveAt(i);
		}
	}
	m_arrMarks.Add(dwPtr);
}

BOOL CBZDoc::CheckMark(DWORD dwPtr)
{
	for_to(i,  m_arrMarks.GetSize()) {
		if(m_arrMarks[i] == dwPtr)
			return TRUE;
	}
	return FALSE;
}

DWORD CBZDoc::JumpToMark(DWORD dwStart)
{
	DWORD dwNext = m_dwTotal;
Retry:
	for_to(i, m_arrMarks.GetSize()) {
		if(m_arrMarks[i] > dwStart && m_arrMarks[i] < dwNext)
			dwNext = m_arrMarks[i];
	}
	if(dwNext == m_dwTotal && dwStart) {
		dwStart = 0;
		goto Retry;
	}
	if(dwNext < m_dwTotal) 
		return dwNext;
	return INVALID;
}

void CBZDoc::OnUpdateFileSave(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	pCmdUI->Enable(!m_bReadOnly);
}

/////////////////////////////////////////////////////////////////////////////
// ###1.60 File Mapping

DWORD CBZDoc::GetFileLength(CFile* pFile, BOOL bErrorMsg)
{
	DWORD dwSizeHigh = 0;
	DWORD dwSize = ::GetFileSize((HANDLE)pFile->m_hFile, &dwSizeHigh);
	if(dwSizeHigh) {
		if(bErrorMsg)
			AfxMessageBox(IDS_ERR_OVER4G);
		dwSize = MAX_FILELENGTH;
	}
	return dwSize;
}

#ifdef FILE_MAPPING

#ifndef _AFX_OLD_EXCEPTIONS
    #define DELETE_EXCEPTION(e) do { e->Delete(); } while (0)
#else   //!_AFX_OLD_EXCEPTIONS
    #define DELETE_EXCEPTION(e)
#endif  //_AFX_OLD_EXCEPTIONS


void CBZDoc::ReleaseFile(CFile* pFile, BOOL bAbort)
{
	// ------ File Mapping ----->
	if(IsFileMapping()) return;
	// <----- File Mapping ------
	ASSERT_KINDOF(CFile, pFile);
	if (bAbort)
		pFile->Abort(); // will not throw an exception
	else {
		pFile->Close();
	}
	delete pFile;
}

BOOL CBZDoc::OnOpenDocument(LPCTSTR lpszPathName) 
{
	if (IsModified())
		TRACE0("Warning: OnOpenDocument replaces an unsaved document.\n");

	CFileException fe;
	CFile* pFile = GetFile(lpszPathName,
		CFile::modeRead|CFile::shareDenyWrite, &fe);
	if (pFile == NULL)
	{
		ReportSaveLoadException(lpszPathName, &fe,
			FALSE, AFX_IDP_FAILED_TO_OPEN_DOC);
		return FALSE;
	}

	DeleteContents();
	SetModifiedFlag();  // dirty during de-serialize

	// ------ File Mapping ----->
	DWORD dwSize = GetFileLength(pFile, TRUE);
	if(dwSize >= options.dwMaxOnMemory) {
		m_bReadOnly = options.bReadOnlyOpen || ::GetFileAttributes(lpszPathName) & FILE_ATTRIBUTE_READONLY;
		if(!m_bReadOnly) {  // Reopen for ReadWrite
			ReleaseFile(pFile, FALSE);
			pFile = GetFile(lpszPathName, CFile::modeReadWrite | CFile::shareExclusive, &fe);
			if(pFile == NULL) { //Retry open (ReadOnly)
				pFile = GetFile(lpszPathName, CFile::modeRead|CFile::shareDenyWrite, &fe);
				if(pFile == NULL) { //Failed open
					ReportSaveLoadException(lpszPathName, &fe, FALSE, AFX_IDP_INVALID_FILENAME);
					return FALSE;
				}
				m_bReadOnly = true;
			}
		}
		m_pFileMapping = pFile;
		m_hMapping = ::CreateFileMapping((HANDLE)pFile->m_hFile, NULL, m_bReadOnly ? PAGE_READONLY : PAGE_READWRITE
			, 0, 0, /*options.bReadOnlyOpen ? 0 : dwSize + options.dwMaxOnMemory,*/ NULL);
		if(!m_hMapping) {
			ErrorMessageBox();
			ReleaseFile(pFile, FALSE);
			return FALSE;
		}
	}
	// <----- File Mapping ------

	CArchive loadArchive(pFile, CArchive::load | CArchive::bNoFlushOnDelete);
	loadArchive.m_pDocument = this;
	loadArchive.m_bForceFlat = FALSE;
	TRY
	{
		CWaitCursor wait;
		if (GetFileLength(pFile) != 0)
			Serialize(loadArchive);     // load me
		loadArchive.Close();
		ReleaseFile(pFile, FALSE);
	}
	CATCH_ALL(e)
	{
		ReleaseFile(pFile, TRUE);
		DeleteContents();   // remove failed contents

		TRY
		{
			ReportSaveLoadException(lpszPathName, e,
				FALSE, AFX_IDP_FAILED_TO_OPEN_DOC);
		}
		END_TRY
		DELETE_EXCEPTION(e);
		return FALSE;
	}
	END_CATCH_ALL

	SetModifiedFlag(FALSE);     // start off with unmodified

	return TRUE;
}

BOOL CBZDoc::OnSaveDocument(LPCTSTR lpszPathName) 
{
	CFileException fe;
	CFile* pFile = NULL;

	// ------ File Mapping ----->
	if(IsFileMapping())
		pFile = m_pFileMapping;
	else 
	// <----- File Mapping ------
		pFile = GetFile(lpszPathName, CFile::modeCreate | CFile::modeReadWrite | CFile::shareExclusive, &fe);

	if (pFile == NULL)
	{
		ReportSaveLoadException(lpszPathName, &fe,
			TRUE, AFX_IDP_INVALID_FILENAME);
		return FALSE;
	}

	CArchive saveArchive(pFile, CArchive::store | CArchive::bNoFlushOnDelete);
	saveArchive.m_pDocument = this;
	saveArchive.m_bForceFlat = FALSE;
	TRY
	{
		CWaitCursor wait;
		Serialize(saveArchive);     // save me
		saveArchive.Close();
		ReleaseFile(pFile, FALSE);
	}
	CATCH_ALL(e)
	{
		ReleaseFile(pFile, TRUE);

		TRY
		{
			ReportSaveLoadException(lpszPathName, e,
				TRUE, AFX_IDP_FAILED_TO_SAVE_DOC);
		}
		END_TRY
		DELETE_EXCEPTION(e);
		return FALSE;
	}
	END_CATCH_ALL

	SetModifiedFlag(FALSE);     // back to unmodified

	return TRUE;        // success
}

BOOL CBZDoc::SaveModified() 
{
	if (!IsModified())
		return TRUE;        // ok to continue

	// get name/title of document
	CString name;
	if (m_strPathName.IsEmpty())
	{
		// get name based on caption
		name = m_strTitle;
		if (name.IsEmpty())
			VERIFY(name.LoadString(AFX_IDS_UNTITLED));
	}
	else
	{
		// get name based on file title of path name
		name = m_strPathName;
		/*if (afxData.bMarked4)
		{
			AfxGetFileTitle(m_strPathName, name.GetBuffer(_MAX_PATH), _MAX_PATH);
			name.ReleaseBuffer();
		}*/
	}

	CString prompt;
	AfxFormatString1(prompt, AFX_IDP_ASK_TO_SAVE, name);
	switch (AfxMessageBox(prompt, MB_YESNOCANCEL, AFX_IDP_ASK_TO_SAVE))
	{
	case IDCANCEL:
		return FALSE;       // don't continue

	case IDYES:
		// If so, either Save or Update, as appropriate
		if (!DoFileSave())
			return FALSE;       // don't continue
		break;

	case IDNO:
		// If not saving changes, revert the document
		if(IsFileMapping() && m_dwUndoSaved != 0xFFffFFff) {
			while(isDocumentEditedSelfOnly())//m_pUndo)
				DoUndo();
		}
		break;

	default:
		ASSERT(FALSE);
		break;
	}
	return TRUE;    // keep going
}

#endif //FILE_MAPPING
