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

#import <AudioToolbox/AudioToolbox.h>

#include <sys/time.h>   // for gettimeofday

#include "lifepoll.h"   // for lifepoll

#include "prefs.h"      // for allowbeep, tempdir
#include "utils.h"

#import "PatternViewController.h"   // for UpdateStatus

// -----------------------------------------------------------------------------

int event_checker = 0;      // if > 0 then we're checking for events in currentRunLoop

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

// need the following to make YesNo/Warning/Fatal dialogs modal:

@interface ModalAlertDelegate : NSObject <UIAlertViewDelegate>
{
    NSInteger returnButt;
}

@property () NSInteger returnButt;

- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex;

@end

@implementation ModalAlertDelegate

@synthesize returnButt;

- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
    returnButt = buttonIndex;
}

@end

// -----------------------------------------------------------------------------

bool YesNo(const char* msg)
{
    Beep();
    
    ModalAlertDelegate *md = [[ModalAlertDelegate alloc] init];
    md.returnButt = -1;
    
    UIAlertView *a = [[UIAlertView alloc] initWithTitle:@"Warning"
                                                message:[NSString stringWithCString:msg encoding:NSUTF8StringEncoding]
                                               delegate:md
                                      cancelButtonTitle:@"No"
                                      otherButtonTitles:@"Yes", nil];
    [a show];
    
    // wait for user to hit button
    while (md.returnButt == -1) {
        event_checker++;
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
        event_checker--;
    }
    
    return md.returnButt != 0;
}

// -----------------------------------------------------------------------------

void Warning(const char* msg)
{
    Beep();
    
    ModalAlertDelegate *md = [[ModalAlertDelegate alloc] init];
    md.returnButt = -1;
    
    UIAlertView *a = [[UIAlertView alloc] initWithTitle:@"Warning"
                                                message:[NSString stringWithCString:msg encoding:NSUTF8StringEncoding]
                                               delegate:md
                                      cancelButtonTitle:@"OK"
                                      otherButtonTitles:nil];
    [a show];
    
    // wait for user to hit button
    while (md.returnButt == -1) {
        event_checker++;
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
        event_checker--;
    }
}

// -----------------------------------------------------------------------------

void Fatal(const char* msg)
{
    Beep();

    ModalAlertDelegate *md = [[ModalAlertDelegate alloc] init];
    md.returnButt = -1;
    
    UIAlertView *a = [[UIAlertView alloc] initWithTitle:@"Fatal Error"
                                                message:[NSString stringWithCString:msg encoding:NSUTF8StringEncoding]
                                               delegate:md
                                      cancelButtonTitle:@"Quit"
                                      otherButtonTitles:nil];
    [a show];
    
    // wait for user to hit button
    while (md.returnButt == -1) {
        event_checker++;
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
        event_checker--;
    }
    
    exit(1);
}

// -----------------------------------------------------------------------------

static SystemSoundID beepID = 0;

void Beep()
{
    if (allowbeep) {
        if (beepID == 0) {
            // get the path to the sound file
            NSString* path = [[NSBundle mainBundle] pathForResource:@"beep" ofType:@"aiff"];
            if (path) {
                NSURL* url = [NSURL fileURLWithPath:path];
                OSStatus err = AudioServicesCreateSystemSoundID((__bridge CFURLRef)url, &beepID);
                if (err == kAudioServicesNoError && beepID > 0) {
                    // play the sound
                    AudioServicesPlaySystemSound(beepID);
                }
            }
        } else {
            // assume we got the sound
            AudioServicesPlaySystemSound(beepID);
        }
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
    if ([[NSFileManager defaultManager] removeItemAtPath:[NSString stringWithCString:filepath.c_str() encoding:NSUTF8StringEncoding]
                                                   error:NULL] == NO) {
        // should never happen
        Warning("RemoveFile failed!");
    };
}

// -----------------------------------------------------------------------------

bool CopyFile(const std::string& inpath, const std::string& outpath)
{
    if (FileExists(outpath)) {
        RemoveFile(outpath);
    }
    return [[NSFileManager defaultManager] copyItemAtPath:[NSString stringWithCString:inpath.c_str() encoding:NSUTF8StringEncoding]
                                                   toPath:[NSString stringWithCString:outpath.c_str() encoding:NSUTF8StringEncoding]
                                                    error:NULL];
}

// -----------------------------------------------------------------------------

void FixURLPath(std::string& path)
{
    // replace any "%20" with " "
    size_t pos = path.find("%20");
    while (pos != std::string::npos) {
        path.replace(pos, 3, " ");
        pos = path.find("%20");
    }
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
        std::transform(basename.begin(), basename.end(), basename.begin(), std::tolower);
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

// let gollybase modules process events

class ios_poll : public lifepoll
{
public:
    virtual int checkevents();
    virtual void updatePop();
};

int ios_poll::checkevents()
{
    if (event_checker > 0) return isInterrupted();
    event_checker++;
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate date]];
    event_checker--;
    return isInterrupted();
}

void ios_poll::updatePop()
{
    UpdateStatus();
}

ios_poll iospoller;    // create instance

lifepoll* Poller()
{
    return &iospoller;
}

void PollerReset()
{
    iospoller.resetInterrupted();
}

void PollerInterrupt()
{
    iospoller.setInterrupted();
}
