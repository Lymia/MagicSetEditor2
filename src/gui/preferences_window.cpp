//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/preferences_window.hpp>
#include <gui/update_checker.hpp>
#include <data/settings.hpp>
#include <util/window_id.hpp>
#include <util/io/package_manager.hpp>
#include <wx/spinctrl.h>
#include <wx/filename.h>
#include <wx/notebook.h>

DECLARE_TYPEOF_COLLECTION(PackagedP);

// use a combo box for the zoom choices instead of a spin control
#define USE_ZOOM_COMBOBOX 1

// ----------------------------------------------------------------------------- : Preferences pages

// A page from the preferences dialog
class PreferencesPage : public wxPanel {
  public:
	PreferencesPage(Window* parent)
		: wxPanel(parent, wxID_ANY)
	{}
	
	/// Stores the settings from the panel in the global settings object
	virtual void store() = 0;
};

// Preferences page for global MSE settings
class GlobalPreferencesPage : public PreferencesPage {
  public:
	GlobalPreferencesPage(Window* parent);
	void store();	
	
  private:
	wxComboBox* language;
	wxCheckBox* open_sets_in_new_window;
};

// Preferences page for card viewing related settings
class DisplayPreferencesPage : public PreferencesPage {
  public:
	DisplayPreferencesPage(Window* parent);
	void store();	
	
  private:
	DECLARE_EVENT_TABLE();
	
	wxCheckBox* high_quality, *borders, *draw_editing, *spellcheck_enabled;
	#if USE_ZOOM_COMBOBOX
		wxComboBox* zoom;
		int zoom_int;
	#else
		wxSpinCtrl* zoom;
	#endif
	wxCheckBox* non_normal_export;
	
	void onSelectColumns(wxCommandEvent&);
	#if USE_ZOOM_COMBOBOX
		void onZoomChange(wxCommandEvent&);
		void updateZoom();
	#endif
};

// Preferences page for directories of programs
// i.e. Apprentice, Magic Workstation
// perhaps in the future also directories for packages?
class DirsPreferencesPage : public PreferencesPage {
  public:
	DirsPreferencesPage(Window* parent);
	void store();
	
  private:
	DECLARE_EVENT_TABLE();
	
	wxTextCtrl* apprentice;
	
	void onApprenticeBrowse(wxCommandEvent&);
};

// Preferences page for automatic updates
class UpdatePreferencesPage : public PreferencesPage {
  public:
	UpdatePreferencesPage(Window* parent);
	void store();
	
  private:
	DECLARE_EVENT_TABLE();
	
	wxChoice* check_at_startup;
	
	// check for updates
	void onCheckUpdatesNow(wxCommandEvent&);
};


// ----------------------------------------------------------------------------- : PreferencesWindow

PreferencesWindow::PreferencesWindow(Window* parent)
	: wxDialog(parent, wxID_ANY, _TITLE_("preferences"), wxDefaultPosition)
{
	// init notebook
	wxNotebook* nb = new wxNotebook(this, ID_NOTEBOOK);
	nb->AddPage(new GlobalPreferencesPage (nb), _TITLE_("global"));
	nb->AddPage(new DisplayPreferencesPage(nb), _TITLE_("display"));
	nb->AddPage(new DirsPreferencesPage   (nb), _TITLE_("directories"));
	nb->AddPage(new UpdatePreferencesPage (nb), _TITLE_("updates"));
	
	// init sizer
	wxSizer* s = new wxBoxSizer(wxVERTICAL);
	s->Add(nb,                                 1, wxEXPAND | (wxALL & ~wxBOTTOM), 8);
	s->AddSpacer(4);
	s->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | (wxALL & ~wxTOP),    8);
	s->SetSizeHints(this);
	SetSizer(s);
}

void PreferencesWindow::onOk(wxCommandEvent&) {
	// store each page
	wxNotebook* nb = static_cast<wxNotebook*>(FindWindow(ID_NOTEBOOK));
	size_t count = nb->GetPageCount();
	for (size_t i = 0 ; i < count ; ++i) {
		static_cast<PreferencesPage*>(nb->GetPage(i))->store();
	}
	// close
	EndModal(wxID_OK);
}

BEGIN_EVENT_TABLE(PreferencesWindow, wxDialog)
	EVT_BUTTON       (wxID_OK, PreferencesWindow::onOk)
