//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#ifndef HEADER_GUI_SYMBOL_WINDOW
#define HEADER_GUI_SYMBOL_WINDOW

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/symbol.hpp>
#include <wx/listctrl.h>

class SymbolControl;
class SymbolPartList;
DECLARE_POINTER_TYPE(ValueActionPerformer);

// ----------------------------------------------------------------------------- : SymbolWindow

/// The window for editing symbols
class SymbolWindow : public wxFrame {
  public:
	/// Construct a SymbolWindow
	SymbolWindow(Window* parent);
	/// Construct a SymbolWindow showing a symbol from a file
	SymbolWindow(Window* parent, const String& filename);
	/// Construct a SymbolWindow showing a symbol value in a set
	SymbolWindow(Window* parent, ValueActionPerformer* performer);
	~SymbolWindow();
	
  private:
	// --------------------------------------------------- : Children
	
	/// Actual initialisation
	void init(Window* parent, SymbolP symbol);
	
	SymbolControl*  control; ///< The control for editing/displaying the symbol
	SymbolPartList* parts;   ///< A list of parts in the symbol
	
	// when editing a symbol field
	ValueActionPerformer* performer;
	
	// --------------------------------------------------- : Event handling
	DECLARE_EVENT_TABLE();
	
	void onFileNew   (wxCommandEvent&);
	void onFileOpen  (wxCommandEvent&);
	void onFileSave  (wxCommandEvent&);
	void onFileSaveAs(wxCommandEvent&);
	void onFileStore (wxCommandEvent&);
	void onFileExit  (wxCommandEvent&);
	
	void onEditUndo  (wxCommandEvent&);
	void onEditRedo  (wxCommandEvent&);
	
	void onModeChange(wxCommandEvent&);
	void onExtraTool (wxCommandEvent&);
	
	void onUpdateUI(wxUpdateUIEvent&);
	
	/// Changing selected parts in the list
	void onSelectFromList(wxCommandEvent& ev);
	/// Activating a part: open the point editor
	void onActivateFromList(wxCommandEvent& ev);
	
	bool inSelectionEvent; ///< Prevent recursion in onSelect...
	
  public:
	void onSelectFromControl();
};

// ----------------------------------------------------------------------------- : EOF
#endif
