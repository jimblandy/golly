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

#include <sys/time.h>	// for gettimeofday
#include <algorithm>	// for std::transform
#include <locale>		// for std::tolower

#include "lifepoll.h"   // for lifepoll
#include "util.h"   	// for linereader

#include "prefs.h"		// for allowbeep, tempdir
#include "utils.h"

//!!! #ifdef ANDROID_GUI
#include "jnicalls.h"	// for UpdateStatus

// -----------------------------------------------------------------------------

int event_checker = 0;      // if > 0 then we're in gollypoller.checkevents()

// -----------------------------------------------------------------------------

void SetColor(gColor& color, unsigned char red, unsigned char green, unsigned char blue)
{
    color.r = red;
    color.g = green;
    color.b = blue;
}

// -----------------------------------------------------------------------------

void SetRect(gRect& rect, int x, int y, int width, int height)
{
    rect.x = x;
    rect.y = y;
    rect.width = width;
    rect.height = height;
}

// -----------------------------------------------------------------------------

bool YesNo(const char* msg)
{
    Beep();

    //!!!
    return false;
}

// -----------------------------------------------------------------------------

void Warning(const char* msg)
{
    Beep();

    //!!!
    LOGE("WARNING: %s", msg);
}

// -----------------------------------------------------------------------------

void Fatal(const char* msg)
{
    Beep();

    //!!!
    LOGE("FATAL ERROR: %s", msg);

    //!!!??? System.exit(1);
    exit(1);
}

// -----------------------------------------------------------------------------

void Beep()
{
    if (allowbeep) {
        //!!!
    }
}

// -----------------------------------------------------------------------------

double TimeInSeconds()
{
    struct timeval trec;
    gettimeofday(&trec, 0);
    return double(trec.tv_sec) + double(trec.tv_usec) / 1.0e6;
}

// -----------------------------------------------------------------------------

static int nextname = 0;

std::string CreateTempFileName(const char* prefix)
{
    /*
    std::string tmplate = tempdir;
    tmplate += prefix;
    tmplate += ".XXXXXX";
    std::string path = mktemp((char*)tmplate.c_str());
    */

    // simpler to ignore prefix and create /tmp/0, /tmp/1, /tmp/2, etc
    char n[32];
    sprintf(n, "%d", nextname++);
    std::string path = tempdir + n;

    return path;
}

// -----------------------------------------------------------------------------

bool FileExists(const std::string& filepath)
{
    FILE* f = fopen(filepath.c_str(), "r");
    if (f) {
        fclose(f);
        return true;
    } else {
        return false;
    }
}

// -----------------------------------------------------------------------------

void RemoveFile(const std::string& filepath)
{
    //!!!
}

// -----------------------------------------------------------------------------

bool CopyFile(const std::string& inpath, const std::string& outpath)
{
    FILE* infile = fopen(inpath.c_str(), "r");
    if (infile) {
    	// read entire file into contents
    	std::string contents;
        const int MAXLINELEN = 4095;
        char linebuf[MAXLINELEN + 1];
        linereader reader(infile);
        while (true) {
            if (reader.fgets(linebuf, MAXLINELEN) == 0) break;
            contents += linebuf;
            contents += "\n";
        }
        reader.close();
        // fclose(infile) has been called

        // write contents to outpath
        FILE* outfile = fopen(outpath.c_str(), "w");
        if (outfile) {
            if (fputs(contents.c_str(), outfile) == EOF) {
                fclose(outfile);
                Warning("CopyFile failed to copy contents to output file!");
                return false;
            }
            fclose(outfile);
        } else {
        	Warning("CopyFile failed to open output file!");
        	return false;
        }

        return true;
    } else {
    	Warning("CopyFile failed to open input file!");
        return false;
    }
}

// -----------------------------------------------------------------------------

bool MoveFile(const std::string& inpath, const std::string& outpath)
{
    //!!!
    return false;//!!!
}

// -----------------------------------------------------------------------------

void FixURLPath(std::string& path)
{
    // replace "%..." with suitable chars for a file path (eg. %20 is changed to space)
    //!!!
}

// -----------------------------------------------------------------------------

bool IsHTMLFile(const std::string& filename)
{
    size_t dotpos = filename.rfind('.');
    if (dotpos == std::string::npos) return false;

    std::string ext = filename.substr(dotpos+1);
    return ( strcasecmp(ext.c_str(),"htm") == 0 ||
             strcasecmp(ext.c_str(),"html") == 0 );
}

// -----------------------------------------------------------------------------