END_EVENT_TABLE  ()


// ----------------------------------------------------------------------------- : Preferences page : global

bool compare_package_name(const PackagedP& a, const PackagedP& b) {
	return a->name() < b->name();
}

GlobalPreferencesPage::GlobalPreferencesPage(Window* parent)
	: PreferencesPage(parent)
{
	// init controls
	language = new wxComboBox(this, wxID_ANY, _(""), wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
	open_sets_in_new_window = new wxCheckBox(this, wxID_ANY, _BUTTON_("open sets in new window"));
	// set values
	vector<PackagedP> locales;
	package_manager.findMatching(_("*.mse-locale"), locales);
	sort(locales.begin(), locales.end(), compare_package_name);
	int n = 0;
	FOR_EACH(package, locales) {
		language->Append(package->name() + _(": ") + package->full_name, package.get());
		if (settings.locale == package->name()) {
			language->SetSelection(n);
		}
		n++;
	}
	open_sets_in_new_window->SetValue(settings.open_sets_in_new_window);
	// init sizer
	wxSizer* s = new wxBoxSizer(wxVERTICAL);
	s->SetSizeHints(this);
		wxSizer* s2 = new wxStaticBoxSizer(wxVERTICAL, this, _LABEL_("language"));
			s2->Add(new wxStaticText(this, wxID_ANY, _LABEL_("app language")), 0,             wxALL,          4);
			s2->Add(language,                                                  0, wxEXPAND | (wxALL & ~wxTOP), 4);
			s2->Add(new wxStaticText(this, wxID_ANY, _HELP_( "app language")), 0,             wxALL,          4);
		s->Add(s2, 0, wxEXPAND | wxALL, 8);
		wxSizer* s3 = new wxStaticBoxSizer(wxVERTICAL, this, _LABEL_("windows"));
			s3->Add(open_sets_in_new_window, 0, wxALL, 4);
		s->Add(s3, 0, wxEXPAND | (wxALL & ~wxTOP), 8);
	SetSizer(s);
}

void GlobalPreferencesPage::store() {
	// locale
	int n = language->GetSelection();
	if (n == wxNOT_FOUND) return;
	Packaged* p = (Packaged*)language->GetClientData(n);
	settings.locale = p->name();
	// set the_locale?
	// open_sets_in_new_window
	settings.open_sets_in_new_window = open_sets_in_new_window->GetValue();
}

// ----------------------------------------------------------------------------- : Preferences page : display

DisplayPreferencesPage::DisplayPreferencesPage(Window* parent)
	: PreferencesPage(parent)
{
	// init controls
	high_quality       = new wxCheckBox(this, wxID_ANY, _BUTTON_("high quality"));
	borders            = new wxCheckBox(this, wxID_ANY, _BUTTON_("show lines"));
	draw_editing       = new wxCheckBox(this, wxID_ANY, _BUTTON_("show editing hints"));
	spellcheck_enabled = new wxCheckBox(this, wxID_ANY, _BUTTON_("spellcheck enabled"));
	#if USE_ZOOM_COMBOBOX
		zoom              = new wxComboBox(this, ID_ZOOM);
	#else
		zoom              = new wxSpinCtrl(this, ID_ZOOM);
	#endif
	non_normal_export = new wxCheckBox(this, wxID_ANY, _BUTTON_("zoom export"));
	//wxButton* columns = new wxButton(this, ID_SELECT_COLUMNS, _BUTTON_("select"));
	// set values
	high_quality->      SetValue( settings.default_stylesheet_settings.card_anti_alias());
	borders->           SetValue( settings.default_stylesheet_settings.card_borders());
	draw_editing->      SetValue( settings.default_stylesheet_settings.card_draw_editing());
	spellcheck_enabled->SetValue( settings.default_stylesheet_settings.card_spellcheck_enabled());
	non_normal_export-> SetValue(!settings.default_stylesheet_settings.card_normal_export());
	#if USE_ZOOM_COMBOBOX
		zoom_int = static_cast<int>(settings.default_stylesheet_settings.card_zoom() * 100);
		zoom->SetValue(String::Format(_("%d%%"),zoom_int));
		int choices[] = {50,66,75,100,120,150,200};
		for (unsigned int i = 0 ; i < sizeof(choices)/sizeof(choices[0]) ; ++i) {
			zoom->Append(String::Format(_("%d%%"),choices[i]));
		}
	#else
		zoom->SetRange(1, 1000);
		zoom->             SetValue(static_cast<int>(settings.default_stylesheet_settings.card_zoom() * 100));
	#endif
	// init sizer
	wxSizer* s = new wxBoxSizer(wxVERTICAL);
		wxSizer* s2 = new wxStaticBoxSizer(wxVERTICAL, this, _LABEL_("card display"));
			s2->Add(high_quality,       0, wxEXPAND | wxALL, 4);
			s2->Add(borders,            0, wxEXPAND | wxALL, 4);
			s2->Add(draw_editing,       0, wxEXPAND | wxALL, 4);
			s2->Add(spellcheck_enabled, 0, wxEXPAND | wxALL, 4);
			wxSizer* s3 = new wxBoxSizer(wxHORIZONTAL);
				s3->Add(new wxStaticText(this, wxID_ANY, _LABEL_("zoom")),             0, wxALL & ~wxLEFT,  4);
				s3->AddSpacer(2);
				s3->Add(zoom);
				s3->Add(new wxStaticText(this, wxID_ANY, _LABEL_("percent of normal")),1, wxALL & ~wxRIGHT, 4);
			s2->Add(s3, 0, wxEXPAND | wxALL, 4);
			s2->Add(non_normal_export,0, wxEXPAND | wxALL, 4);
			s2->Add(new wxStaticText(this, wxID_ANY,     _HELP_("zoom export")),       0, wxALL & ~wxTOP, 4);
		s->Add(s2, 0, wxEXPAND | wxALL, 8);
		/*
		wxSizer* s4 = new wxStaticBoxSizer(wxVERTICAL, this, _("Card List"));
			wxSizer* s5 = new wxBoxSizer(wxHORIZONTAL);
				s5->Add(new wxStaticText(this, wxID_ANY, _("Columns: ")),      0, wxALL & ~wxLEFT | wxEXPAND, 4);
				s5->Add(columns);
			s4->Add(s5, 0, wxEXPAND | wxALL, 4);
		s->Add(s4, 0, wxEXPAND | wxALL & ~wxTOP, 8);
		*/
	s->SetSizeHints(this);
	SetSizer(s);
}

void DisplayPreferencesPage::store() {
	settings.default_stylesheet_settings.card_anti_alias         = high_quality->GetValue();
	settings.default_stylesheet_settings.card_borders            = borders->GetValue();
	settings.default_stylesheet_settings.card_draw_editing       = draw_editing->GetValue();
	settings.default_stylesheet_settings.card_spellcheck_enabled = spellcheck_enabled->GetValue();
	#if USE_ZOOM_COMBOBOX
		updateZoom();
		settings.default_stylesheet_settings.card_zoom          = zoom_int / 100.0;
	#else
		settings.default_stylesheet_settings.card_zoom          = zoom->GetValue() / 100.0;
	#endif
	settings.default_stylesheet_settings.card_normal_export = !non_normal_export->GetValue();
}

void DisplayPreferencesPage::onSelectColumns(wxCommandEvent&) {
	// Impossible, set specific
}

#if USE_ZOOM_COMBOBOX
	void DisplayPreferencesPage::onZoomChange(wxCommandEvent&) {
		updateZoom();
	}
	void DisplayPreferencesPage::updateZoom() {
		String s = zoom->GetValue();
		int i = zoom_int;
		if (wxSscanf(s.c_str(),_("%u"),&i)) {
			zoom_int = min(max(i,1),1000);
		}
		zoom->SetValue(String::Format(_("%d%%"),(int)zoom_int));
	}
#endif

BEGIN_EVENT_TABLE(DisplayPreferencesPage, wxPanel)
	EVT_BUTTON       (ID_SELECT_COLUMNS, DisplayPreferencesPage::onSelectColumns)
	#if USE_ZOOM_COMBOBOX
		EVT_COMBOBOX     (ID_ZOOM, DisplayPreferencesPage::onZoomChange)
		EVT_TEXT_ENTER   (ID_ZOOM, DisplayPreferencesPage::onZoomChange)
	#endif
END_EVENT_TABLE  ()


// ----------------------------------------------------------------------------- : Preferences page : directories

DirsPreferencesPage::DirsPreferencesPage(Window* parent)
	: PreferencesPage(parent)
{
	// init controls
	apprentice   = new wxTextCtrl(this, wxID_ANY);
	wxButton* ab = new wxButton(this, ID_APPRENTICE_BROWSE, _BUTTON_("browse"));
	// set values
	apprentice->SetValue(settings.apprentice_location);
	// init sizer
	wxSizer* s = new wxBoxSizer(wxVERTICAL);
		wxSizer* s2 = new wxStaticBoxSizer(wxVERTICAL, this, _LABEL_("external programs"));
			s2->Add(new wxStaticText(this, wxID_ANY, _LABEL_("apprentice")), 0, wxALL, 4);
			wxSizer* s3 = new wxBoxSizer(wxHORIZONTAL);
				s3->Add(apprentice, 1, wxEXPAND | wxRIGHT, 4);
				s3->Add(ab,         0, wxEXPAND);
			s2->Add(s3, 0, wxEXPAND | (wxALL & ~wxTOP), 4);
		s->Add(s2, 0, wxEXPAND | wxALL, 8);
	s->SetSizeHints(this);
	SetSizer(s);
}

void DirsPreferencesPage::store() {
	settings.apprentice_location = apprentice->GetValue();
}

void DirsPreferencesPage::onApprenticeBrowse(wxCommandEvent&) {
	// browse for appr.exe
	wxFileDialog dlg(this, _TITLE_("locate apprentice"), apprentice->GetValue(), _(""), _LABEL_("apprentice exe") + _("|appr.exe"), wxFD_OPEN);
	if (dlg.ShowModal() == wxID_OK) {
		wxFileName fn(dlg.GetPath());
		apprentice->SetValue(fn.GetPath());
	}
}
	
BEGIN_EVENT_TABLE(DirsPreferencesPage, wxPanel)
	EVT_BUTTON     (ID_APPRENTICE_BROWSE, DirsPreferencesPage::onApprenticeBrowse)
END_EVENT_TABLE  ();


// ----------------------------------------------------------------------------- : Preferences page : updates

UpdatePreferencesPage::UpdatePreferencesPage(Window* parent)
	: PreferencesPage(parent)
{
	// init controls
	check_at_startup    = new wxChoice(this, wxID_ANY);
	wxButton* check_now = new wxButton(this, ID_CHECK_UPDATES_NOW, _BUTTON_("check now"));
	// set values
	check_at_startup->Append(_BUTTON_("always"));                        // 0
	check_at_startup->Append(_BUTTON_("if internet connection exists")); // 1
	check_at_startup->Append(_BUTTON_("never"));                         // 2
	check_at_startup->SetSelection(settings.check_updates);
	// init sizer
	wxSizer* s = new wxBoxSizer(wxVERTICAL);
		s->Add(new wxStaticText(this, wxID_ANY, _LABEL_("check at startup")), 0, wxALL, 8);
		s->Add(check_at_startup, 0, wxALL & ~wxTOP, 8);
		s->Add(check_now,        0, wxALL & ~wxTOP, 8);
		s->Add(new wxStaticText(this, wxID_ANY, _LABEL_("checking requires internet")), 0, wxALL & ~wxTOP, 8);
	SetSizer(s);
}

void UpdatePreferencesPage::store() {
	int sel = check_at_startup->GetSelection();
	if      (sel == 0) settings.check_updates = CHECK_ALWAYS;
	else if (sel == 1) settings.check_updates = CHECK_IF_CONNECTED;
	else               settings.check_updates = CHECK_NEVER;
}

void UpdatePreferencesPage::onCheckUpdatesNow(wxCommandEvent&) {
	check_updates_now(false);
	if (!update_data_found()) {
		wxMessageBox(_ERROR_("checking updates failed"), _TITLE_("update check"), wxICON_ERROR | wxOK);
	} else if (!update_available()) {
		wxMessageBox(_ERROR_("no updates"),              _TITLE_("update check"), wxICON_INFORMATION | wxOK);
	} else {
		show_update_dialog(GetParent());
	}
}

BEGIN_EVENT_TABLE(UpdatePreferencesPage, wxPanel)
	EVT_BUTTON      (ID_CHECK_UPDATES_NOW, UpdatePreferencesPage::onCheckUpdatesNow)
END_EVENT_TABLE  ()
