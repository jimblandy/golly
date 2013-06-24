/*** /

 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2013 Andrew Trevorrow and Tomas Rokicki.

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

#ifndef _CONTROL_H_
#define _CONTROL_H_

#include "bigint.h"     // for bigint
#include "algos.h"      // for algo_type

#include <string>		// for std::string
#include <list>		    // for std::list

// Data and routines for generating patterns:

extern bool generating;     // currently generating pattern?
extern int minexpo;         // step exponent at maximum delay (must be <= 0)

bool StartGenerating();
void StopGenerating();
void NextGeneration(bool useinc);
void ResetPattern(bool resetundo = true);
void RestorePattern(bigint& gen, const char* filename,
                    bigint& x, bigint& y, int mag, int base, int expo);
void SetMinimumStepExponent();
void SetStepExponent(int newexpo);
void SetGenIncrement();
bool CreateBorderCells(lifealgo* curralgo);
bool DeleteBorderCells(lifealgo* curralgo);
void ClearRect(lifealgo* curralgo, int top, int left, int bottom, int right);
const char* ChangeGenCount(const char* genstring, bool inundoredo = false);
void ClearOutsideGrid();
void ReduceCellStates(int newmaxstate);
void ChangeRule(const std::string& rulestring);
void ChangeAlgorithm(algo_type newalgotype, const char* newrule = "", bool inundoredo = false);
std::string CreateRuleFiles(std::list<std::string>& deprecated,
                            std::list<std::string>& keeprules);

#endif
