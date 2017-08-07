// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef WRITEPATTERN_H
#define WRITEPATTERN_H
class lifealgo;

typedef enum {
   RLE_format,          // run length encoded
   XRLE_format,         // extended RLE
   MC_format            // macrocell (native hashlife format)
} pattern_format;

typedef enum {
   no_compression,      // write uncompressed data
   gzip_compression     // write gzip compressed data
} output_compression;

/*
 *   Save current pattern to a file.
 */
const char *writepattern(const char *filename,
                         lifealgo &imp,
                         pattern_format format,
                         output_compression compression,
                         int top, int left, int bottom, int right);

#endif
