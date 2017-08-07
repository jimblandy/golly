// This file is part of Golly.
// See docs/License.html for the copyright notice.

#ifndef _WXTIMELINE_H_
#define _WXTIMELINE_H_

// Timeline support:

void CreateTimelineBar(wxWindow* parent);
// Create timeline bar window at bottom of given parent window.

int TimelineBarHeight();
// Return height of timeline bar.

void ResizeTimelineBar(int y, int wd);
// Move and/or resize timeline bar.

void UpdateTimelineBar();
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
