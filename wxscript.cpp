                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2006 Andrew Trevorrow and Tomas Rokicki.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 Web site:  http://sourceforge.net/projects/golly
 Authors:   rokicki@gmail.com  andrew@trevorrow.com

                        / ***/

// NOTE: Much of the code in this module is from the wxScript package
// by Francesco Montorsi.  Here is Francesco's copyright notice:

///////////////////////////////////////////////////////////////////////
// wxScript classes                                                  //
// Copyright (C) 2003 by Francesco Montorsi                          //
//                                                                   //
// This program is free software; you can redistribute it and/or     //
// modify it under the terms of the GNU General Public License       //
// as published by the Free Software Foundation; either version 2    //
// of the License, or (at your option) any later version.            //
//                                                                   //
// This program is distributed in the hope that it will be useful,   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the      //
// GNU General Public License for more details.                      //
//                                                                   //
// You should have received a copy of the GNU General Public         //
// License along with this program; if not, write to the Free        //
// Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,   //
// MA 02111-1307, USA.                                               //
//                                                                   //
// For any comment, suggestion or feature request, please contact    //
// frm@users.sourceforge.net                                         //
//                                                                   //
///////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
   #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include <wx/file.h>       // for wxFile
#include "wx/filename.h"   // for wxFileName

#include "bigint.h"
#include "lifealgo.h"
#include "qlifealgo.h"
#include "hlifealgo.h"
#include "liferules.h"     // for global_liferules
#include "readpattern.h"
#include "writepattern.h"

#include "wxgolly.h"       // for wxGetApp, mainptr, viewptr, statusptr
#include "wxmain.h"        // for mainptr->...
#include "wxview.h"        // for viewptr->...
#include "wxstatus.h"      // for statusptr->...
#include "wxutils.h"       // for Warning
#include "wxprefs.h"       // for hashing etc
#include "wxinfo.h"        // for ShowInfo
#include "wxhelp.h"        // for ShowHelp
#include "wxscript.h"

// Python includes
#ifdef HAVE_WCHAR_H
#undef HAVE_WCHAR_H        // Python.h redefines this if we use Unicode mode
#endif
#include <Python.h>

// -----------------------------------------------------------------------------

// Francesco's script.h

// Defines & macros
// ------------------

//! The maximum number of arguments for a single function.
#define wxSCRIPTFNC_MAX_ARGS        32

#ifndef wxSAFE_DELETE
#define wxSAFE_DELETE(x)            { if (x) delete x; x = NULL; }
#endif

#ifndef wxSAFE_DELETE_ARRAY
#define wxSAFE_DELETE_ARRAY(x)      { if (x) delete [] x; x = NULL; }
#endif


//! A classification of the types of a wxScriptVar.
enum wxScriptTypeGeneric {

   wxSTG_UNDEFINED = -1,   //! Something wrong.

   wxSTG_VOID,             //! Basic C++ types.
   wxSTG_INT,
   wxSTG_LONG,
   wxSTG_CHAR,
   wxSTG_FLOAT,
   wxSTG_DOUBLE,
   wxSTG_BOOL,

   wxSTG_USERDEFINED,      //! A C++ user-defined type (a class, structure, union or enum).

   wxSTG_POINTER,          //! A pointer to something.
   wxSTG_REFERENCE         //! A reference to something.
};



//! A sort of extended ENUM containing information and utilities
//! about a interpreted type which can be chosen at runtime.
class wxScriptTypeInfo
{
protected:

   //! The way this class stores a C++ type cannot be an enum or
   //! something like that because in C++ there can be infinite
   //! data types with different names, because an undefined
   //! number of classes, structures, unions... can be declared.
   wxString m_strName;

public:
   wxScriptTypeInfo(const wxString &str = wxEmptyString) { Set(str); }
   wxScriptTypeInfo(wxScriptTypeGeneric type) { SetGenericType(type); }
   virtual ~wxScriptTypeInfo() {}

   // miscellaneous
   virtual void DeepCopy(const wxScriptTypeInfo *p) { m_strName = p->m_strName; }
   virtual bool Match(const wxScriptTypeInfo &p) const { return (GetName().IsSameAs(p.GetName(), FALSE)); }
   virtual bool Match(const wxScriptTypeInfo *p) const { return p != NULL && Match(*p); }
   friend bool operator==(const wxScriptTypeInfo &first, const wxScriptTypeInfo &second) 
      { return first.Match(second); }

   // setters
   virtual void Set(const wxString &str);
   virtual void SetGenericType(wxScriptTypeGeneric t);
   virtual void SetAsPointer() { Set(GetName() + wxT("*")); }
   virtual void SetAsReference() { Set(GetName() + wxT("&")); }
   
   // getters
   wxString GetPointerTypeName() const;
   wxString GetName() const { return m_strName; }
   wxScriptTypeInfo GetPointerType() const { return wxScriptTypeInfo(GetPointerTypeName()); }
   wxScriptTypeGeneric GetGenericType() const;

   // checkers
   bool isPointer() const;
   bool isReference() const;
   bool isValid() const { return !m_strName.IsEmpty(); }
};



//! A variable object containing both info about its type
//! (using wxScriptTypeInfo) and the contents of the variable.
class wxScriptVar
{
protected:

   //! The type of this variable.
   wxScriptTypeInfo m_tType;

   //! The contents of this variable.
   union {
      long m_content;
      double m_floatcontent;
   };

public:

   //! Copy constructor.
   wxScriptVar(const wxScriptVar &var) {
      Copy(var);
   }

   wxScriptVar(const wxString &type = wxEmptyString, 
            const wxString &content = wxEmptyString) : m_content(0) {
      SetType(type);
      SetContent(content);
   }

   //! This constructor is used to init this variable as a pointer.
   //! The given type string must end with "*" and the given memory
   //! address must be cast to "int *" to avoid ambiguity with the
   //! previous constructor.
   //!
   //! Example:
   //!    myClass test;
   //!    wxScriptVar arg1("myClass *", (int *)(&test));
   //!
   wxScriptVar(const wxString &type,
            int *pointer) : m_content((long)pointer) {

      SetType(type);
      wxASSERT(GetType().isPointer());
   }

   virtual ~wxScriptVar() { ResetContent(); }


   wxScriptVar &operator=(const wxScriptVar &tocopy)
      { Copy(tocopy); return *this; }



   /////////////////////////////////////////////////////////////////////////////
   //! \name SET functions. @{

   //! Sets the type of this variable; wxScriptTypeInfo::Set() is used.
   virtual void SetType(const wxString &str)      { m_tType.Set(str); }

   //! Sets the contents of this variable using the given string.
   //! The content decoding is done using the current type set.
   //! This function uses the same decode rules used to encode
   //! the content by the #GetContentString() function; so, no
   //! loss of data should happen if you do:
   //! \code
   //!     wxScriptVar myvar("bool", "false");
   //!     myvar.SetContent(myvar.GetContentString());
   //! \endcode
   virtual void SetContent(const wxString &);

   virtual void SetContent(const wxChar *str)   { SetContent(wxString(str)); }
   virtual void SetContent(wxChar *str)         { SetContent(wxString(str)); }
   virtual void SetContent(long l)              { m_content = l; }
   virtual void SetContent(double l)            { m_floatcontent = l; }
   virtual void SetContent(bool l)              { m_content = (l != 0); }
   virtual void SetContent(void *l)             { m_content = (long)l; }
   virtual void SetContent(const void *l)       { m_content = (long)l; }

   //! Changes both type & content of this variable.
   void Set(const wxString &type, const wxString &content)     { SetType(type); SetContent(content); }
   void Set(wxScriptTypeGeneric type, char *content)           { m_tType.SetGenericType(type); SetContent(content); }
   void Set(wxScriptTypeGeneric type, long content)            { m_tType.SetGenericType(type); SetContent(content); }
   void Set(wxScriptTypeGeneric type, double content)          { m_tType.SetGenericType(type); SetContent(content); }
   void Set(wxScriptTypeGeneric type, bool content)            { m_tType.SetGenericType(type); SetContent(content); }
   void Set(wxScriptTypeGeneric type, void *content)           { m_tType.SetGenericType(type); SetContent(content); }
   void Set(wxScriptTypeGeneric type, const wxString &content) { m_tType.SetGenericType(type); SetContent(content); }

