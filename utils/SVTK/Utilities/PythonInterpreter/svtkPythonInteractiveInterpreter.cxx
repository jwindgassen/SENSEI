/*=========================================================================

  Program:   Visualization Toolkit
  Module:    svtkPythonInteractiveInterpreter.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "svtkPython.h"

#include "svtkPythonInteractiveInterpreter.h"

#include "svtkCommand.h"
#include "svtkObjectFactory.h"
#include "svtkPythonInterpreter.h"
#include "svtkSmartPointer.h"

#include <string>
class svtkPythonInteractiveInterpreter::svtkInternals
{
  PyObject* InteractiveConsole;
  PyObject* InteractiveConsoleLocals;

  std::string PS1;
  std::string PS2;

public:
  svtkSmartPointer<svtkPythonInterpreter> Interpreter;

  svtkInternals()
    : InteractiveConsole(nullptr)
    , InteractiveConsoleLocals(nullptr)
  {
  }
  ~svtkInternals() { this->CleanupPythonObjects(); }

  PyObject* GetInteractiveConsolePyObject() { return this->InteractiveConsole; }
  PyObject* GetInteractiveConsoleLocalsPyObject() { return this->InteractiveConsoleLocals; }

  void CleanupPythonObjects()
  {
    if (this->InteractiveConsole)
    {
      svtkPythonScopeGilEnsurer gilEnsurer;
      Py_XDECREF(this->InteractiveConsoleLocals);
      Py_XDECREF(this->InteractiveConsole);
      this->InteractiveConsole = nullptr;
      this->InteractiveConsoleLocals = nullptr;
      if (svtkPythonInterpreter::IsInitialized())
      {
        const char* code = "import gc; gc.collect()\n";
        svtkPythonInterpreter::RunSimpleString(code);
      }
    }
  }

  PyObject* GetInteractiveConsole()
  {
    if (this->InteractiveConsole)
    {
      return this->InteractiveConsole;
    }

    svtkPythonInterpreter::Initialize();

    svtkPythonScopeGilEnsurer gilEnsurer;
    // set up the code.InteractiveConsole instance that we'll use.
    const char* code = "import code\n"
                       "__svtkConsoleLocals={'__name__':'__svtkconsole__','__doc__':None}\n"
                       "__svtkConsole=code.InteractiveConsole(__svtkConsoleLocals)\n";

    // The const_cast can be removed for Python 3.3 or later.
    PyRun_SimpleString(const_cast<char*>(code));

    // Now get the reference to __svtkConsole and save the pointer.
    PyObject* main_module = PyImport_AddModule("__main__");
    PyObject* global_dict = PyModule_GetDict(main_module);
    this->InteractiveConsole = PyDict_GetItemString(global_dict, "__svtkConsole");
    this->InteractiveConsoleLocals = PyDict_GetItemString(global_dict, "__svtkConsoleLocals");
    if (!this->InteractiveConsole || !this->InteractiveConsoleLocals)
    {
      svtkGenericWarningMacro(
        "Failed to locate the InteractiveConsole/InteractiveConsoleLocals object.");
      return nullptr;
    }
    Py_INCREF(this->InteractiveConsole);
    Py_INCREF(this->InteractiveConsoleLocals);

    // The const_cast can be removed for Python 3.3 or later.
    PyRun_SimpleString(const_cast<char*>("del __svtkConsole; del __svtkConsoleLocals"));

    // Maybe we need an API to enable developers to set the prompts.
    // (The const_cast can be removed for Python 3.3 or later).
    PyObject* ps1 = PySys_GetObject(const_cast<char*>("ps1"));
    if (!ps1)
    {
#if PY_VERSION_HEX >= 0x03000000
      ps1 = PyUnicode_FromString(">>> ");
#else
      ps1 = PyString_FromString(">>> ");
#endif
      // The const_cast can be removed for Python 3.3 or later.
      PySys_SetObject(const_cast<char*>("ps1"), ps1);
      Py_XDECREF(ps1);
    }

    // The const_cast can be removed for Python 3.3 or later.
    PyObject* ps2 = PySys_GetObject(const_cast<char*>("ps2"));
    if (!ps2)
    {
#if PY_VERSION_HEX >= 0x03000000
      ps2 = PyUnicode_FromString("... ");
#else
      ps2 = PyString_FromString("... ");
#endif
      // The const_cast can be removed for Python 3.3 or later.
      PySys_SetObject(const_cast<char*>("ps2"), ps2);
      Py_XDECREF(ps2);
    }

    return this->InteractiveConsole;
  }
};

svtkStandardNewMacro(svtkPythonInteractiveInterpreter);
//----------------------------------------------------------------------------
svtkPythonInteractiveInterpreter::svtkPythonInteractiveInterpreter()
  : Internals(new svtkPythonInteractiveInterpreter::svtkInternals())
{
  this->Internals->Interpreter = svtkSmartPointer<svtkPythonInterpreter>::New();
  this->Internals->Interpreter->AddObserver(
    svtkCommand::AnyEvent, this, &svtkPythonInteractiveInterpreter::HandleEvents);
}

//----------------------------------------------------------------------------
svtkPythonInteractiveInterpreter::~svtkPythonInteractiveInterpreter()
{
  delete this->Internals;
  this->Internals = nullptr;
}

//----------------------------------------------------------------------------
void svtkPythonInteractiveInterpreter::HandleEvents(
  svtkObject* svtkNotUsed(caller), unsigned long eventid, void* calldata)
{
  if (eventid == svtkCommand::ExitEvent)
  {
    this->Internals->CleanupPythonObjects();
  }

  this->InvokeEvent(eventid, calldata);
}

//----------------------------------------------------------------------------
bool svtkPythonInteractiveInterpreter::Push(const char* const code)
{
  PyObject* console = this->Internals->GetInteractiveConsole();
  if (!console)
  {
    return false;
  }

  // The embedded python interpreter cannot handle DOS line-endings, see
  // http://sourceforge.net/tracker/?group_id=5470&atid=105470&func=detail&aid=1167922
  std::string buffer = code ? code : "";
  // replace "\r\n" with "\n"
  std::string::size_type i = buffer.find("\r\n");
  for (; i != std::string::npos; i = buffer.find("\r\n", i))
  {
    buffer.replace(i, 2, "\n");
    i++;
  }

  // replace "\r" with "\n"  (sometimes seen on Mac)
  i = buffer.find("\r");
  for (; i != std::string::npos; i = buffer.find("\r", i))
  {
    buffer.replace(i, 1, "\n");
    i++;
  }

  svtkPythonScopeGilEnsurer gilEnsurer;
  bool ret_value = false;
  // The const_cast can be removed for Python 3.4 or later.
  PyObject* res =
    PyObject_CallMethod(console, const_cast<char*>("push"), const_cast<char*>("z"), buffer.c_str());
  if (res)
  {
    int status = 0;
    if (PyArg_Parse(res, "i", &status))
    {
      ret_value = (status > 0);
    }
    Py_DECREF(res);
  }
  return ret_value;
}

//----------------------------------------------------------------------------
int svtkPythonInteractiveInterpreter::RunStringWithConsoleLocals(const char* script)
{
  // The implementation of this method is modeled after
  // PyRun_SimpleStringFlags() found in Python's pythonrun.c

  this->Internals->GetInteractiveConsole(); // ensure the console is initialized

  svtkPythonScopeGilEnsurer gilEnsurer;
  PyObject* context = this->Internals->GetInteractiveConsoleLocalsPyObject();
  // The const_cast can be removed for Python 3.3 or later.
  PyObject* result = PyRun_String(const_cast<char*>(script), Py_file_input, context, context);

  if (result == nullptr)
  {
    PyErr_Print();
    return -1;
  }

  Py_DECREF(result);
#if PY_VERSION_HEX >= 0x03000000
  // The const_cast can be removed for Python 3.3 or later.
  PyObject* f = PySys_GetObject(const_cast<char*>("stdout"));
  if (f == 0 || PyFile_WriteString("\n", f) != 0)
  {
    PyErr_Clear();
  }
#else
  if (Py_FlushLine())
  {
    PyErr_Clear();
  }
#endif

  return 0;
}

//----------------------------------------------------------------------------
void svtkPythonInteractiveInterpreter::Reset()
{
  this->Internals->CleanupPythonObjects();
}

//----------------------------------------------------------------------------
void* svtkPythonInteractiveInterpreter::GetInteractiveConsolePyObject()
{
  return this->Internals->GetInteractiveConsolePyObject();
}

//----------------------------------------------------------------------------
void* svtkPythonInteractiveInterpreter::GetInteractiveConsoleLocalsPyObject()
{
  return this->Internals->GetInteractiveConsoleLocalsPyObject();
}

//----------------------------------------------------------------------------
void svtkPythonInteractiveInterpreter::PrintSelf(ostream& os, svtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