bool IsTextFile(const std::string& filename)
{
    if (!IsHTMLFile(filename)) {
        // if non-html file name contains "readme" then assume it's a text file
        std::string basename = filename;
        size_t lastsep = basename.rfind('/');
        if (lastsep != std::string::npos) {
            basename = basename.substr(lastsep+1);
        }
        //!!! why compile error??? (ok in Xcode)
        //!!! std::transform(basename.begin(), basename.end(), basename.begin(), std::tolower);
        if (basename.find("readme") != std::string::npos) return true;
    }
    size_t dotpos = filename.rfind('.');
    if (dotpos == std::string::npos) return false;

    std::string ext = filename.substr(dotpos+1);
    return ( strcasecmp(ext.c_str(),"txt") == 0 ||
             strcasecmp(ext.c_str(),"doc") == 0 );
}

// -----------------------------------------------------------------------------

bool IsZipFile(const std::string& filename)
{
    size_t dotpos = filename.rfind('.');
    if (dotpos == std::string::npos) return false;

    std::string ext = filename.substr(dotpos+1);
    return ( strcasecmp(ext.c_str(),"zip") == 0 ||
             strcasecmp(ext.c_str(),"gar") == 0 );
}

// -----------------------------------------------------------------------------

bool IsRuleFile(const std::string& filename)
{
    size_t dotpos = filename.rfind('.');
    if (dotpos == std::string::npos) return false;

    std::string ext = filename.substr(dotpos+1);
    return ( strcasecmp(ext.c_str(),"rule") == 0 ||
             strcasecmp(ext.c_str(),"table") == 0 ||
             strcasecmp(ext.c_str(),"tree") == 0 ||
             strcasecmp(ext.c_str(),"colors") == 0 ||
             strcasecmp(ext.c_str(),"icons") == 0 );
}

// -----------------------------------------------------------------------------

bool IsScriptFile(const std::string& filename)
{
    size_t dotpos = filename.rfind('.');
    if (dotpos == std::string::npos) return false;

    std::string ext = filename.substr(dotpos+1);
    return ( strcasecmp(ext.c_str(),"pl") == 0 ||
             strcasecmp(ext.c_str(),"py") == 0 );
}

// -----------------------------------------------------------------------------

bool EndsWith(const std::string& str, const std::string& suffix)
{
    // return true if str ends with suffix
    size_t strlen = str.length();
    size_t sufflen = suffix.length();
    return (strlen >= sufflen) && (str.rfind(suffix) == strlen - sufflen);
}

// -----------------------------------------------------------------------------

// let gollybase modules process events

class golly_poll : public lifepoll
{
public:
    virtual int checkevents();
    virtual void updatePop();
};

int golly_poll::checkevents()
{
    if (event_checker > 0) return isInterrupted();
    event_checker++;
    //!!! use Looper??? see http://developer.android.com/reference/android/os/Looper.html
    //!!! [[NSRunLoop currentRunLoop] runUntilDate:[NSDate date]];
    event_checker--;
    return isInterrupted();
}

void golly_poll::updatePop()
{
    UpdateStatus();
}

golly_poll gollypoller;    // create instance

lifepoll* Poller()
{
    return &gollypoller;
}

void PollerReset()
{
	gollypoller.resetInterrupted();
}

void PollerInterrupt()
{
	gollypoller.setInterrupted();
}

// -----------------------------------------------------------------------------

void BeginProgress(const char* title)
{
	/*!!!
    if (progresscount == 0) {
        // disable interaction with all views but don't show progress view just yet
        [globalController enableInteraction:NO];
        [globalTitle setText:[NSString stringWithCString:title encoding:NSUTF8StringEncoding]];
        cancelProgress = false;
        progstart = TimeInSeconds();
    }
    progresscount++;    // handles nested calls
    */
}

// -----------------------------------------------------------------------------

bool AbortProgress(double fraction_done, const char* message)
{
	/*!!!
    if (progresscount <= 0) Fatal("Bug detected in AbortProgress!");
    double secs = TimeInSeconds() - progstart;
    if (!globalProgress.hidden) {
        if (secs < prognext) return false;
        prognext = secs + 0.1;     // update progress bar about 10 times per sec
        if (fraction_done < 0.0) {
            // show indeterminate progress gauge???
        } else {
            [globalController updateProgressBar:fraction_done];
        }
        return cancelProgress;
    } else {
        // note that fraction_done is not always an accurate estimator for how long
        // the task will take, especially when we use nextcell for cut/copy
        if ( (secs > 1.0 && fraction_done < 0.3) || secs > 2.0 ) {
            // task is probably going to take a while so show progress view
            globalProgress.hidden = NO;
            [globalController updateProgressBar:fraction_done];
        }
        prognext = secs + 0.01;     // short delay until 1st progress update
    }
    */
    return false;
}

// -----------------------------------------------------------------------------

void EndProgress()
{
	/*!!!
    if (progresscount <= 0) Fatal("Bug detected in EndProgress!");
    progresscount--;
    if (progresscount == 0) {
        // hide the progress view and enable interaction with other views
        globalProgress.hidden = YES;
        [globalController enableInteraction:YES];
    }
    */
}