   //! Sets as empty the current contents (deleting eventually allocated memory).
   virtual void ResetContent();

   //! Copies the given wxScriptVar object.
   virtual void Copy(const wxScriptVar &var);

   //@}



   /////////////////////////////////////////////////////////////////////////////
   //! \name GET functions. @{

   //! Returns the type of this variable.
   wxScriptTypeInfo GetType() const    { return m_tType; }

   //! Returns the contents of this variable as LONG.
   long GetContentLong() const         { return m_content; }

   //! Returns the contents of this variable as a DOUBLE.
   double GetContentDouble() const     { return (m_tType == wxSTG_INT || m_tType == wxSTG_LONG) ? m_content : m_floatcontent; }

   //! Returns the contents encoded in a string.
   //! The content encoding is done using the current type set.
   //! This function uses the same encode rules used to decode
   //! the content by the #SetContent(const wxString &) function.
   virtual wxString GetContentString() const;   

   //! Returns the memory address hold by this variable if it is set
   //! as a pointer.
   virtual void *GetPointer() const    { return (m_tType.isPointer() ? (void*)m_content : NULL); }

   //@}
};



//! An interpreted function.
class wxScriptFunction
{
protected:

   //! The name of the function.
   wxString m_strName;

   //! The return type of the function.
   wxScriptTypeInfo m_tReturn;

   //! The argument type array.
   wxScriptTypeInfo m_pArgList[wxSCRIPTFNC_MAX_ARGS];

   //! The number of entries of #m_pArgList array.
   int m_nArgCount;

protected:

   //! Returns the "call string"; that is, the string containing
   //! function name + wxT("(") + arg[n]->GetContentString() + ")".
   wxString GetCallString(wxScriptVar *arr) const;

public:

   //! Creates & set the properties of the object.
   wxScriptFunction(const wxString &name = wxEmptyString, 
               const wxString &ret = wxEmptyString,
               wxScriptTypeInfo *arr = NULL, int n = 0) { 
      m_nArgCount = 0;
      Set(name, ret, arr, n);
   }

   virtual ~wxScriptFunction() {}


   //! Deep copies the function.
   virtual void DeepCopy(const wxScriptFunction *tocopy) {
      m_strName = tocopy->m_strName;
      m_nArgCount = tocopy->m_nArgCount;
      m_tReturn.DeepCopy(&tocopy->m_tReturn);

      for (int i=0; i < m_nArgCount; i++)
         m_pArgList[i].DeepCopy(&tocopy->m_pArgList[i]);
   }

   //! Returns a clone of this object.
   //! The caller must delete the returned object when it's no longer useful.
   virtual wxScriptFunction *Clone() const = 0;

   //! Checks if this function has the given return type, argument count
   //! and argument types.
   virtual bool Match(const wxScriptTypeInfo *ret, int argc, ...);

   //! Executes the function script.
   virtual bool Exec(wxScriptVar &ret, wxScriptVar *arg) const = 0;

   //! Sets the name, return type and arguments of this scripted function.
   virtual void Set(const wxString &name, const wxString &ret, 
                wxScriptTypeInfo *arr, int n);


   //////////////////////////////////////////////////////////////////////
   //! \name Getters. @{

   //! Returns the name of this function
   wxString GetName() const   { return m_strName; }

   //! Returns the number of arguments accepted by this function.
   int GetArgCount() const    { return m_nArgCount; }

   //! Returns the type of the return value of this function.
   wxScriptTypeInfo GetRetType() const        { return m_tReturn; }

   //! Returns the type of the n-th argument required by this function.
   wxScriptTypeInfo GetArgType(int n) const   { return m_pArgList[n]; }

   //@}
};



//! An array of wxScriptFunction objects.
class wxScriptFunctionArray
{
protected:

   //! A simple auto-expanding array containing pointers to the 
   //! wxScriptFunction contained... the object handled by this array
   //! are not automatically deleted: wxScriptFunctionArray::Clear
   //! have to cleanup everything.
   //!
   //! VERY IMPORTANT: we cannot use the wxObjArray macros because
   //! we need to store pointers to abstract classes which cannot
   //! be copied (wxObjArray requires non-abstract classes to store).
   wxArrayPtrVoid m_arr;

   //! An optional string which is used by #GetName() to strip
   //! off unwanted function prefixes.
   wxString m_strToStrip;

public:
   wxScriptFunctionArray(const wxString &tostrip = wxEmptyString) : m_strToStrip(tostrip) {}
   virtual ~wxScriptFunctionArray() { Clear(); }


   // --------
   // Getters
   // --------

   wxScriptFunction *Get(int idx) const      { return (wxScriptFunction *)m_arr.Item(idx); }
   wxScriptFunction *Get(const wxString &fncname, int n = 0) const;
   int GetIdx(const wxString &fncname, int n = 0) const;

   int GetCountOf(const wxString &fncname) const;
   wxString GetName(int n) const;
   wxString GetPrefixToStrip() const         { return m_strToStrip; }
   int GetCount() const                      { return m_arr.GetCount(); }
   wxArrayPtrVoid &GetArray()                { return m_arr; }


   // --------------
   // Miscellaneous
   // --------------

   void Insert(wxScriptFunction *toadd, int idx);
   void Append(wxScriptFunction *toadd)               { m_arr.Insert(toadd, GetCount()); }
   void Append(const wxScriptFunctionArray &arr);

   void Remove(int i);
   void Remove(const wxString &fncname, int n = 0)    { Remove(GetIdx(fncname, n)); }

   void SetPrefixToStrip(const wxString &str)         { m_strToStrip = str; }
   void DeepCopy(const wxScriptFunctionArray *arr);
   void Clear();
};



//! A script file.
class wxScriptFile
{
protected:

   //! The name of the file containing the scripted functions.
   wxString m_strFileName;

public:
   wxScriptFile() {}
   virtual ~wxScriptFile() {}
   
   virtual bool Load(const wxString &file) = 0;
};


// this will be defined later
class wxPython;

//! A singleton class that wraps all the script interpreters supported.
class wxScriptInterpreter
{
protected:

   //! The shared instance of the wxScriptInterpreter.
   // use wxPython::Get() to access
   static wxPython *m_pPython;

public:      // ctor & dtor

   wxScriptInterpreter();
   virtual ~wxScriptInterpreter();

   //! The description of the last error.
   static wxString m_strLastErr;

public:      // static functions

   // A STATIC function which returns the global instance of this class
   // cannot exist because wxScriptInterpreter is an abstract class
   // and thus, only derived classes (wxPython) can
   // be instantiated; you can access them using their static Get()
   // functions: wxPython::Get()

   //! Initializes the script interpreter.
   static bool Init();

   //! Deallocates the script interpreter.
   static void Cleanup();

   //! Returns TRUE if the script interpreter is ready to work.
   static bool areAllReady();

   //! Returns the list of the functions currently recognized by the interpreter.
   static void GetTotalFunctionList(wxScriptFunctionArray &);

   //! Load the given scriptfile and then returns an instance to the
   //! scriptfile wrapper, or NULL if the file couldn't be loaded.
   static wxScriptFile *Load(const wxString &filename);

public:      // virtual functions

   //! Returns TRUE if this script interpreter is ready to work.
   virtual bool isReady() const = 0;

   //! Returns the list of the functions currently recognized by the interpreter.
   virtual void GetFunctionList(wxScriptFunctionArray &) const = 0;

   //! Returns version info about this interpreter or a wxEmptyString if
   //! this interpreter does not supply any version info.
   //! The exact form of the string returned is interpreter-dependent,
   //! but it is usually given as "INTERPRETER_NAME VERSION_STRING".
   virtual wxString GetVersionInfo() const = 0;
};


