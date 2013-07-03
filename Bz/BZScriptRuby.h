#pragma once
class BZScriptRuby
{
public:
	BZScriptRuby(void);
	~BZScriptRuby(void);
	void init(void);
	void cleanup(void);
	void onClear(CBZScriptView* sview);
	CString run(CBZScriptView* sview, const char * cmdstr);
};

