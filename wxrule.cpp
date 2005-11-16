                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2005 Andrew Trevorrow and Tomas Rokicki.

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

// for compilers that support precompilation
#include "wx/wxprec.h"

// for all others, include the necessary headers
#ifndef WX_PRECOMP
   #include "wx/wx.h"
#endif

#include "lifealgo.h"
#include "liferules.h"     // for global_liferules

#include "wxgolly.h"       // for wxGetApp, curralgo
#include "wxutils.h"       // for Warning
#include "wxrule.h"

// -----------------------------------------------------------------------------

// eventually implement a more sophisticated dialog with a pop-up menu
// containing named rules like Conway's Life, HighLife, etc!!!

bool ChangeRule(bool hashing)
{
   wxString oldrule = wxT( curralgo->getrule() );
   wxString newrule = wxGetTextFromUser( _T("Enter rule using B0..8/S0..8 notation:"),
                                         _T("Change rule"),
                                         oldrule,
                                         wxGetApp().GetTopWindow() );
   if (newrule.empty()) {
      // user hit Cancel
   } else {
      const char *err = curralgo->setrule( (char *)newrule.c_str() );
      // restore old rule if an error occurred
      if (err) {
         Warning(err);
         curralgo->setrule( (char *)oldrule.c_str() );
      } else if ( global_liferules.hasB0notS8 && hashing ) {
         Warning("B0-not-S8 rules are not allowed when hashing.");
         curralgo->setrule( (char *)oldrule.c_str() );
      } else {
         // new rule is ok
         return true;
      }
   }
   return false;
}