// to avoid a lot of repetitions
#define wxSCRIPTFNC_IMPLEMENT_CLONE(x)          \
   virtual wxScriptFunction *Clone() const {    \
      wxScriptFunction *newf = new x();         \
      newf->DeepCopy(this);                     \
      return newf; }    

// -----------------------------------------------------------------------------

// Francesco's scpython.h

// String conversions (all python functions use chars)
// ---------------------------------------------------

#if wxUSE_UNICODE
   #define WX2PY(x)      (wxString(x).mb_str(wxConvUTF8).data())
#else
   #define WX2PY(x)      (wxString(x).mb_str(wxConvUTF8))
#endif

#define PY2WX(x)         (wxString(x, wxConvUTF8))


#define PYSTRING2WX(x)   (PY2WX(PyString_AsString(x)))
#define WX2PYSTRING(x)   (PyString_FromString(WX2PY(x))


//! A Python interpreted function.
class wxScriptFunctionPython : public wxScriptFunction
{
protected:

   //! The python dictionary where this function is contained.
   PyObject *m_pDict;

   //! The Python function.
   PyObject *m_pFunc;

protected:

   //! Converts the given wxScriptVar into a PyObject.
   //! Returns a "borrowed reference" (see python docs).
   PyObject *CreatePyObjFromScriptVar(const wxScriptVar &toconvert) const;

   //! Converts the given PyObject into a wxScriptVar.
   wxScriptVar CreateScriptVarFromPyObj(PyObject *toconvert) const;

   //! Decreases the refcount of the python objects currently holded.
   void ReleaseOldObj();

public:

   //! Creates the object; no info about return value and
   //! arguments are required since wxPython does not store them.
   wxScriptFunctionPython(const wxString &name = wxEmptyString,
                  PyObject *dictionary = NULL, PyObject *func = NULL);
   virtual ~wxScriptFunctionPython();


   virtual wxScriptFunction *Clone() const {
      wxScriptFunction *newf = new wxScriptFunctionPython();
      newf->DeepCopy(this);
      return newf;
   }

   virtual void DeepCopy(const wxScriptFunction *tocopy);
   virtual void Set(const wxString &name, PyObject *dictionary, PyObject *function);
   virtual bool Exec(wxScriptVar &ret, wxScriptVar *arg) const;
};


//! A wxPython file script.
class wxScriptFilePython : public wxScriptFile
{
public:
   wxScriptFilePython(const wxString &toload = wxEmptyString) {      
      if (!toload.IsEmpty())
         Load(toload);      
   }

   virtual bool Load(const wxString &filename);
};


//! The wxPython interpreter.
class wxPython : public wxScriptInterpreter
{
public:

   //! The current python module.
   PyObject *m_pModule;

   //! The current dictionaries being used.
   PyObject *m_pLocals, *m_pGlobals;

   //! Recognizes the given type of python exception and sets
   //! the wxScriptInterpreter::m_strLastErr variable accordingly.
   void OnException();

public:
   wxPython() {}
   virtual ~wxPython() { Cleanup(); }

   //! Returns the global instance of this class.
   static wxPython *Get() { return m_pPython; }

   //! Inits Python interpreter.
   virtual bool Init();

   //! Undoes what #Init() does.
   virtual void Cleanup();

   //! Returns TRUE if Python is ready.
   virtual bool isReady() const;

   //! Returns the list of the functions currently recognized by the interpreter.
   virtual void GetFunctionList(wxScriptFunctionArray &) const;

   //! Uses the Py_GetVersion function to return a version string.
   virtual wxString GetVersionInfo() const
      { return wxT("Python ") + PY2WX(Py_GetVersion()); }
};

// -----------------------------------------------------------------------------

// Francesco's script.cpp

// setup static
wxString wxScriptInterpreter::m_strLastErr;
wxPython *wxScriptInterpreter::m_pPython = NULL;

// basic types
wxScriptTypeInfo *wxScriptTypeVOID = NULL;
wxScriptTypeInfo *wxScriptTypeCHAR = NULL;
wxScriptTypeInfo *wxScriptTypeINT = NULL;
wxScriptTypeInfo *wxScriptTypeLONG = NULL;
wxScriptTypeInfo *wxScriptTypeFLOAT = NULL;
wxScriptTypeInfo *wxScriptTypeDOUBLE = NULL;
wxScriptTypeInfo *wxScriptTypeBOOL = NULL;

// pointers to basic types
wxScriptTypeInfo *wxScriptTypePVOID = NULL;
wxScriptTypeInfo *wxScriptTypePCHAR = NULL;
wxScriptTypeInfo *wxScriptTypePINT = NULL;
wxScriptTypeInfo *wxScriptTypePLONG = NULL;
wxScriptTypeInfo *wxScriptTypePFLOAT = NULL;
wxScriptTypeInfo *wxScriptTypePDOUBLE = NULL;
wxScriptTypeInfo *wxScriptTypePBOOL = NULL;

// ----------------------
// wxSCRIPTINTERPRETER
// ----------------------

wxScriptInterpreter::wxScriptInterpreter()
{}

wxScriptInterpreter::~wxScriptInterpreter()
{}

bool wxScriptInterpreter::Init()
{
   // remove previous instance, if present
   Cleanup();

   m_pPython = new wxPython();
   m_pPython->Init();

   // create global objects
   wxScriptTypeVOID = new wxScriptTypeInfo(wxT("void"));
   wxScriptTypeINT = new wxScriptTypeInfo(wxT("int"));
   wxScriptTypeCHAR = new wxScriptTypeInfo(wxT("char"));
   wxScriptTypeLONG = new wxScriptTypeInfo(wxT("long"));
   wxScriptTypeFLOAT = new wxScriptTypeInfo(wxT("float"));
   wxScriptTypeDOUBLE = new wxScriptTypeInfo(wxT("double"));
   wxScriptTypeBOOL = new wxScriptTypeInfo(wxT("bool"));

   wxScriptTypePVOID = new wxScriptTypeInfo(wxT("void*"));
   wxScriptTypePINT = new wxScriptTypeInfo(wxT("int*"));
   wxScriptTypePCHAR = new wxScriptTypeInfo(wxT("char*"));
   wxScriptTypePLONG = new wxScriptTypeInfo(wxT("long*"));
   wxScriptTypePFLOAT = new wxScriptTypeInfo(wxT("float*"));
   wxScriptTypePDOUBLE = new wxScriptTypeInfo(wxT("double*"));
   wxScriptTypePBOOL = new wxScriptTypeInfo(wxT("bool*"));

   return areAllReady();
}

void wxScriptInterpreter::Cleanup()
{
   wxSAFE_DELETE(m_pPython);

   wxSAFE_DELETE(wxScriptTypeVOID);
   wxSAFE_DELETE(wxScriptTypeINT);
   wxSAFE_DELETE(wxScriptTypeCHAR);
   wxSAFE_DELETE(wxScriptTypeLONG);
   wxSAFE_DELETE(wxScriptTypeFLOAT);
   wxSAFE_DELETE(wxScriptTypeDOUBLE);
   wxSAFE_DELETE(wxScriptTypeBOOL);

   wxSAFE_DELETE(wxScriptTypePVOID);
   wxSAFE_DELETE(wxScriptTypePINT);
   wxSAFE_DELETE(wxScriptTypePCHAR);
   wxSAFE_DELETE(wxScriptTypePLONG);
   wxSAFE_DELETE(wxScriptTypePFLOAT);
   wxSAFE_DELETE(wxScriptTypePDOUBLE);
   wxSAFE_DELETE(wxScriptTypePBOOL);
}

bool wxScriptInterpreter::areAllReady()
{
   bool res = TRUE;

   if (m_pPython) res &= m_pPython->isReady();

   return res;
}

void wxScriptInterpreter::GetTotalFunctionList(wxScriptFunctionArray &arr)
{
   wxScriptFunctionArray arrpy;

   if (m_pPython && m_pPython->isReady()) m_pPython->GetFunctionList(arrpy);

   arr.Append(arrpy);
}

