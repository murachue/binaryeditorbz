#pragma once
class BZScriptPython
{
public:
	BZScriptPython(void);
	~BZScriptPython(void);
	void init(CBZScriptView *sview);
	void cleanup(CBZScriptView *sview);
	void onClear(CBZScriptView* sview);
	CString run(CBZScriptView* sview, const char * cmdstr);
};

