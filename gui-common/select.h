// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _SELECT_H_
#define _SELECT_H_

#include "bigint.h"     // for bigint
#include "lifealgo.h"   // for lifealgo

#include "utils.h"      // for gRect

// Most editing functions operate on the current selection.
// The Selection class encapsulates all selection-related operations.

class Selection {
public:
    Selection();
    Selection(int t, int l, int b, int r);
    ~Selection();

    bool operator==(const Selection& sel) const;
    bool operator!=(const Selection& sel) const;

    bool Exists();
    // return true if the selection exists

    void Deselect();
    // remove the selection

    bool TooBig();
    // return true if any selection edge is outside the editable limits

    void DisplaySize();
    // display the selection's size in the status bar

    void SetRect(int x, int y, int wd, int ht);
    // set the selection to the given rectangle

    void GetRect(int* x, int* y, int* wd, int* ht);
    // return the selection rectangle

    void SetEdges(bigint& t, bigint& l, bigint& b, bigint& r);
    // set the selection using the given rectangle edges

    void CheckGridEdges();
    // change selection edges if necessary to ensure they are inside a bounded grid

    bool Contains(bigint& t, bigint& l, bigint& b, bigint& r);
    // return true if the selection encloses the given rectangle

    bool Outside(bigint& t, bigint& l, bigint& b, bigint& r);
    // return true if the selection is completely outside the given rectangle

    bool ContainsCell(int x, int y);
    // return true if the given cell is within the selection

    void Advance();
    // advance the pattern inside the selection by one generation

    void AdvanceOutside();
    // advance the pattern outside the selection by one generation

    void Modify(const int x, const int y,
                bigint& anchorx, bigint& anchory,
                bool* forceh, bool* forcev);
    // modify the existing selection based on where the user tapped (inside)

    void Move(const bigint& xcells, const bigint& ycells);
    // move the selection by the given number of cells

    void SetLeftRight(const bigint& x, const bigint& anchorx);
    // set the selection's left and right edges

    void SetTopBottom(const bigint& y, const bigint& anchory);
    // set the selection's top and bottom edges

    void Fit();
    // fit the selection inside the current viewport

    void Shrink(bool fit);
    // shrink the selection so it just encloses all the live cells
    // and optionally fit the new selection inside the current viewport

    bool Visible(gRect* visrect);
    // return true if the selection is visible in the current viewport
    // and, if visrect is not NULL, set it to the visible rectangle

    void Clear();
    // kill all cells inside the selection

    void ClearOutside();
    // kill all cells outside the selection

    void CopyToClipboard(bool cut);
    // copy the selection to the clipboard (using RLE format) and
    // optionally clear the selection if cut is true

    bool CanPaste(const bigint& wd, const bigint& ht, bigint& top, bigint& left);
    // return true if the selection fits inside a rectangle of size ht x wd;
    // if so then top and left are set to the selection's top left corner

    void RandomFill();
    // randomly fill the selection

    bool Flip(bool topbottom, bool inundoredo);
    // return true if selection was successfully flipped

    bool Rotate(bool clockwise, bool inundoredo);
    // return true if selection was successfully rotated

private:
    bool SaveOutside(bigint& t, bigint& l, bigint& b, bigint& r);
    // remember live cells outside the selection

    void EmptyUniverse();
    // kill all cells by creating a new, empty universe

    void AddRun(int state, int multistate, unsigned int &run,
                unsigned int &linelen, char* &chptr);

    void AddEOL(char* &chptr);
    // these routines are used by CopyToClipboard to create RLE data

    bool SaveDifferences(lifealgo* oldalgo, lifealgo* newalgo,
                         int itop, int ileft, int ibottom, int iright);
    // compare same rectangle in the given universes and remember the differences
    // in cell states; return false only if user aborts lengthy comparison

    bool FlipRect(bool topbottom, lifealgo* srcalgo, lifealgo* destalgo, bool erasesrc,
                  int top, int left, int bottom, int right);
    // called by Flip to flip given rectangle from source universe to
    // destination universe and optionally kill cells in the source rectangle;
    // return false only if user aborts lengthy flip

    bool RotateRect(bool clockwise, lifealgo* srcalgo, lifealgo* destalgo, bool erasesrc,
                    int itop, int ileft, int ibottom, int iright,
                    int ntop, int nleft, int nbottom, int nright);
    // called by Rotate to rotate given rectangle from source universe to
    // destination universe and optionally kill cells in the source rectangle;
    // return false only if user aborts lengthy rotation

    bool RotatePattern(bool clockwise,
                       bigint& newtop, bigint& newbottom,
                       bigint& newleft, bigint& newright,
                       bool inundoredo);
    // called by Rotate when the selection encloses the entire pattern;
    // return false only if user aborts lengthy rotation

    bigint seltop, selleft, selbottom, selright;
    // currently we only support a single rectangular selection
    // which is represented by these edges; eventually we might
    // support arbitrarily complex selection shapes by maintaining
    // a list or dynamic array of non-overlapping rectangles

    bool exists;      // does the selection exist?
};

#endif