wxScriptFile *wxScriptInterpreter::Load(const wxString &filename)
{
   if (!wxFileName::FileExists(filename)) {
      wxScriptInterpreter::m_strLastErr = wxT("The script file does not exist: ") + filename;
      return NULL;
   }

   wxScriptFile *p = new wxScriptFilePython();
   
   if (p == NULL) {
      wxScriptInterpreter::m_strLastErr = wxT("Could not create wxScriptFilePython!");
      return NULL;
   }

   if (!p->Load(filename)) {
      // assume wxScriptFilePython::Load has set wxScriptInterpreter::m_strLastErr
      delete p;
      return NULL;
   }
   
   return p;
}


// --------------------
// wxSCRIPTTYPEINFO
// --------------------

void wxScriptTypeInfo::Set(const wxString &str)
{ 
   // the final form of the m_strName should be:
   //
   //    {int,long,char,float,double,...}[*,&]
   //     
   m_strName = str; 

   // remove leading & trailing spaces
   m_strName.Trim(TRUE);
   m_strName.Trim(FALSE);
   
   // cannot make lower the name of this variable with:
   //       m_strName = m_strName.MakeLower();
   // this would work with simple types like wxT("char"), "int"...
   // but it would destroy the case-sensitivity of the C/C++
   // language for user-types !

   // remove the CONST and VOLATILE keywords
   m_strName.Replace(wxT("const"), wxT(""));
   m_strName.Replace(wxT("volatile"), wxT(""));
   m_strName.Trim(TRUE); 
   m_strName.Trim(FALSE);

   // remove spaces before the "*" symbol
   m_strName.Replace(wxT(" *"), wxT("*"));
   m_strName.Replace(wxT(" &"), wxT("&"));

   // remove everything after the type
   if (m_strName.Contains(wxT(' ')))
      m_strName = m_strName.BeforeLast(wxT(' '));
}

bool wxScriptTypeInfo::isPointer() const
{
   if (m_strName.IsEmpty())
      return FALSE;
   if (m_strName.Last() != wxT('*'))
      return FALSE;
   return TRUE;
}

bool wxScriptTypeInfo::isReference() const
{
   if (m_strName.IsEmpty())
      return FALSE;
   if (m_strName.Last() != wxT('&'))
      return FALSE;
   return TRUE;
}

wxString wxScriptTypeInfo::GetPointerTypeName() const
{
   // just remove the last "*" placed at the end...
   if (!isPointer())
      return wxEmptyString;         // this is not a pointer !!
   return m_strName.Left(m_strName.Len()-1);
}

wxScriptTypeGeneric wxScriptTypeInfo::GetGenericType() const
{
   // check for basic types
   if (Match(wxScriptTypeINT))
      return wxSTG_INT;
   if (Match(wxScriptTypeLONG))
      return wxSTG_LONG;
   if (Match(wxScriptTypeCHAR))
      return wxSTG_CHAR;
   if (Match(wxScriptTypeFLOAT))
      return wxSTG_FLOAT;
   if (Match(wxScriptTypeDOUBLE))
      return wxSTG_DOUBLE;
   if (Match(wxScriptTypeBOOL))
      return wxSTG_BOOL;

   if (isPointer()) return wxSTG_POINTER;
   if (isReference()) return wxSTG_REFERENCE;

   // an user defined type ?
   return wxSTG_UNDEFINED;
}

void wxScriptTypeInfo::SetGenericType(wxScriptTypeGeneric t)
{
   switch (t) {
   case wxSTG_INT:
      m_strName = wxT("int");
      break;
   case wxSTG_LONG:
      m_strName = wxT("long");
      break;
   case wxSTG_CHAR:
      m_strName = wxT("char");
      break;
   case wxSTG_FLOAT:
      m_strName = wxT("float");
      break;
   case wxSTG_DOUBLE:
      m_strName = wxT("double");
      break;
   case wxSTG_BOOL:
      m_strName = wxT("bool");
      break;
   case wxSTG_VOID:
      m_strName = wxT("void");
      break;

   case wxSTG_POINTER:
   case wxSTG_REFERENCE:
   case wxSTG_USERDEFINED:
   case wxSTG_UNDEFINED:
      break;
   }
}


// --------------------
// wxSCRIPTVAR
// --------------------

void wxScriptVar::Copy(const wxScriptVar &var)
{
   // delete old contents (*before* setting the new type)
   ResetContent();

   // then, copy the type
   m_tType = var.m_tType;

   // then, copy the content using strings; as described
   // in #GetContentString() and #SetContent() functions,
   // no data loss should happen doing this...
   wxString content(var.GetContentString());
   SetContent(content);
}

void wxScriptVar::ResetContent()
{
   if (m_tType.GetGenericType() == wxSTG_POINTER && m_content != 0) {

      if (m_tType.GetPointerType().GetGenericType() == wxSTG_CHAR) {
         delete [] ((char*)m_content);
      } else {
         // AKT: safe way to avoid gcc warning???!!!
         // delete ((void*)m_content);
         delete ((char*)m_content);
      }
   }

   m_content = 0;
}

void wxScriptVar::SetContent(const wxString &str)
{
   ResetContent();

   switch (m_tType.GetGenericType()) {
   case wxSTG_INT:
   case wxSTG_LONG:
   case wxSTG_VOID:
      str.ToLong(&m_content, 10);
      break;

   case wxSTG_CHAR:
      m_content = (wxChar)str.GetChar(0);      
      break;

      // even floats & doubles are stored as strings: they could not
      // fit into an 
   case wxSTG_FLOAT:
   case wxSTG_DOUBLE:
      str.ToDouble(&m_floatcontent);
      break;

   case wxSTG_BOOL:

      // there are at least two ways to encode a boolean value into
      // a string: using a number (typically 0 or 1) or using
      // the "true"/"false" strings...
      if (str.IsNumber()) {

         // we're using a number
         str.ToLong(&m_content, 10);

         // normalize content evaluating its truth value
         m_content = (m_content) ? 1 : 0;

      } else {

         // we are using a string
         if (str.CmpNoCase(wxT("TRUE")) == 0)
            m_content = 1;
         else
            m_content = 0;
      }
      break;

   case wxSTG_POINTER:

      // an exception is for char*
      if (m_tType.GetPointerType().GetGenericType() == wxSTG_CHAR) {

         // create a memory buffer encoded in UTF8 standard
         wxCharBuffer cb(str.mb_str(wxConvUTF8));
         const char *original = cb.data();
         int asciilen = strlen(original);

         char *pmem = new char[asciilen+1];
         strcpy(pmem, original);

         // and then set that memory address as the content of this var
         m_content = (long)pmem;

      } else {

         // the pointer address should be expressed in hexadecimal form
         str.ToLong(&m_content, 16);
      }
      break;

   case wxSTG_REFERENCE:
   case wxSTG_USERDEFINED:
   case wxSTG_UNDEFINED:
      // FIXME: what can we do here ?
      break;
   }
}

wxString wxScriptVar::GetContentString() const
{   
   wxString res;

   // get a string representation of the var's contents
   switch (m_tType.GetGenericType()) {
   case wxSTG_INT:
   case wxSTG_LONG:
   case wxSTG_VOID:
      return wxString::Format(wxT("%d"), (int)m_content);

   case wxSTG_CHAR:
      return wxString((wxChar)m_content, 1);

   case wxSTG_FLOAT:
   case wxSTG_DOUBLE:
      return wxString::Format(wxT("%g"), m_floatcontent);

   case wxSTG_BOOL:
      return (m_content ? wxT("true") : wxT("false"));

   case wxSTG_POINTER:

      // an exception is for char*
      if (m_tType.GetPointerType().GetGenericType() == wxSTG_CHAR) {
         
         char *pmem = (char *)m_content;
         
         // AKT: return LUA2WX(pmem);
         return wxString(pmem, wxConvUTF8);


      } else {

         // the pointer address should be expressed in hexadecimal form
         return wxString::Format(wxT("%X"), (unsigned int)m_content);
      }
   
   case wxSTG_REFERENCE:
   case wxSTG_USERDEFINED:
   case wxSTG_UNDEFINED:
      // FIXME: what can we do here ?
      return wxString::Format(wxT("%d"), (int)m_content);
   }

   return res;
}


