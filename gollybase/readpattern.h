// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef READPATTERN_H
#define READPATTERN_H
#include "bigint.h"
class lifealgo ;

/*
 *   Read pattern file into given life algorithm implementation.
 */
const char *readpattern(const char *filename, lifealgo &imp) ;

/*
 *   Get next line from current pattern file.
 */
char *getline(char *line, int maxlinelen) ;

/*
 *   Similar to readpattern but we return the pattern edges
 *   (not necessarily the minimal bounding box; eg. if an
 *   RLE pattern is empty or has empty borders).
 */
const char *readclipboard(const char *filename, lifealgo &imp,
                          bigint *t, bigint *l, bigint *b, bigint *r) ;

/*
 *   Extract comments from pattern file and store in given buffer.
 *   It is the caller's job to free commptr when done (if not NULL).
 */
const char *readcomments(const char *filename, char **commptr) ;

#endif
