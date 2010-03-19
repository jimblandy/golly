                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2010 Andrew Trevorrow and Tomas Rokicki.

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
#ifndef _WXTIMELINE_H_
#define _WXTIMELINE_H_

// Timeline support:

void CreateTimelineBar(wxWindow* parent);
// Create timeline bar window at bottom of given parent window.

int TimelineBarHeight();
// Return height of timeline bar.

void ResizeTimelineBar(int y, int wd);
// Move and/or resize timeline bar.

void UpdateTimelineBar(bool active);
// Update state of buttons in timeline bar.

void ToggleTimelineBar();
// Show/hide timeline bar.

void StartStopRecording();
// If recording a timeline then stop, otherwise start a new recording
// (if no timeline exists) or extend the existing timeline.

void DeleteTimeline();
// Delete the existing timeline.

void InitTimelineFrame();
// Go to the first frame in the recently loaded timeline.

bool TimelineExists();
// Does a timeline exist in the current algorithm?

bool AutoPlay();
// Called in MainFrame::OnIdle so we can check if the next
// timeline frame needs to be displayed.  Returns true if we are
// in autoplay mode and so another idle event needs to be sent.

void PlayTimeline(int direction);
// Play timeline forwards if given direction is +ve, or backwards
// if direction is -ve, or stop if direction is 0.

void PlayTimelineFaster();
// Increase the rate at which timeline frames are displayed.

void PlayTimelineSlower();
// Decrease the rate at which timeline frames are displayed.

void ResetTimelineSpeed();
// Reset autoplay speed to 0 (no delay, no frame skipping).

bool TimelineIsPlaying();
// Return true if timeline is in autoplay mode.

#endif
