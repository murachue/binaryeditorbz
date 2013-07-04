#pragma once
class BZScriptRuby
{
public:
	BZScriptRuby(void);
	~BZScriptRuby(void);
	void init(CBZScriptView *sview);
	void cleanup(CBZScriptView *sview);
	void onClear(CBZScriptView* sview);
	CString run(CBZScriptView* sview, const char * cmdstr);
};

