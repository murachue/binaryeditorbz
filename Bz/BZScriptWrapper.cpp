#include "stdafx.h"
#include "BZScriptInterface.h"
#include "BZScriptWrapper.h"


BZScriptWrapper::BZScriptWrapper(void)
{
}


BZScriptWrapper::~BZScriptWrapper(void)
{
}


BOOL BZScriptWrapper::getNextHistory(CString &str)
{
	if(histidx < history.GetCount())
	{
		histidx++;
		if(histidx < history.GetCount())
		{
			str = history.GetAt(histidx);
		} else
		{
			str = CString(_T(""));
		}
		return TRUE;
	}
	return FALSE;
}


BOOL BZScriptWrapper::getPrevHistory(CString &str)
{
	if(0 < histidx)
	{
		histidx--;
		str = history.GetAt(histidx);
		return TRUE;
	}
	return FALSE;
}


BOOL BZScriptWrapper::init(BZScriptInterface *sif, CBZScriptView *view)
{
	if(!sif->init(view))
		return FALSE;

	this->sif = sif;
	histidx = 0;

	return TRUE;
}


CString BZScriptWrapper::run(CBZScriptView *view, CString str)
{
	// —š—ð‚ÌÅŒã‚Æ“¯‚¶‚È‚ç—š—ð‚É’Ç‰Á‚µ‚È‚¢
	if(history.GetCount() == 0 || history.GetAt(history.GetCount() - 1) != str)
	{
		histidx = history.Add(str) + 1;
	} else
	{
		histidx = history.GetCount();
	}

	return sif->run(view, CStringA(str));
}
