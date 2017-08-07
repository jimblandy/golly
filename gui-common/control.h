// This file is part of Golly.
// See docs/License.html for the copyright notice.

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
const char* ChangeGenCount(const char* genstring, bool inundoredo = false);
void ClearOutsideGrid();
void ReduceCellStates(int newmaxstate);
void ChangeRule(const std::string& rulestring);
void ChangeAlgorithm(algo_type newalgotype, const char* newrule = "", bool inundoredo = false);
std::string CreateRuleFiles(std::list<std::string>& deprecated,
                            std::list<std::string>& keeprules);

#endif
