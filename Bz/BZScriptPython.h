#pragma once
class BZScriptPython
{
public:
	BZScriptPython(void);
	~BZScriptPython(void);
	void init(void);
	void cleanup(void);
	void onClear(CBZScriptView* sview);
	CString run(CBZScriptView* sview, const char * cmdstr);
};

