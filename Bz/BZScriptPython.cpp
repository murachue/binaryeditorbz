#include "stdafx.h"
#include "Bz.h" // options
#include "BZView.h"
#include "BZDoc.h"
#include "BZScriptInterface.h"
#include "BZScriptView.h"
#include "BZScriptPython.h"

// !!!!!
// Py_NO_ENABLE_SHARED also disables dllimport, I just want to avoid to linking against python27_d.lib in Debug config which is not included in Win32 Python distrib...
#ifdef _DEBUG
#define _DEBUG_IS_ON
#undef _DEBUG
#endif

#include "python.h"

#ifdef _DEBUG_IS_ON
#define _DEBUG
#endif

static CBZScriptView *cbzsv; // TODO: Global...


extern "C" BZScriptInterface __declspec(dllexport) * getScriptInterface(void)
{
	return new BZScriptPython();
}


BZScriptPython::BZScriptPython(void)
{
}


BZScriptPython::~BZScriptPython(void)
{
}


static PyObject* python_write(PyObject *self, PyObject *args)
{
	int ok;
	char *str;
	int len;

	ok = PyArg_ParseTuple(args, "s#", &str, &len);
	if(!ok) // TODO: is it correct?
	{
		// TODO: exception
		PyErr_SetString(PyExc_NotImplementedError, "argument must be like write(str)");
		return NULL; // must NULL when raising exception?
	}
	CString cstr(str, len);

	CString rstr;
	cbzsv->m_editResult.GetWindowText(rstr);
	cstr.Replace(_T("\n"), _T("\r\n"));
	rstr.Append(cstr);
	cbzsv->m_editResult.SetWindowText(rstr);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef pythonMethods[] =
{
	{"write", python_write, METH_VARARGS, ""},
	{NULL, NULL, 0, NULL}
};

BOOL BZScriptPython::init(CBZScriptView *sview)
{
	Py_SetProgramName("BZ");
	Py_Initialize();
	PyObject *name;
	//name = PyString_FromString("sys");
	//PyObject *mod = PyImport_ImportModule("sys");
	//PyObject *mod = PyImport_Import(name);
	//Py_DECREF(name);

	PyObject *module = PyImport_AddModule("__main__");
	PyObject *py_globals = (module == NULL) ? NULL : PyModule_GetDict(module);
	PyObject *mod = PyImport_ImportModuleEx("sys", py_globals, py_globals, NULL);

	// TODO: sys.stdout/stdin/stderrを自作関数に置き換える
	// なぜうごかない…
	PyObject *mymodule = Py_InitModule("bzwriter", pythonMethods);
	name = PyString_FromString("write");
	PyObject_SetAttr(mod, name, mymodule);
	Py_DECREF(name);
	Py_DECREF(mod);

	// しかたないのでad-hocに TODO: なんとかしてこの行を抹消する。
	PyRun_SimpleString("import sys; import bzwriter; sys.stdout=bzwriter; sys.stderr=bzwriter");

	return TRUE;
}


void BZScriptPython::cleanup(CBZScriptView *sview)
{
	Py_Finalize();
}


void BZScriptPython::onClear(CBZScriptView* sview)
{
	cbzsv = sview;
	PyRun_SimpleString("print('Python ' + sys.version)");
}


CString BZScriptPython::run(CBZScriptView* sview, const char * cmdstr)
{
	// TODO: Isn't there method that handle write() as Ruby?
    //PyObject *pModule = PyImport_AddModule("__main__"); //create main module

	// referring IDAPython PythonEvalOrExec..
	PyCompilerFlags cf = {0};
	PyObject *py_code = Py_CompileStringFlags(cmdstr, "<string>", Py_eval_input, &cf);
	if(py_code == NULL || PyErr_Occurred())
	{
		// Not an expression?
		PyErr_Clear();

		// Run as a string
		PyRun_SimpleString(cmdstr);
		return _T("");
	}

	PyObject *module = PyImport_AddModule("__main__");
	PyObject *py_globals = (module == NULL) ? NULL : PyModule_GetDict(module);
	cbzsv = sview;
	//PYW_GIL_ENSURE;
	PyObject *py_result = PyEval_EvalCode((PyCodeObject*)py_code, py_globals, py_globals);
	//PYW_GIL_RELEASE;
	Py_DECREF(py_code);

	if(py_result == NULL || PyErr_Occurred())
	{
		PyObject *t,*v,*b;
		PyErr_Fetch(&t,&v,&b);
		PyErr_Print();
		return _T("");
	}
	if(py_result != Py_None)
	{
		PyObject *pystr = PyObject_Str(py_result);
		Py_DECREF(py_result);
		char *cstr = PyString_AsString(pystr);
		int cstrlen = strlen(cstr);
		CStringA csresult(cstr, cstrlen);
		Py_DECREF(pystr);
		CString csnative(csresult);
		CString ostr;

		csnative.Replace(_T("\n"), _T("\r\n"));
		ostr.Format(_T("%s\r\n"), csnative);

		return ostr;
	}

	Py_DECREF(py_result);

	return _T("");//ostr;
}


CString BZScriptPython::name(void)
{
	return CString(_T("Python"));
}