// --------------------
// wxSCRIPTFUNCTION
// --------------------

wxString wxScriptFunction::GetCallString(wxScriptVar *arg) const
{
   wxString cmd = m_strName + wxT("(");

   // create the string with the arguments...
   for (int i=0; i < m_nArgCount; i++) {
      if (i != 0) cmd += wxT(", ");
      cmd += wxT("\"") + arg[i].GetContentString() + wxT("\"");
   }
   
   cmd += wxT(")");
   return cmd;
}

void wxScriptFunction::Set(const wxString &name, const wxString &ret,
                       wxScriptTypeInfo *arr, int n)
{
   m_strName = name;
   m_tReturn.Set(ret);

   // copy arg list
   m_nArgCount = n;
   for (int i=0; i < n; i++)
      m_pArgList[i].DeepCopy(&arr[i]);
}

bool wxScriptFunction::Match(const wxScriptTypeInfo *ret, int argc, ...)
{
   // first of all, check return type & arg count
   if (!m_tReturn.Match(ret) || argc != GetArgCount())
      return FALSE;

   wxScriptTypeInfo *p = NULL;
   va_list marker;
   bool okay = TRUE;
   int n = 0;
   
   va_start(marker, argc);     /* Initialize variable arguments. */
   do {

      p = va_arg(marker, wxScriptTypeInfo *);

      if (p) {

         // check if n-th argument types coincides
         okay &= GetArgType(n).Match(p);
      }

      // check next arg
      n++;

   } while (p && n < argc);

   va_end(marker);              /* Reset variable arguments.      */

   return okay;
}


// -----------------------
// wxSCRIPTFUNCTIONARRAY
// -----------------------

void wxScriptFunctionArray::DeepCopy(const wxScriptFunctionArray *arr)
{
   // be sure to be empty....
   Clear();
   Append(*arr);

   // copy the prefix to strip
   m_strToStrip = arr->m_strToStrip;
}

void wxScriptFunctionArray::Append(const wxScriptFunctionArray &arr)
{
   // don't use the WX_APPEND_ARRAY macro
   // because it would just copy pointers; we must 
   // copy the wxScriptFunction objects
   for (int i=0; i < (int)arr.m_arr.GetCount(); i++)
      m_arr.Add(arr.Get(i)->Clone());
}

void wxScriptFunctionArray::Insert(wxScriptFunction *toadd, int idx)
{
   // always insert only one copy because we insert pointers and not
   // the objects itself; inserting twice the same memory address would
   // result in twice deletion of that same object !
   m_arr.Insert(toadd, idx, 1);
}

wxString wxScriptFunctionArray::GetName(int n) const
{
   wxString tmp = Get(n)->GetName();

   // remove the "prefix-to-strip" if present...
   if (tmp.Left(m_strToStrip.Len()) == m_strToStrip)
      tmp.Remove(0, m_strToStrip.Len());
   return tmp;
}

int wxScriptFunctionArray::GetIdx(const wxString &fncname, int n) const
{
   int occ = -1;
   for (int i=0; i < GetCount(); i++) {
      if (GetName(i) == fncname) {
         occ++;

         // is this the occurence we're looking for ?
         if (occ == n)
            return i;
      }
   }

   // not found
   return -1;
}

wxScriptFunction *wxScriptFunctionArray::Get(const wxString &fncname, int n) const
{
   int idx = GetIdx(fncname, n);

   if (idx >= 0)
      return Get(idx);
   return NULL;
}

int wxScriptFunctionArray::GetCountOf(const wxString &fncname) const
{
   int n=0;
   for (int i=0; i < GetCount(); i++)
      if (GetName(i) == fncname)
         n++;
   return n;
}

void wxScriptFunctionArray::Remove(int n)
{ 
   if (n < 0 || n >= GetCount())
      return;

   // we must delete the object
   delete Get(n);
   
   // and remove it from the array...
   m_arr.RemoveAt(n);
}

void wxScriptFunctionArray::Clear()
{
   for (int i=(int)m_arr.GetCount(); i > 0; i--)
      Remove(0);      // always remove the first: the array automatically shrinks

   // array should be already empty...
   m_arr.Clear();
}

// -----------------------------------------------------------------------------

// Francesco's scpython.cpp

// Since I don't know any way to check the python version, I'll use this
// simple trick to know if we have BOOL support
#ifdef Py_BOOLOBJECT_H
   #define wxPYTHON_HAS_BOOL
#endif


// -----------------
// wxPYTHON
// -----------------

bool wxPython::Init()
{
   // inizialize the python interpreter
   Py_Initialize();

   // initialize our "execution frame"
   m_pModule = PyImport_AddModule("__main__");

   // PyImport_AddModule returns a borrowed reference: we'll now transform
   // it to a full reference...
   Py_INCREF(m_pModule);

   // we'll do the same for our dictionary...
   m_pGlobals = PyModule_GetDict(m_pModule);
   Py_INCREF(m_pGlobals);

   // our locals dict will be a reference to the global one...
   m_pLocals = m_pGlobals;
   Py_INCREF(m_pLocals);

   // everything was okay ?
   return isReady();
}

void wxPython::Cleanup()
{
   // free all our references
   Py_DECREF(m_pModule);
   Py_DECREF(m_pGlobals);
   Py_DECREF(m_pLocals);

   // and everything else which is python-related
   Py_Finalize();
}

bool wxPython::isReady() const
{
   // returns !=0 is everything is okay
   return Py_IsInitialized() != 0;
}

void wxPython::GetFunctionList(wxScriptFunctionArray &arr) const
{
   // get the list of the callable objects stored into the
   // GLOBAL dictionary...
   try {

      // we'll scan the items of the globals dictionary
      // (which are given to us into a PyList)
      PyObject *list = PyDict_Values(m_pGlobals);
      if (!list || PyList_Check(list) == FALSE)
         return;
            
      for (int i=0,max=PyList_Size(list); i<max; i++) {
         
         // get the i-th item: this returns a borrowed reference...
         PyObject *elem = PyList_GetItem(list, i);
         if (elem && PyCallable_Check(elem) != 0 &&
            PyObject_HasAttrString(elem, "func_name")) {
            
            // this is a function; add it to the list
            PyObject *str = PyObject_GetAttrString(elem, "func_name");
            wxString name(PYSTRING2WX(str));

            // this is a function defined in the GLOBALS dictionary
            // and identified by the PyObject "elem"
            arr.Append(new wxScriptFunctionPython(name, m_pGlobals, elem));            
            Py_DECREF(str);
         }
      }

      Py_DECREF(list);
      
   } catch (...) {
      
      // if there was an exception, then something went wrong
      wxPython::Get()->OnException();
      return;
   }
}

