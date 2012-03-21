//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#ifndef HEADER_GUI_SET_WINDOW
#define HEADER_GUI_SET_WINDOW

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/set.hpp>
#include <wx/fdrepdlg.h>
#include <gui/card_select_window.hpp>

class IconMenu;
class SetWindowPanel;
class wxFindDialogEvent;
struct CardSelectEvent;

// ----------------------------------------------------------------------------- : SetWindow

/// The main window of MSE, used for editing Sets.
/** This window consists of several panels, only one panel is visible at a time.
 */
class SetWindow : public wxFrame, public SetView {
  public:
	/// Construct a SetWindow
	SetWindow(Window* parent, const SetP& set);
	~SetWindow();
	
	/// Set the icon of one of the panels
	void setPanelIcon(SetWindowPanel* panel, wxBitmap const& icon);
	
	// --------------------------------------------------- : Set actions
	
  private:
	DECLARE_EVENT_TABLE();
	
	// --------------------------------------------------- : Data
	
	// gui items
	vector<SetWindowPanel*> panels;        ///< All panels on this window
	SetWindowPanel*         current_panel;
	IconMenu*               menuExport;
	
	/// Number of items in the recent sets list
	size_t number_of_recentSets;
	
	// data for find/replace
	wxDialog* find_dialog;
	wxFindReplaceData find_data;
		
	// --------------------------------------------------- : Panel managment
	
	/// Add a panel to the window, as well as to the menu and tab bar
	/** The position only determines the order in which events will be send.
	 */
	void addPanel(IconMenu* windowMenu, wxToolBar* tabBar, SetWindowPanel* panel, UInt pos, const String& image_name, const String& name);
	
	/// Select a panel, based on a tab id
	void selectPanel(int id);
	
	// --------------------------------------------------- : Managing multiple main windows
	
	/// All opened set windows
	static vector<SetWindow*> set_windows;
	
	/// Is this the only window that has this set?
	bool isOnlyWithSet();
	
	/// Switch this window to the new set, or open another window for it (depending on the settings)
	void switchSet(const SetP& new_set);
	
	// --------------------------------------------------- : Action related
  protected:
	/// We want to respond to set changes
	virtual void onChangeSet();
	/// Actions that change the set
	virtual void onAction(const Action&, bool undone);
	
  public:
	// minSize = mainSizer->getMinWindowSize(this)
	// but wx made that private
	void fixMinWindowSize();
  private:
	/// Update the window title based on the set name
	void updateTitle();
	
	// --------------------------------------------------- : Cards
	
	/// A different card has been selected
	void onCardSelect(CardSelectEvent&);
	void onCardActivate(CardSelectEvent&);
	/// Card subsets that can be exported
	void selectionChoices(ExportCardSelectionChoices& out);
	
	// --------------------------------------------------- : Window events - close
	
	/// Ask the user to save the set
	void onClose(wxCloseEvent&);
	
	/// Ask if the user wants to save the set
	/** Returns true  if the action can be continued (the set was saved, or need not be saved)
	 *  Returns false if the action should be canceled (user pressed cancel, or save failed)
	 */
	bool askSaveAndContinue();
	
	// --------------------------------------------------- : Window events - update UI
		
	void onUpdateUI(wxUpdateUIEvent&);
	/// The number of 'recent set' menu items shown
	UInt number_of_recent_sets;
	void updateRecentSets();
	
	// --------------------------------------------------- : Window events - menu - file
	void onFileNew             (wxCommandEvent&);
	void onFileOpen            (wxCommandEvent&);
	void onFileSave            (wxCommandEvent&);
	void onFileSaveAs          (wxCommandEvent&);
//	void onFileInspect         (wxCommandEvent&);
	void onFileExportMenu      (wxCommandEvent&);
	void onFileExportImage     (wxCommandEvent&);
	void onFileExportImages    (wxCommandEvent&);
	void onFileExportHTML      (wxCommandEvent&);
	void onFileExportApprentice(wxCommandEvent&);
	void onFileExportMWS       (wxCommandEvent&);
	void onFileCheckUpdates    (wxCommandEvent&);
	void onFileProfiler        (wxCommandEvent&);
	void onFilePrint           (wxCommandEvent&);
	void onFilePrintPreview    (wxCommandEvent&);
	void onFileReload          (wxCommandEvent&);
	void onFileRecent          (wxCommandEvent&);
	void onFileExit            (wxCommandEvent&);
	
	// --------------------------------------------------- : Window events - menu - edit
	
	void onEditUndo            (wxCommandEvent&);
	void onEditRedo            (wxCommandEvent&);
	void onEditCut             (wxCommandEvent&);
	void onEditCopy            (wxCommandEvent&);
	void onEditPaste           (wxCommandEvent&);
	void onEditFind            (wxCommandEvent&);
	void onEditFindNext        (wxCommandEvent&);
	void onEditReplace         (wxCommandEvent&);
	void onEditAutoReplace     (wxCommandEvent&);
	void onEditPreferences     (wxCommandEvent&);
	
	void onFind                (wxFindDialogEvent&);
	void onFindNext            (wxFindDialogEvent&);
	void onReplace             (wxFindDialogEvent&);
	void onReplaceAll          (wxFindDialogEvent&);
	
	// --------------------------------------------------- : Window events - menu - window
	
	void onWindowNewWindow     (wxCommandEvent&);
	void onWindowSelect        (wxCommandEvent&);
	// --------------------------------------------------- : Window events - menu - help
	
	void onHelpIndex           (wxCommandEvent&);
	void onHelpWebsite         (wxCommandEvent&);
	void onHelpAbout           (wxCommandEvent&);
	
	// --------------------------------------------------- : Window events - other
	
	void onChildMenu           (wxCommandEvent&);
	void onMenuOpen            (wxMenuEvent&);
	
	void onIdle                (wxIdleEvent&);
	
	void onSizeChange          (wxCommandEvent&);
	void onEraseBackground     (wxEraseEvent&) {} // reduce flicker
};

// ----------------------------------------------------------------------------- : EOF
#endif
