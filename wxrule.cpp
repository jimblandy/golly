                        /*** /

This file is part of Golly, a Game of Life Simulator.
Copyright (C) 2008 Andrew Trevorrow and Tomas Rokicki.

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

#include "wxgolly.h"       // for wxGetApp
#include "wxmain.h"        // for mainptr->
#include "wxprefs.h"       // for namedrules, allowundo
#include "wxutils.h"       // for Warning
#include "wxlayer.h"       // for currlayer
#include "wxalgos.h"       // for NumAlgos, CreateNewUniverse
#include "wxrule.h"

// -----------------------------------------------------------------------------

bool ValidRule(wxString& rule)
{
   // return true if given rule is valid in at least one algorithm
   // and convert rule to canonical form
   
   // qlife and hlife share global_liferules, so we need to save
   // and restore the current rule -- yuk
   wxString oldrule = wxString(currlayer->algo->getrule(), wxConvLocal);
   
   for (int i = 0; i < NumAlgos(); i++) {
      lifealgo* tempalgo = CreateNewUniverse(i);
      const char* err = tempalgo->setrule( rule.mb_str(wxConvLocal) );
      if (!err) {
         // convert rule to canonical form
         rule = wxString(tempalgo->getrule(), wxConvLocal);
         delete tempalgo;
         currlayer->algo->setrule( oldrule.mb_str(wxConvLocal) );
         return true;
      }
      delete tempalgo;
   }
   currlayer->algo->setrule( oldrule.mb_str(wxConvLocal) );
   return false;
}

// -----------------------------------------------------------------------------

bool MatchingRules(const wxString& rule1, const wxString& rule2)
{
   // return true if given strings are equivalent rules
   if (rule1 == rule2) {
      return true;
   } else {
      // we want "s23b3" or "23/3" to match "B3/S23" so convert given rules
      // to canonical form (if valid) and then compare
      wxString canon1 = rule1;
      wxString canon2 = rule2;
      return ValidRule(canon1) && ValidRule(canon2) && canon1 == canon2;
   }
}

// -----------------------------------------------------------------------------

wxString GetRuleName(const wxString& rulestring)
{
   // search namedrules array for matching rule
   wxString rulename;
   size_t i;
   for (i = 0; i < namedrules.GetCount(); i++) {
      // extract rule after '|'
      wxString thisrule = namedrules[i].AfterFirst('|');
      if ( MatchingRules(rulestring, thisrule) ) {
         // extract name before '|'
         rulename = namedrules[i].BeforeFirst('|');
         return rulename;
      }
   }
   // given rulestring has not been named
   rulename = rulestring;
   return rulename;
}

// -----------------------------------------------------------------------------

// define a modal dialog for changing the current rule

class RuleDialog : public wxDialog
{
public:
   RuleDialog(wxWindow* parent);
   virtual bool TransferDataFromWindow();    // called when user hits OK

private:
   // control ids
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
   Create(parent, wxID_ANY, _("Set Rule"), wxDefaultPosition, wxDefaultSize);
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

const wxString UNNAMED = _("UNNAMED");

const int HGAP = 12;
const int BIGVGAP = 12;

// following ensures OK/Cancel buttons are better aligned
#ifdef __WXMAC__
   const int STDHGAP = 0;
#elif defined(__WXMSW__)
   const int STDHGAP = 9;
#else
   const int STDHGAP = 10;
#endif

void RuleDialog::CreateControls()
{
   wxBoxSizer* topSizer = new wxBoxSizer( wxVERTICAL );
   SetSizer(topSizer);
   
   ruletext = new wxTextCtrl(this, RULE_TEXT,
                             wxString(currlayer->algo->getrule(), wxConvLocal));

   wxString title = _("Enter a new rule:");
   wxStaticText* textlabel = new wxStaticText(this, wxID_STATIC, title);
   
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
                                 _("Or select a named rule:"));

   wxButton* delbutt = new wxButton(this, RULE_DEL_BUTT, _("Delete"),
                                    wxDefaultPosition, wxDefaultSize, 0);

   wxButton* addbutt = new wxButton(this, RULE_ADD_BUTT, _("Add"),
                                    wxDefaultPosition, wxDefaultSize, 0);

   addtext = new wxTextCtrl(this, RULE_ADD_TEXT, wxEmptyString,
                            wxDefaultPosition, wxSize(160,wxDefaultCoord));

   wxSizer* stdbutts = CreateButtonSizer(wxOK | wxCANCEL);

   wxBoxSizer* hbox1 = new wxBoxSizer( wxHORIZONTAL );
   hbox1->Add(namechoice, 0, wxALIGN_CENTER_VERTICAL, 0);
   hbox1->AddSpacer(HGAP);
   hbox1->Add(delbutt, 0, wxALIGN_CENTER_VERTICAL, 0);

   wxBoxSizer* hbox2 = new wxBoxSizer( wxHORIZONTAL );
   hbox2->Add(addtext, 0, wxALIGN_CENTER_VERTICAL, 0);
   hbox2->AddSpacer(HGAP);
   hbox2->Add(addbutt, 0, wxALIGN_CENTER_VERTICAL, 0);

   wxBoxSizer* stdhbox = new wxBoxSizer( wxHORIZONTAL );
   stdhbox->Add(stdbutts, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxRIGHT, STDHGAP);

   topSizer->AddSpacer(BIGVGAP);
   topSizer->Add(textlabel, 0, wxLEFT | wxRIGHT, HGAP);
   topSizer->AddSpacer(10);
   topSizer->Add(ruletext, 0, wxGROW | wxLEFT | wxRIGHT, HGAP);
   topSizer->AddSpacer(BIGVGAP);
   topSizer->Add(namelabel, 0, wxLEFT | wxRIGHT, HGAP);
   topSizer->AddSpacer(6);
   topSizer->Add(hbox1, 0, wxLEFT | wxRIGHT, HGAP);
   topSizer->AddSpacer(BIGVGAP);
   topSizer->Add(hbox2, 0, wxLEFT | wxRIGHT, HGAP);
   topSizer->AddSpacer(BIGVGAP);
   topSizer->Add(stdhbox, 1, wxGROW | wxTOP | wxBOTTOM, 10);
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
      // matching rule found so remove UNNAMED item if it exists
      // use (int) twice to avoid warnings on wx 2.6.x/2.7
      if ( (int) namechoice->GetCount() > (int) namedrules.GetCount() ) {
         namechoice->Delete( namechoice->GetCount() - 1 );
      }
   } else {
      // no match found so use index of UNNAMED item,
      // appending it if it doesn't exist;
      // use (int) twice to avoid warnings on wx 2.6.x/2.7
      if ( (int) namechoice->GetCount() == (int) namedrules.GetCount() ) {
         namechoice->Append(UNNAMED);
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
      // do nothing if UNNAMED item was chosen
      return;
   } else {
      // remove UNNAMED item if it exists
      // use (int) twice to avoid warnings on wx 2.6.x/2.7
      if ( (int) namechoice->GetCount() > (int) namedrules.GetCount() ) {
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
   
   // validate new rule and convert to canonical form
   wxString newrule = ruletext->GetValue();
   if (!ValidRule(newrule)) {
      Warning(_("The new rule is not valid in any algorithm."));
      ruletext->SetFocus();
      ruletext->SetSelection(-1,-1);
      return;
   }
      
   // validate new name
   wxString newname = addtext->GetValue();
   if ( newname.IsEmpty() ) {
      Warning(_("Type in a name for the new rule."));
      addtext->SetFocus();
      return;
   } else if ( newname.Find('|') >= 0 ) {
      Warning(_("Sorry, but rule names must not contain \"|\"."));
      addtext->SetFocus();
      addtext->SetSelection(-1,-1);
      return;
   } else if ( newname == UNNAMED ) {
      Warning(_("You can't use that name smarty pants."));
      addtext->SetFocus();
      addtext->SetSelection(-1,-1);
      return;
   } else if ( namechoice->FindString(newname) != wxNOT_FOUND ) {
      Warning(_("That name is already used for another rule."));
      addtext->SetFocus();
      addtext->SetSelection(-1,-1);
      return;
   }
   
   // replace UNNAMED with new name
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
   
   // force a change to UNNAMED item
   nameindex = -1;
   UpdateName();
}

// -----------------------------------------------------------------------------

void RuleDialog::OnUpdateAdd(wxUpdateUIEvent& event)
{
   // Add button is only enabled if UNNAMED item is selected
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
   if (newrule.IsEmpty()) newrule = wxT("B3/S23");
   
   // first try new rule in current algorithm
   const char* err = currlayer->algo->setrule( newrule.mb_str(wxConvLocal) );
   if (!err) return true;
   
   // try to find another algorithm that supports the new rule
   for (int i = 0; i < NumAlgos(); i++) {
      if (i != currlayer->algtype) {
         lifealgo* tempalgo = CreateNewUniverse(i);
         err = tempalgo->setrule( newrule.mb_str(wxConvLocal) );
         delete tempalgo;
         if (!err) {
            // change the current algorithm and switch to the new rule
            mainptr->ChangeAlgorithm(i, newrule);
            return true;
         }
      }
   }
   
   Warning(_("The new rule is not valid in any algorithm."));
   ruletext->SetFocus();
   ruletext->SetSelection(-1,-1);
   return false;
}

// -----------------------------------------------------------------------------

bool ChangeRule()
{
   wxArrayString oldnames = namedrules;
   wxString oldrule = wxString(currlayer->algo->getrule(), wxConvLocal);

   RuleDialog dialog( wxGetApp().GetTopWindow() );
   if ( dialog.ShowModal() == wxID_OK ) {
      // TransferDataFromWindow has changed the current rule,
      // and possibly the current algorithm as well
      return true;
   } else {
      // user hit Cancel so restore rule and name array
      currlayer->algo->setrule( oldrule.mb_str(wxConvLocal) );
      namedrules.Clear();
      namedrules = oldnames;
      return false;
   }
}
