// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _FILE_H_
#define _FILE_H_

#include <string>           // for std::string

#include "writepattern.h"   // for pattern_format, output_compression

// Routines for opening, saving, unzipping, downloading files:

void NewPattern(const char* title = "untitled");
bool LoadPattern(const char* path, const char* newtitle);
void SetPatternTitle(const char* filename);
bool SaveCurrentLayer();
void CreateUniverse();
void OpenFile(const char* path, bool remember = true);
bool CopyTextToClipboard(const char* text);
bool GetTextFromClipboard(std::string& text);
bool SavePattern(const std::string& path, pattern_format format, output_compression compression);
const char* WritePattern(const char* path, pattern_format format, output_compression compression,
                         int top, int left, int bottom, int right);
void UnzipFile(const std::string& zippath, const std::string& entry);
void GetURL(const std::string& url, const std::string& pageurl);
bool DownloadFile(const std::string& url, const std::string& filepath);
void ProcessDownload(const std::string& filepath);
void LoadLexiconPattern(const std::string& lexpattern);
void LoadRule(const std::string& rulestring);
std::string GetBaseName(const char* path);

#endif
