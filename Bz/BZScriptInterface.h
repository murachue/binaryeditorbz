#pragma once
class CBZScriptView;

class BZScriptInterface
{
public:

	BZScriptInterface(void)
	{
	}

	~BZScriptInterface(void)
	{
	}

	virtual BOOL init(CBZScriptView *sview) = 0;
	virtual void cleanup(CBZScriptView *sview) = 0;
	virtual void onClear(CBZScriptView* sview) = 0;
	virtual CString run(CBZScriptView* sview, const char * cmdstr) = 0;
	virtual CString name(void) = 0;
};

