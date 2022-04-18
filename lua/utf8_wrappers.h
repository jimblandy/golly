/**
 * Wrappers to provide Unicode (UTF-8) support on Windows.
 *
 * Copyright (c) 2018 Peter Wu <peter@lekensteyn.nl>
 * SPDX-License-Identifier: (GPL-2.0-or-later OR MIT)
 */

#ifdef _WIN32
#include <windows.h>    /* AKT: needed for HMODULE etc */

#if defined(loadlib_c) || defined(lauxlib_c) || defined(liolib_c) || defined(luac_c)
#include <stdio.h>  /* for loadlib_c */
FILE *fopen_utf8(const char *pathname, const char *mode);
#define fopen               fopen_utf8
#endif

#ifdef lauxlib_c
FILE *freopen_utf8(const char *pathname, const char *mode, FILE *stream);
#define freopen             freopen_utf8
#endif

#ifdef liolib_c
FILE *popen_utf8(const char *command, const char *mode);
#define _popen              popen_utf8
#endif

#ifdef loslib_c
int remove_utf8(const char *pathname);
int rename_utf8(const char *oldpath, const char *newpath);
int system_utf8(const char *command);
#define remove              remove_utf8
#define rename              rename_utf8
#define system              system_utf8
#endif

#ifdef loadlib_c
DWORD GetModuleFileNameA_utf8(HMODULE hModule, LPSTR lpFilename, DWORD nSize);
HMODULE LoadLibraryExA_utf8(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
#define GetModuleFileNameA  GetModuleFileNameA_utf8
#define LoadLibraryExA      LoadLibraryExA_utf8
#endif
#endif

#if defined(lua_c) || defined(luac_c)
int main_utf8(int argc, char *argv[]);
#define main                main_utf8
#endif
