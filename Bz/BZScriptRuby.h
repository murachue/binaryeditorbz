#pragma once
class BZScriptRuby : public BZScriptInterface
{
public:
	BZScriptRuby(void);
	~BZScriptRuby(void);
	virtual BOOL init(CBZScriptView *sview);
	virtual void cleanup(CBZScriptView *sview);
	virtual void onClear(CBZScriptView* sview);
	virtual CString run(CBZScriptView* sview, const char * cmdstr);
	virtual CString name(void);
};

