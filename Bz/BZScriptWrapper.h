#pragma once
class BZScriptWrapper
{
public:
	BZScriptWrapper(void);
	~BZScriptWrapper(void);

	BOOL getNextHistory(CString &str);
	BOOL getPrevHistory(CString &str);
	BOOL init(BZScriptInterface *sif, CBZScriptView *view);
	CString run(CBZScriptView *view, CString str);
	BZScriptInterface *getSIF(void) { return sif; };
protected:
	BZScriptInterface *sif;
	CStringArray history;
	int histidx;
};