void wxPython::OnException()
{
   wxASSERT(PyErr_Occurred() != NULL);
   wxString err;

#define pyEXC_HANDLE(x)                         \
   else if (PyErr_ExceptionMatches(PyExc_##x))  \
      err = wxString(wxT(#x));

   // a generic exception description
   if (PyErr_ExceptionMatches(PyExc_AssertionError))
      err = wxT("Assertion error");
   pyEXC_HANDLE(AttributeError)
   pyEXC_HANDLE(EOFError)
   pyEXC_HANDLE(FloatingPointError)
   pyEXC_HANDLE(IOError)
   pyEXC_HANDLE(ImportError)
   pyEXC_HANDLE(IndexError)
   pyEXC_HANDLE(KeyError)
   pyEXC_HANDLE(KeyboardInterrupt)
   pyEXC_HANDLE(MemoryError)
   pyEXC_HANDLE(NameError)
   pyEXC_HANDLE(NotImplementedError)
   pyEXC_HANDLE(OSError)
   pyEXC_HANDLE(OverflowError)
   pyEXC_HANDLE(RuntimeError)
   pyEXC_HANDLE(SyntaxError)
   pyEXC_HANDLE(SystemError)
   pyEXC_HANDLE(SystemExit)
   pyEXC_HANDLE(TypeError)
   pyEXC_HANDLE(ValueError)
   pyEXC_HANDLE(ZeroDivisionError)

   // AKT: added these
   pyEXC_HANDLE(Exception)
   pyEXC_HANDLE(StandardError)
   pyEXC_HANDLE(ArithmeticError)
   pyEXC_HANDLE(LookupError)
   pyEXC_HANDLE(EnvironmentError)

   else
      err = wxT("Unknown error");

   wxScriptInterpreter::m_strLastErr = wxT("Exception occurred: ") + err;
   
   PyErr_Clear();
}


// -------------------------
// wxSCRIPTFUNCTIONPYTHON
// -------------------------

wxScriptFunctionPython::wxScriptFunctionPython(const wxString &name,
                                    PyObject *dict, PyObject *func)
{
   m_pDict = NULL;
   m_pFunc = NULL;

   // set our name, dict & func pointers
   Set(name, dict, func);
}

wxScriptFunctionPython::~wxScriptFunctionPython()
{ 
   // free python objects
   ReleaseOldObj(); 
} 

void wxScriptFunctionPython::ReleaseOldObj()
{
   // we don't need these python objects anymore...
   if (m_pDict) Py_DECREF(m_pDict);
   if (m_pFunc) Py_DECREF(m_pFunc);

   m_pDict = NULL;
   m_pFunc = NULL;
}

void wxScriptFunctionPython::Set(const wxString &name, PyObject *dict, PyObject *func)
{
   ReleaseOldObj();

   // set name
   m_strName = name;

   // and our python vars
   m_pDict = dict;
   m_pFunc = func;

   if (!dict || !func) return;      // don't proceed

   // we now own a reference to these python objects
   Py_INCREF(m_pDict);
   Py_INCREF(m_pFunc);

   // get the number of arguments taken by this function
   wxASSERT(PyObject_HasAttrString(func, "func_code"));
   PyObject *code = PyObject_GetAttrString(func, "func_code");
   wxASSERT(PyObject_HasAttrString(code, "co_argcount"));
   PyObject *argc = PyObject_GetAttrString(code, "co_argcount");
   wxASSERT(PyInt_Check(argc));

   // convert the PyInt object to a number & store it
   m_nArgCount = (int)PyInt_AsLong(argc);
   Py_DECREF(code);
   Py_DECREF(argc);
}

bool wxScriptFunctionPython::Exec(wxScriptVar &ret, wxScriptVar *arg) const
{
   if (!m_pFunc) return FALSE;      // this is an invalid function

   // create the ruple with the arguments to give to the function
   PyObject *t = PyTuple_New(m_nArgCount);
   for (int i=0; i < m_nArgCount; i++) {

      // PyTuple_SetItem function will steal a reference from
      // CreatePyObjFromScriptVar so refcount should be okay doing nothing
      if (PyTuple_SetItem(t, i, CreatePyObjFromScriptVar(arg[i])) != 0) {

         wxScriptInterpreter::m_strLastErr = wxT("Could not create the argument tuple.");
         return FALSE;
      }
   }

   // do the function call
   try {
   
      PyObject *res = PyObject_CallObject(m_pFunc, t);
      Py_DECREF(t);      // we won't use the tuple anymore...

      if (res == NULL) {      // something wrong ?

         if (PyErr_Occurred() != NULL)
            wxPython::Get()->OnException();
         else
            wxScriptInterpreter::m_strLastErr = wxT("Unknown call error.");
         return FALSE;
      }
   
      // convert python result object to a wxScriptVar
      ret = CreateScriptVarFromPyObj(res);
      Py_DECREF(res);      // we won't use this anymore

   } catch (...) {

      // something wrong...
      wxPython::Get()->OnException();
      return FALSE;
   }   

   // finally we have finished...
   return TRUE;
}

PyObject *wxScriptFunctionPython::CreatePyObjFromScriptVar(const wxScriptVar &toconvert) const
{
   wxScriptTypeInfo t(toconvert.GetType());
   PyObject *res = NULL;
      
   if (t.Match(*wxScriptTypeINT) ||
      t.Match(*wxScriptTypeLONG) ||
      t.Match(*wxScriptTypeCHAR)) {

      res = PyInt_FromLong(toconvert.GetContentLong());

   } else if (t.Match(*wxScriptTypeFLOAT) || 
            t.Match(*wxScriptTypeDOUBLE)) {

      res = PyFloat_FromDouble(toconvert.GetContentDouble());

   } else if (t.Match(*wxScriptTypeBOOL)) {

#ifdef wxPYTHON_HAS_BOOL
      res = PyBool_FromLong(toconvert.GetContentLong());
#else
      res = PyInt_FromLong(toconvert.GetContentLong());
#endif

   } else if (t.isPointer()) {

      wxScriptTypeInfo pt = t.GetPointerType();
      if (pt.Match(wxScriptTypeCHAR))
         res = PyString_FromString(WX2PY(toconvert.GetContentString()));
      else
         res = PyBuffer_FromMemory(toconvert.GetPointer(), 0);

   }

   return res;
}

wxScriptVar wxScriptFunctionPython::CreateScriptVarFromPyObj(PyObject *toconvert) const
{
   wxScriptVar ret;

   // the order is important (a PyBool is a subtype of a PyInt) !!
#ifdef wxPYTHON_HAS_BOOL
   if (PyBool_Check(toconvert)) {

      // compare this boolean object to the Py_True object
      int res;
      if (PyObject_Cmp(toconvert, Py_True, &res) == -1) {

         ret.GetType().SetGenericType(wxSTG_UNDEFINED);
         return ret;
      }

      // and then set the bool scriptvar
      if (res == -1)
         ret.Set(wxSTG_BOOL, (bool)FALSE);
      else
         ret.Set(wxSTG_BOOL, (bool)TRUE);

   } else 
#endif
   if (PyInt_Check(toconvert)) {

      ret.Set(wxSTG_INT, (long)PyInt_AsLong(toconvert));

   } else if (PyLong_Check(toconvert)) {

      ret.Set(wxSTG_LONG, (long)PyLong_AsLong(toconvert));

   } else if (PyFloat_Check(toconvert)) {

      ret.Set(wxSTG_DOUBLE, (double)PyFloat_AsDouble(toconvert));

   } else if (PyObject_CheckReadBuffer(toconvert)) {

      const void *buffer;
      int len;
      PyObject_AsReadBuffer(toconvert, &buffer, &len);
      ret.Set(wxT("void*"), wxString::Format(wxT("%X"), (unsigned int)buffer));

   } else if (PyString_Check(toconvert)) {

      ret.Set(wxT("char*"), PY2WX(PyString_AsString(toconvert)));
   }

   return ret;
}

void wxScriptFunctionPython::DeepCopy(const wxScriptFunction *tocopy)
{
   wxScriptFunctionPython *pf = (wxScriptFunctionPython *)tocopy;

   // release old objects
   ReleaseOldObj();

   // inc the refcount of new objects
   m_pDict = pf->m_pDict;
   m_pFunc = pf->m_pFunc;
   Py_INCREF(m_pDict);
   Py_INCREF(m_pFunc);

   wxScriptFunction::DeepCopy(tocopy);
}


// --------------------
// wxSCRIPTFILEPYTHON
// --------------------

// AKT:
wxString gollydir;      // location of Golly app
wxString scriptdir;     // location of script file

bool wxScriptFilePython::Load(const wxString &filename)
{
   try {

      // if filename contains backslashes then we must convert them to "\\"
      // to avoid "\a" being treated as escape char
      wxString fname = filename;
      fname.Replace("\\", "\\\\");

      // build absolute path to Golly's Scripts folder
      wxString scriptsdir = gollydir + wxT("Scripts");
      scriptsdir.Replace("\\", "\\\\");

      // use PyRun_SimpleString to add Golly's Scripts folder to Python's
      // import search list (so scripts anywhere can do "from glife import *")
      // and then execute the given script
      wxString command = wxT("import sys ; sys.path.append(\"") + scriptsdir + wxT("\")");
      command += wxT(" ; execfile(\"") + fname + wxT("\")");
      PyRun_SimpleString(command.c_str());

   } catch (...) {

      wxPython::Get()->OnException();
      return FALSE;
   }

   return TRUE;
}

// -----------------------------------------------------------------------------

// the following golly_* routines can be called from Python scripts; some are
// based on code in PLife's lifeint.cc (see http://plife.sourceforge.net/)

static PyObject *golly_new(PyObject *self, PyObject *args)
{
   wxUnusedVar(self);
   char *title = NULL;

   if (!PyArg_ParseTuple(args, "z", &title)) return NULL;

   mainptr->NewPattern();
   if (title != NULL && title[0] != 0)
      mainptr->SetWindowTitle(title);

   Py_INCREF(Py_None);
   return Py_None;
}

static PyObject *golly_fit(PyObject *self, PyObject *args)
{
   wxUnusedVar(self);

   if (!PyArg_ParseTuple(args, "")) return NULL;

   viewptr->FitPattern();

   Py_INCREF(Py_None);
   return Py_None;
}

static PyObject *golly_setrule(PyObject *self, PyObject *args)
{
   wxUnusedVar(self);
   char *rule_string = NULL;

   if (!PyArg_ParseTuple(args, "z", &rule_string)) return NULL;

   wxString oldrule = wxT( curralgo->getrule() );
   const char *err;
   if (rule_string == NULL || rule_string[0] == 0) {
      err = curralgo->setrule("B3/S23");
   } else {
      err = curralgo->setrule(rule_string);
   }
   if (err) {
      Warning(err);
      curralgo->setrule( (char*)oldrule.c_str() );
   } else if ( global_liferules.hasB0notS8 && hashing ) {
      Warning("B0-not-S8 rules are not allowed when hashing.");
      curralgo->setrule( (char*)oldrule.c_str() );
   } else {
      // show new rule in main window's title
      mainptr->SetWindowTitle("");
   }

   Py_INCREF(Py_None);
   return Py_None;
}

// helper routine used in calls that build cell lists
static void AddCell(PyObject *list, long x, long y)
{
   PyList_Append(list, PyInt_FromLong(x));
   PyList_Append(list, PyInt_FromLong(y));
}   

// helper routine to extract cell list from given universe
static void ExtractCells(PyObject *list, lifealgo *universe, bool shift = false)
{
   if ( !universe->isEmpty() ) {
      bigint top, left, bottom, right;
      universe->findedges(&top, &left, &bottom, &right);
      int itop = top.toint();
      int ileft = left.toint();
      int ibottom = bottom.toint();
      int iright = right.toint();
      int cx, cy;
      for ( cy=itop; cy<=ibottom; cy++ ) {
         for ( cx=ileft; cx<=iright; cx++ ) {
            int skip = universe->nextcell(cx, cy);
            if (skip >= 0) {
               // found next live cell in this row
               cx += skip;
               if (shift) {
                  // shift cells so that top left cell of bounding box is at 0,0
                  AddCell(list, cx - ileft, cy - itop);
               } else {
                  AddCell(list, cx, cy);
               }
            } else {
               cx = iright;  // done this row
            }
         }
      }
   }
}

static PyObject *golly_parse(PyObject *self, PyObject *args)
{
   wxUnusedVar(self);
   char *s;

   long x0, y0, axx, axy, ayx, ayy;

   if (!PyArg_ParseTuple(args, "zllllll", &s, &x0, &y0, &axx, &axy, &ayx, &ayy))
      return NULL;

   PyObject *list = PyList_New (0);

   long x = 0, y = 0;

   if (strchr(s, '*')) {
      // parsing 'visual' format
      int c = *s++;
      while (c) {
         switch (c) {
         case '\n': if (x) { x = 0; y++; } break;
         case '.': x++; break;
         case '*':
            AddCell(list, x0 + x * axx + y * axy, y0 + x * ayx + y * ayy);
            x++;
            break;
         }
         c = *s++;
      }
   } else {
      // parsing 'RLE' format
      int prefix = 0;
      bool done = false;
      int c = *s++;
      while (c && !done) {
         if (isdigit(c))
            prefix = 10 * prefix + (c - '0');
         else {
            prefix += (prefix == 0);
            switch (c) {
            case '!': done = true; break;
            case '$': x = 0; y += prefix; break;
            case 'b': x += prefix; break;
            case 'o':
               for (int k = 0; k < prefix; k++, x++)
                  AddCell(list, x0 + x * axx + y * axy, y0 + x * ayx + y * ayy);
               break;
            }
            prefix = 0;
         }
         c = *s++;
      }
   }

   return list;
}

static PyObject *golly_transform(PyObject *self, PyObject *args)
{
   wxUnusedVar(self);
   long x0, y0, axx, axy, ayx, ayy;

   PyObject *list;
   PyObject *new_list;

   if (!PyArg_ParseTuple(args, "O!llllll", &PyList_Type, &list, &x0, &y0, &axx, &axy, &ayx, &ayy))
      return NULL;

   new_list = PyList_New (0);

   int num_cells = PyList_Size (list) / 2;
   for (int n = 0; n < num_cells; n++) {
      long x = PyInt_AsLong (PyList_GetItem (list, 2 * n));
      long y = PyInt_AsLong (PyList_GetItem (list, 2 * n + 1));

      AddCell(new_list, x0 + x * axx + y * axy, y0 + x * ayx + y * ayy);
   }

   return new_list;
}

static PyObject *golly_putcells(PyObject *self, PyObject *args)
{
   wxUnusedVar(self);
   long x0, y0, axx, axy, ayx, ayy;
   PyObject *list;

   if (!PyArg_ParseTuple(args, "O!llllll", &PyList_Type, &list, &x0, &y0, &axx, &axy, &ayx, &ayy))
      return NULL;

   int num_cells = PyList_Size(list) / 2;
   for (int n = 0; n < num_cells; n++) {
      long x = PyInt_AsLong( PyList_GetItem(list, 2 * n) );
      long y = PyInt_AsLong( PyList_GetItem(list, 2 * n + 1) );

      // paste (possibly transformed) cell into current universe
      curralgo->setcell(x0 + x * axx + y * axy, y0 + x * ayx + y * ayy, 1);
   }
   curralgo->endofpattern();
   mainptr->savestart = true;
   mainptr->UpdatePatternAndStatus();

   Py_INCREF(Py_None);
   return Py_None;
}

static PyObject *golly_evolve(PyObject *self, PyObject *args)
{
   wxUnusedVar(self);
   int N = 0;

   PyObject *given_list;
   PyObject *evolved_list;

   if (!PyArg_ParseTuple(args, "O!i", &PyList_Type, &given_list, &N)) return NULL;

   // create temporary qlife universe
   lifealgo *tempalgo;
   tempalgo = new qlifealgo();
   tempalgo->setpoll(wxGetApp().Poller());
   
   // no need for this -- all universes share global rule table
   // tempalgo->setrule( curralgo->getrule() );
   
   // copy cell list into temporary universe
   int num_cells = PyList_Size (given_list) / 2;
   for (int n = 0; n < num_cells; n++) {
      long x = PyInt_AsLong( PyList_GetItem(given_list, 2 * n) );
      long y = PyInt_AsLong( PyList_GetItem(given_list, 2 * n + 1) );
      tempalgo->setcell(x, y, 1);
   }
   tempalgo->endofpattern();

   // advance pattern by N gens
   mainptr->generating = true;
   wxGetApp().PollerReset();
   tempalgo->setIncrement(N);
   tempalgo->step();
   mainptr->generating = false;

   // convert new pattern into a new cell list
   evolved_list = PyList_New(0);
   ExtractCells(evolved_list, tempalgo);
   delete tempalgo;

   return evolved_list;
}

static PyObject *golly_load(PyObject *self, PyObject *args)
{
   wxUnusedVar(self);
   PyObject *list;
   char *file_name;

   if (!PyArg_ParseTuple(args, "z", &file_name)) return NULL;

   // create temporary qlife universe
   lifealgo *tempalgo;
   tempalgo = new qlifealgo();
   tempalgo->setpoll(wxGetApp().Poller());
   
   // readpatterm might change global rule table
   wxString oldrule = wxT( curralgo->getrule() );
   
   // read pattern into temporary universe
   const char *err = readpattern(file_name, *tempalgo);
   if (err && strcmp(err,cannotreadhash) == 0) {
      // macrocell file, so switch to hlife universe
      delete tempalgo;
      tempalgo = new hlifealgo();
      tempalgo->setpoll(wxGetApp().Poller());
      err = readpattern(file_name, *tempalgo);
   }
   
   // restore rule
   curralgo->setrule( (char*)oldrule.c_str() );

   if (err) {
      delete tempalgo;
      Warning(err);
      return NULL;
   }
   
   // convert pattern into a cell list
   list = PyList_New(0);
   ExtractCells(list, tempalgo, true);    // true = shift so bbox's top left cell is at 0,0
   delete tempalgo;

   return list;
}

static PyObject *golly_save(PyObject *self, PyObject *args)
{
   wxUnusedVar(self);
   PyObject *given_list;
   char *file_name;
   char *s = NULL;      // the description string is currently ignored!!!

   if (!PyArg_ParseTuple(args, "O!z|z", &PyList_Type, &given_list, &file_name, &s))
      return NULL;

   // create temporary qlife universe
   lifealgo *tempalgo;
   tempalgo = new qlifealgo();
   tempalgo->setpoll(wxGetApp().Poller());
   
   // copy cell list into temporary universe
   int num_cells = PyList_Size (given_list) / 2;
   for (int n = 0; n < num_cells; n++) {
      long x = PyInt_AsLong( PyList_GetItem(given_list, 2 * n) );
      long y = PyInt_AsLong( PyList_GetItem(given_list, 2 * n + 1) );
      tempalgo->setcell(x, y, 1);
   }
   tempalgo->endofpattern();

   // write pattern to given file in RLE format
   bigint top, left, bottom, right;
   tempalgo->findedges(&top, &left, &bottom, &right);
   const char *err = writepattern(file_name, *tempalgo, RLE_format,
                                  top.toint(), left.toint(), bottom.toint(), right.toint());
   delete tempalgo;
   if (err) Warning(err);

   Py_INCREF(Py_None);
   return Py_None;
}

static PyObject *golly_show(PyObject *self, PyObject *args)
{
   wxUnusedVar(self);
   char *s = NULL;

   if (!PyArg_ParseTuple(args, "z", &s)) return NULL;

   statusptr->DisplayMessage(s);

   Py_INCREF(Py_None);
   return Py_None;
}

static PyObject *golly_warn(PyObject *self, PyObject *args)
{
   wxUnusedVar(self);
   char *s = NULL;

   if (!PyArg_ParseTuple(args, "z", &s)) return NULL;

   Warning(s);

   Py_INCREF(Py_None);
   return Py_None;
}

wxString pyerror;

static PyObject *golly_stderr(PyObject *self, PyObject *args)
{
   wxUnusedVar(self);
   char *s = NULL;

   if (!PyArg_ParseTuple(args, "z", &s)) return NULL;

   // save Python's stderr messages in global string for display after script finishes
   pyerror = wxT(s);

   Py_INCREF(Py_None);
   return Py_None;
}

/* this example shows how to take 2 int args and return an int value
static PyObject *golly_sum (PyObject *self, PyObject *args)
{
   wxUnusedVar(self);
   int n1 = 0;
   int n2 = 0;

   if (!PyArg_ParseTuple(args, "ii", &n1, &n2)) return NULL;

   PyObject *result = Py_BuildValue("i", n1 + n2);
   return result;
}
*/

// -----------------------------------------------------------------------------

static PyMethodDef golly_methods[] = {
   { "new",       golly_new,        METH_VARARGS, "create new universe and optionally set title" },
   { "fit",       golly_fit,        METH_VARARGS, "fit entire pattern in current view" },
   { "setrule",   golly_setrule,    METH_VARARGS, "set current rule according to string" },
   { "parse",     golly_parse,      METH_VARARGS, "parse RLE or Life 1.05 string and return cell list" },
   { "transform", golly_transform,  METH_VARARGS, "apply an affine transformation to cell list" },
   { "putcells",  golly_putcells,   METH_VARARGS, "paste given cell list into Golly universe" },
   { "evolve",    golly_evolve,     METH_VARARGS, "evolve pattern contained in given cell list" },
   { "load",      golly_load,       METH_VARARGS, "load pattern from file and return cell list" },
   { "save",      golly_save,       METH_VARARGS, "save cell list to a file (in RLE format)" },
   { "show",      golly_show,       METH_VARARGS, "show given string in status bar" },
   { "warn",      golly_warn,       METH_VARARGS, "show given string in warning dialog" },
   { "stderr",    golly_stderr,     METH_VARARGS, "show Python error message" },
   { NULL, NULL, 0, NULL }
};

// -----------------------------------------------------------------------------

void RunScript(const char* filename)
{
   wxString fname = wxT(filename);
   statusptr->ClearMessage();
   pyerror = wxEmptyString;
   
   if (!wxScriptInterpreter::Init()) {
      Warning("Could not initialize the Python interpreter!  Is it installed?");
      wxScriptInterpreter::Cleanup();
      return;
   }
   
   // let user know we're busy running a script
   wxSetCursor(*wxHOURGLASS_CURSOR);

   // allow Python to call the above golly_* routines
   Py_InitModule3("golly", golly_methods, "Internal golly routines");

   // temporarily change current directory to location of script
   gollydir = wxFileName::GetCwd();
   if (gollydir.Last() != wxFILE_SEP_PATH) gollydir += wxFILE_SEP_PATH;
   //!!!Warning(gollydir.c_str());
   wxFileName fullname(fname);
   fullname.Normalize();
   scriptdir = fullname.GetPath();
   if (!scriptdir.IsEmpty()) {
      if (scriptdir.Last() != wxFILE_SEP_PATH) scriptdir += wxFILE_SEP_PATH;
      wxSetWorkingDirectory(scriptdir);
      //!!!Warning(scriptdir.c_str());
   }

   // load the script
   wxScriptFile *pf = wxScriptInterpreter::Load(fname);
   if (pf == NULL) {
      // assume wxScriptInterpreter::m_strLastErr has been set
      Warning(wxScriptInterpreter::m_strLastErr.c_str());
   } else {
      delete pf;
   }
   
   // restore current directory to location of Golly app
   if (!scriptdir.IsEmpty()) {
      wxSetWorkingDirectory(gollydir);
   }

   wxScriptInterpreter::Cleanup();
   
   // display any Python error message
   if (!pyerror.IsEmpty()) {
      wxBell();
      wxSetCursor(*wxSTANDARD_CURSOR);
      wxMessageBox(pyerror, wxT("Python error:"), wxOK | wxICON_EXCLAMATION, wxGetActiveWindow());
   }
   
   // update cursor
   viewptr->CheckCursor(mainptr->IsActive());
}

// -----------------------------------------------------------------------------

bool IsScript(const char *filename)
{
   // currently we only support Python scripts so return true if filename
   // ends with ".py", ignoring case
   wxString fname = wxT(filename);
   wxString ext = fname.AfterLast(wxT('.'));

   // false means case-insensitive comparison
   return ext.IsSameAs(wxT("PY"), false);
}
