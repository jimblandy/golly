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

#include "wx/wxprec.h"     // for compilers that support precompilation
#ifndef WX_PRECOMP
   #include "wx/wx.h"      // for all others include the necessary headers
#endif

#include "lifealgo.h"
#include "liferules.h"     // for global_liferules

#include "wxgolly.h"       // for wxGetApp, curralgo
#include "wxprefs.h"       // for namedrules, hashing
#include "wxutils.h"       // for Warning
#include "wxrule.h"

// -----------------------------------------------------------------------------

// define a modal dialog for changing the current rule

class RuleDialog : public wxDialog
{
public:
   RuleDialog(wxWindow* parent);
   virtual bool TransferDataFromWindow();    // called when user hits OK

private:
   // control IDs
   enum {
      RULE_TEXT,
      RULE_NAME,
      RULE_ADD_BUTT,
      RULE_ADD_TEXT,
      RULE_DEL_BUTT
   };

   void CreateControls();     // init ruletext, addtext, namechoice
   void UpdateName();         // update namechoice depending on ruletext
   
   wxTextCtrl* ruletext;      // text box for user to type in rule
   wxTextCtrl* addtext;       // text box for user to type in name of rule
   wxChoice* namechoice;      // kept in sync with namedrules but can have one
                              // more item appended ("UNNAMED")
   
   int nameindex;             // current namechoice selection
   bool ignore_text_change;   // prevent OnRuleTextChanged doing anything?
   
   // event handlers
   void OnRuleTextChanged(wxCommandEvent& event);
   void OnChooseName(wxCommandEvent& event);
   void OnAddName(wxCommandEvent& event);
   void OnDeleteName(wxCommandEvent& event);
   void OnUpdateAdd(wxUpdateUIEvent& event);
   void OnUpdateDelete(wxUpdateUIEvent& event);

   DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(RuleDialog, wxDialog)
   EVT_TEXT       (RULE_TEXT,       RuleDialog::OnRuleTextChanged)
   EVT_CHOICE     (RULE_NAME,       RuleDialog::OnChooseName)
   EVT_BUTTON     (RULE_ADD_BUTT,   RuleDialog::OnAddName)
   EVT_BUTTON     (RULE_DEL_BUTT,   RuleDialog::OnDeleteName)
   EVT_UPDATE_UI  (RULE_ADD_BUTT,   RuleDialog::OnUpdateAdd)
   EVT_UPDATE_UI  (RULE_DEL_BUTT,   RuleDialog::OnUpdateDelete)
END_EVENT_TABLE()

// -----------------------------------------------------------------------------

RuleDialog::RuleDialog(wxWindow* parent)
{
   Create(parent, wxID_ANY, _("Rule"), wxDefaultPosition, wxDefaultSize);
   ignore_text_change = true;
   CreateControls();
   GetSizer()->Fit(this);
   GetSizer()->SetSizeHints(this);
   Centre();
   ignore_text_change = false;

   // select all of rule text
   ruletext->SetFocus();
   ruletext->SetSelection(0,999);   // wxMac bug: -1,-1 doesn't work here
}

// -----------------------------------------------------------------------------

const int HGAP = 12;
const int BIGVGAP = 12;
const int SMALLVGAP = 5;

// following ensures OK/Cancel buttons are aligned with Add and Delete buttons
#ifdef __WXMAC__
   const int STDHGAP = 0;
#elif defined(__WXMSW__)
   const int STDHGAP = 9;
#else
   const int STDHGAP = 6;
#endif

void RuleDialog::CreateControls()
{
   RuleDialog* dlg = this;
   wxBoxSizer *topSizer = new wxBoxSizer( wxVERTICAL );
   dlg->SetSizer(topSizer);
   
   // create the controls:
   
   ruletext = new wxTextCtrl(this, RULE_TEXT, wxT(curralgo->getrule()));
   wxStaticText* textlabel = new wxStaticText(this, wxID_STATIC,
                                 _("Enter rule using B0..8/S0..8 notation:"));
   
   wxArrayString namearray;
   size_t i;
   for (i=0; i<namedrules.GetCount(); i++) {
      // remove "|..." part
      wxString name = namedrules[i].BeforeFirst('|');
      namearray.Add(name);
   }
   namechoice = new wxChoice(this, RULE_NAME,
                             wxDefaultPosition, wxSize(160,wxDefaultCoord),
                             namearray);

   // choose appropriate name
   nameindex = -1;
   UpdateName();

   wxStaticText* namelabel = new wxStaticText(this, wxID_STATIC,
                                 _("Or select named rule:"));

   wxButton* delbutt = new wxButton(this, RULE_DEL_BUTT, _("Delete"),
                                    wxDefaultPosition, wxDefaultSize, 0);

   wxButton* addbutt = new wxButton(this, RULE_ADD_BUTT, _("Add"),
                                    wxDefaultPosition, wxDefaultSize, 0);

   addtext = new wxTextCtrl(this, RULE_ADD_TEXT, wxEmptyString,
                            wxDefaultPosition, wxSize(160,wxDefaultCoord));

   wxSizer* stdbutts = CreateButtonSizer(wxOK | wxCANCEL);
   
   // now position the controls:

   wxBoxSizer *hbox1 = new wxBoxSizer( wxHORIZONTAL );
   hbox1->Add(namechoice, 0, wxALIGN_CENTER_VERTICAL, 0);
   hbox1->AddSpacer(HGAP);
   hbox1->Add(delbutt, 0, wxALIGN_CENTER_VERTICAL, 0);

   wxBoxSizer *hbox2 = new wxBoxSizer( wxHORIZONTAL );
   hbox2->Add(addtext, 0, wxALIGN_CENTER_VERTICAL, 0);
   hbox2->AddSpacer(HGAP);
   hbox2->Add(addbutt, 0, wxALIGN_CENTER_VERTICAL, 0);

   wxBoxSizer *stdhbox = new wxBoxSizer( wxHORIZONTAL );
   stdhbox->Add(stdbutts, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxRIGHT, STDHGAP);

   topSizer->AddSpacer(BIGVGAP);
   topSizer->Add(textlabel, 0, wxLEFT | wxRIGHT, HGAP);
   topSizer->AddSpacer(SMALLVGAP);
   topSizer->Add(ruletext, 0, wxGROW | wxLEFT | wxRIGHT, HGAP);
   topSizer->AddSpacer(BIGVGAP);
   topSizer->Add(namelabel, 0, wxLEFT | wxRIGHT, HGAP);
   topSizer->AddSpacer(SMALLVGAP);
   topSizer->Add(hbox1, 0, wxLEFT | wxRIGHT, HGAP);
   topSizer->AddSpacer(BIGVGAP);
   topSizer->Add(hbox2, 0, wxLEFT | wxRIGHT, HGAP);
   topSizer->AddSpacer(BIGVGAP);
   topSizer->Add(stdhbox, 1, wxGROW | wxTOP | wxBOTTOM, 10);
}

// -----------------------------------------------------------------------------

// convert given rule string to a canonical bit pattern
int CanonicalRule(const char *rulestring)
{
   const int survbit = 9;              // bits 0..8 for birth, 9..17 for survival
   const int wolfbit = survbit * 2;
   const int hexbit = wolfbit + 1;

   // WARNING: this code is a simplified version of liferules::setrule()
   int rulebits = 0;
   int addend = survbit;
   int i;
   
   for (i=0; rulestring[i]; i++) {
      if (rulestring[i] == 'h' || rulestring[i] == 'H') {
         rulebits |= 1 << hexbit;
      } else if (rulestring[i] == 'b' || rulestring[i] == 'B' || rulestring[i] == '/') {
         addend = 0;
      } else if (rulestring[i] == 's' || rulestring[i] == 'S') {
         addend = survbit;
      } else if (rulestring[i] >= '0' && rulestring[i] <= '8') {
         rulebits |= 1 << (addend + rulestring[i] - '0');
      } else if (rulestring[i] == 'w' || rulestring[i] == 'W') {
         int wolfram = atol(rulestring+i+1);
         if ( wolfram < 0 || wolfram > 254 || wolfram & 1 ) {
            // when we support toroidal universe we can allow all numbers from 0..255!!!
            return -1;
         }
         return wolfram | (1 << wolfbit);
      } else {
         // unexpected character so not a valid rule
         return -1;
      }
   }
   
   return rulebits;
}

// -----------------------------------------------------------------------------

// return true if given strings are equivalent rules
bool MatchingRules(wxString &rule1, wxString &rule2)
{
   if (rule1 == rule2) {
      return true;
   } else {
      // we want "s23b3" or "23/3" to match "B3/S23" so convert given rules to
      // a canonical bit pattern and compare
      int canon1 = CanonicalRule(rule1);
      int canon2 = CanonicalRule(rule2);
      // both rules must also be valid (non-negative) for a match
      return canon1 >= 0 && canon2 >= 0 && canon1 == canon2;
   }
}

// -----------------------------------------------------------------------------

void RuleDialog::UpdateName()
{
   // may need to change named rule depending on current rule text
   int newindex;
   wxString newrule = ruletext->GetValue();
   if ( newrule.IsEmpty() ) {
      // empty string is a quick way to restore normal Life
      newindex = 0;
   } else {
      // search namedrules array for matching rule
      size_t i;
      newindex = -1;
      for (i=0; i<namedrules.GetCount(); i++) {
         // extract rule after '|'
         wxString thisrule = namedrules[i].AfterFirst('|');
         if ( MatchingRules(newrule, thisrule) ) {
            newindex = i;
            break;
         }
      }
   }
   if (newindex >= 0) {
      // matching rule found so remove "UNNAMED" item if it exists
      if ( namechoice->GetCount() > (int) namedrules.GetCount() ) {
         namechoice->Delete( namechoice->GetCount() - 1 );
      }
   } else {
      // no match found so use index of "UNNAMED" item,
      // appending it if it doesn't exist
      if ( namechoice->GetCount() == (int) namedrules.GetCount() ) {
         namechoice->Append("UNNAMED");
      }
      newindex = namechoice->GetCount() - 1;
   }
   if (nameindex != newindex) {
      nameindex = newindex;
      namechoice->SetSelection(nameindex);
   }
}

// -----------------------------------------------------------------------------

void RuleDialog::OnRuleTextChanged(wxCommandEvent& WXUNUSED(event))
{
   if (ignore_text_change) return;
   UpdateName();
}

// -----------------------------------------------------------------------------

void RuleDialog::OnChooseName(wxCommandEvent& WXUNUSED(event))
{
   // update rule text based on chosen name
   nameindex = namechoice->GetSelection();
   if ( nameindex == (int) namedrules.GetCount() ) {
      // do nothing if "UNNAMED" item was chosen
      return;
   } else {
      // remove "UNNAMED" item if it exists
      if ( namechoice->GetCount() > (int) namedrules.GetCount() ) {
         namechoice->Delete( namechoice->GetCount() - 1 );
      }
   }
   wxString rule = namedrules[nameindex].AfterFirst('|');
   ignore_text_change = true;
   ruletext->SetValue(rule);
   ruletext->SetFocus();
   ruletext->SetSelection(-1,-1);
   ignore_text_change = false;
}

// -----------------------------------------------------------------------------

void RuleDialog::OnAddName(wxCommandEvent& WXUNUSED(event))
{
   if ( nameindex < (int) namedrules.GetCount() ) {
      // OnUpdateAdd should prevent this but play safe
      wxBell();
      return;
   }
   
   // validate new rule
   wxString newrule = ruletext->GetValue();
   const char *err = curralgo->setrule( (char*) newrule.c_str() );
   if (err) {
      Warning("Rule is not valid.");
      ruletext->SetFocus();
      ruletext->SetSelection(-1,-1);
      return;
   }
      
   // validate new name
   wxString newname = addtext->GetValue();
   if ( newname.IsEmpty() ) {
      Warning("Type in a name for the new rule.");
      addtext->SetFocus();
      return;
   } else if ( newname.Find('|') >= 0 ) {
      Warning("Sorry, but rule names must not contain \"|\".");
      addtext->SetFocus();
      addtext->SetSelection(-1,-1);
      return;
   } else if ( newname == "UNNAMED" ) {
      Warning("You can't use that name smarty pants.");
      addtext->SetFocus();
      addtext->SetSelection(-1,-1);
      return;
   } else if ( namechoice->FindString(newname) != wxNOT_FOUND ) {
      Warning("That name is already used for another rule.");
      addtext->SetFocus();
      addtext->SetSelection(-1,-1);
      return;
   }
   
   // replace "UNNAMED" with new name
   namechoice->Delete( namechoice->GetCount() - 1 );
   namechoice->Append( newname );
   
   // append new name and rule to namedrules
   newname += '|';
   newname += newrule;
   namedrules.Add(newname);
   
   // force a change to newly appended item
   nameindex = -1;
   UpdateName();
}

// -----------------------------------------------------------------------------

void RuleDialog::OnDeleteName(wxCommandEvent& WXUNUSED(event))
{
   if ( nameindex <= 0 || nameindex >= (int) namedrules.GetCount() ) {
      // OnUpdateDelete should prevent this but play safe
      wxBell();
      return;
   }
   
   // remove current name
   namechoice->Delete(nameindex);
   namedrules.RemoveAt((size_t) nameindex);
   
   // force a change to "UNNAMED" item
   nameindex = -1;
   UpdateName();
}

// -----------------------------------------------------------------------------

void RuleDialog::OnUpdateAdd(wxUpdateUIEvent& event)
{
   // Add button is only enabled if "UNNAMED" item is selected
   event.Enable( nameindex == (int) namedrules.GetCount() );
}

// -----------------------------------------------------------------------------

void RuleDialog::OnUpdateDelete(wxUpdateUIEvent& event)
{
   // Delete button is only enabled if a non-Life named rule is selected
   event.Enable( nameindex > 0 && nameindex < (int) namedrules.GetCount() );
}

// -----------------------------------------------------------------------------

bool RuleDialog::TransferDataFromWindow()
{
   // get and validate new rule
   wxString newrule = ruletext->GetValue();
   if ( newrule.IsEmpty() ) {
      curralgo->setrule("B3/S23");
      return true;
   }
   const char *err = curralgo->setrule( (char*) newrule.c_str() );
   if (err) {
      Warning(err);
      return false;
   } else if ( global_liferules.hasB0notS8 && hashing ) {
      Warning("B0-not-S8 rules are not allowed when hashing.");
      return false;
   }
   return true;
}

// -----------------------------------------------------------------------------

bool ChangeRule()
{
   wxArrayString oldnames = namedrules;
   wxString oldrule = wxT( curralgo->getrule() );

   RuleDialog dialog( wxGetApp().GetTopWindow() );
   if ( dialog.ShowModal() == wxID_OK ) {
      // TransferDataFromWindow has validated and changed rule
      return true;
   } else {
      // user hit Cancel so restore rule and name array
      curralgo->setrule( (char*) oldrule.c_str() );
      namedrules.Clear();       // safer to free memory???
      namedrules = oldnames;
      return false;
   }
}

// -----------------------------------------------------------------------------

wxString rulename;

const char* GetRuleName(const char* rulestring)
{
   // search namedrules array for matching rule
   wxString givenrule = rulestring;
   size_t i;
   for (i=0; i<namedrules.GetCount(); i++) {
      // extract rule after '|'
      wxString thisrule = namedrules[i].AfterFirst('|');
      if ( MatchingRules(givenrule, thisrule) ) {
         // extract name before '|'
         rulename = namedrules[i].BeforeFirst('|');
         return rulename.c_str();
      }
   }
   return rulestring;      // not named
}