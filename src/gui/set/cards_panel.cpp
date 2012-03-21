//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/set/cards_panel.hpp>
#include <gui/control/image_card_list.hpp>
#include <gui/control/card_editor.hpp>
#include <gui/control/text_ctrl.hpp>
#include <gui/control/filter_ctrl.hpp>
#include <gui/about_window.hpp> // for HoverButton
#include <gui/update_checker.hpp>
#include <gui/icon_menu.hpp>
#include <gui/util.hpp>
#include <data/set.hpp>
#include <data/game.hpp>
#include <data/card.hpp>
#include <data/add_cards_script.hpp>
#include <data/action/set.hpp>
#include <data/settings.hpp>
#include <util/find_replace.hpp>
#include <util/tagged_string.hpp>
#include <util/window_id.hpp>
#include <wx/splitter.h>

DECLARE_TYPEOF_COLLECTION(AddCardsScriptP);

#ifdef EVT_TOOL_DROPDOWN
	// This is only available after patching wx or in version 2.10
	// see http://trac.wxwidgets.org/ticket/8556
	#define HAVE_TOOLBAR_DROPDOWN_MENU 1
#endif

// ----------------------------------------------------------------------------- : CardsPanel

CardsPanel::CardsPanel(Window* parent, int id)
	: SetWindowPanel(parent, id)
{
	// init controls
	editor      = new CardEditor(this, ID_EDITOR);
	splitter    = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
	card_list   = new FilteredImageCardList(splitter, ID_CARD_LIST);
	nodes_panel = new wxPanel(splitter, wxID_ANY);
	notes       = new TextCtrl(nodes_panel, ID_NOTES, true);
	collapse_notes = new HoverButton(nodes_panel, ID_COLLAPSE_NOTES, _("btn_collapse"), wxNullColour, false);
	collapse_notes->SetExtraStyle(wxWS_EX_PROCESS_UI_UPDATES);
	filter    = nullptr;
	editor->next_in_tab_order = card_list;
	// init sizer for notes panel
	wxSizer* sn = new wxBoxSizer(wxVERTICAL);
		wxSizer* sc = new wxBoxSizer(wxHORIZONTAL);
		sc->Add(new wxStaticText(nodes_panel, wxID_ANY, _LABEL_("card notes")), 1, wxEXPAND | wxLEFT, 2);
		sc->Add(collapse_notes, 0, wxALIGN_CENTER | wxRIGHT, 2);
	sn->Add(sc, 0, wxEXPAND, 2);
	sn->Add(notes, 1, wxEXPAND | wxTOP, 2);
	nodes_panel->SetSizer(sn);
	// init splitter
	splitter->SetMinimumPaneSize(15);
	splitter->SetSashGravity(1.0);
	splitter->SplitHorizontally(card_list, nodes_panel, -40);
	notes_below_editor = false;
	// init sizer
	wxSizer* s = new wxBoxSizer(wxHORIZONTAL);
		s_left = new wxBoxSizer(wxVERTICAL);
		s_left->Add(editor);
	s->Add(s_left,   0, wxEXPAND | wxRIGHT, 2);
	s->Add(splitter, 1, wxEXPAND);
	s->SetSizeHints(this);
	SetSizer(s);
	
	// init menus
	menuCard = new IconMenu();
		menuCard->Append(ID_CARD_PREV,								_MENU_("previous card"),	_HELP_("previous card"));
		menuCard->Append(ID_CARD_NEXT,								_MENU_("next card"),		_HELP_("next card"));
		menuCard->AppendSeparator();
		menuCard->Append(ID_CARD_ADD,		_("card_add"),			_MENU_("add card"),			_HELP_("add card"));
		insertManyCardsMenu = new wxMenuItem(menuCard, ID_CARD_ADD_MULT, _MENU_("add cards"), _HELP_("add cards"));
		set_menu_item_image(insertManyCardsMenu, _("card_add_multiple"));
		((wxMenu*)menuCard)->Append(insertManyCardsMenu);
																	// NOTE: space after "Del" prevents wx from making del an accellerator
																	// otherwise we delete a card when delete is pressed inside the editor
																	// Adding a space never hurts, please keep it just to be safe.
		menuCard->Append(ID_CARD_REMOVE,	_("card_del"),			_MENU_("remove card")+_(" "),_HELP_("remove card"));
		menuCard->AppendSeparator();
		IconMenu* menuRotate = new IconMenu();
			menuRotate->Append(ID_CARD_ROTATE_0,		_("card_rotate_0"),		_MENU_("rotate 0"),		_HELP_("rotate 0"),		wxITEM_CHECK);
			menuRotate->Append(ID_CARD_ROTATE_270,		_("card_rotate_270"),	_MENU_("rotate 270"),	_HELP_("rotate 270"),	wxITEM_CHECK);
			menuRotate->Append(ID_CARD_ROTATE_90,		_("card_rotate_90"),	_MENU_("rotate 90"),	_HELP_("rotate 90"),	wxITEM_CHECK);
			menuRotate->Append(ID_CARD_ROTATE_180,		_("card_rotate_180"),	_MENU_("rotate 180"),	_HELP_("rotate 180"),	wxITEM_CHECK);
		menuCard->Append(wxID_ANY,			_("card_rotate"),		_MENU_("orientation"),		_HELP_("orientation"),		wxITEM_NORMAL, menuRotate);
		menuCard->AppendSeparator();
		// This probably belongs in the window menu, but there we can't remove the separator once it is added
		menuCard->Append(ID_SELECT_COLUMNS,							_MENU_("card list columns"),_HELP_("card list columns"));
	
	menuFormat = new IconMenu();
		menuFormat->Append(ID_FORMAT_BOLD,		_("bold"),			_MENU_("bold"),				_HELP_("bold"),				wxITEM_CHECK);
		menuFormat->Append(ID_FORMAT_ITALIC,	_("italic"),		_MENU_("italic"),			_HELP_("italic"),			wxITEM_CHECK);
		menuFormat->Append(ID_FORMAT_SYMBOL,	_("symbol"),		_MENU_("symbols"),			_HELP_("symbols"),			wxITEM_CHECK);
		menuFormat->Append(ID_FORMAT_REMINDER,	_("reminder"),		_MENU_("reminder text"),	_HELP_("reminder text"),	wxITEM_CHECK);
		menuFormat->AppendSeparator();
		insertSymbolMenu = new wxMenuItem(menuFormat, ID_INSERT_SYMBOL, _MENU_("insert symbol"));
		menuFormat->Append(insertSymbolMenu);
	
	toolAddCard = nullptr;
}

void CardsPanel::updateNotesPosition() {
	wxSize editor_size = editor->GetBestSize();
	int room_below_editor = GetSize().y - editor_size.y;
	bool should_be_below = room_below_editor > 100;
	// move?
	if (should_be_below && !notes_below_editor) {
		notes_below_editor = true;
		// move the notes_panel to below the editor, it gets this as its parent
		splitter->Unsplit(nodes_panel);
		nodes_panel->Reparent(this);
		s_left->Add(nodes_panel, 1, wxEXPAND | wxTOP, 2);
		collapse_notes->Hide();
		nodes_panel->Show();
	} else if (!should_be_below && notes_below_editor) {
		notes_below_editor = false;
		// move the notes_panel back to below the card list
		s_left->Detach(nodes_panel);
		nodes_panel->Reparent(splitter);
		collapse_notes->Show();
		splitter->SplitHorizontally(card_list, nodes_panel, -80);
	}
}
bool CardsPanel::Layout() {
	updateNotesPosition();
	return SetWindowPanel::Layout();
}

/*void removeInsertSymbolMenu() {
		menuFormat->Append(ID_INSERT_SYMBOL,	_(""),				 _MENU_("insert symbol"));
}*/// TODO
CardsPanel::~CardsPanel() {
//	settings.card_notes_height = splitter->GetSashPosition();
	// we don't own the submenu
	wxMenu* menu = insertSymbolMenu->GetSubMenu();
	if (menu && menu->GetParent() == menuFormat) {
		menu->SetParent(nullptr);
	}
	insertSymbolMenu->SetSubMenu(nullptr); 
	// delete menus
	delete menuCard;
	delete menuFormat;
}

void CardsPanel::onChangeSet() {
	editor->setSet(set);
	notes->setSet(set);
	card_list->setSet(set);
	
	// change insertManyCardsMenu
	delete insertManyCardsMenu->GetSubMenu();
	insertManyCardsMenu->SetSubMenu(makeAddCardsSubmenu(false));
	// re-add the menu
	menuCard->Remove(ID_CARD_ADD_MULT);
	((wxMenu*)menuCard)->Insert(4,insertManyCardsMenu); // HACK: the position is hardcoded
	// also for the toolbar dropdown menu
	#if HAVE_TOOLBAR_DROPDOWN_MENU
		if (toolAddCard) {
			toolAddCard->SetDropdownMenu(makeAddCardsSubmenu(true));
		}
	#endif
}

wxMenu* CardsPanel::makeAddCardsSubmenu(bool add_single_card_option) {
	IconMenu* cards_scripts_menu = nullptr;
	// default item?
	if (add_single_card_option) {
		cards_scripts_menu = new IconMenu;
		cards_scripts_menu->Append(ID_CARD_ADD, _("card_add"), _MENU_("add card"), _HELP_("add card"));
		cards_scripts_menu->AppendSeparator();
	}
	// create menu for add_cards_scripts
	if (set && set->game && !set->game->add_cards_scripts.empty()) {
		int id = ID_ADD_CARDS_MENU_MIN;
		if (!cards_scripts_menu) cards_scripts_menu = new IconMenu;
		FOR_EACH(cs, set->game->add_cards_scripts) {
			cards_scripts_menu->Append(id++, cs->name, cs->description);
		}
	}
	return cards_scripts_menu;
}

// ----------------------------------------------------------------------------- : UI

void CardsPanel::initUI(wxToolBar* tb, wxMenuBar* mb) {
	// Toolbar
	tb->AddTool(ID_FORMAT_BOLD,		_(""), load_resource_tool_image(_("bold")),			wxNullBitmap, wxITEM_CHECK, _TOOLTIP_("bold"),			_HELP_("bold"));
	tb->AddTool(ID_FORMAT_ITALIC,	_(""), load_resource_tool_image(_("italic")),		wxNullBitmap, wxITEM_CHECK, _TOOLTIP_("italic"),		_HELP_("italic"));
	tb->AddTool(ID_FORMAT_SYMBOL,	_(""), load_resource_tool_image(_("symbol")),		wxNullBitmap, wxITEM_CHECK, _TOOLTIP_("symbols"),		_HELP_("symbols"));
	tb->AddTool(ID_FORMAT_REMINDER,	_(""), load_resource_tool_image(_("reminder")),		wxNullBitmap, wxITEM_CHECK, _TOOLTIP_("reminder text"),	_HELP_("reminder text"));
	tb->AddSeparator();
	#if HAVE_TOOLBAR_DROPDOWN_MENU
		toolAddCard = tb->AddTool(ID_CARD_ADD,		_(""), load_resource_tool_image(_("card_add")),		wxNullBitmap, wxITEM_DROPDOWN,_TOOLTIP_("add card"),		_HELP_("add card"));
		toolAddCard->SetDropdownMenu(makeAddCardsSubmenu(true));
	#else
		tb->AddTool(ID_CARD_ADD,		_(""), load_resource_tool_image(_("card_add")),		wxNullBitmap, wxITEM_NORMAL,_TOOLTIP_("add card"),		_HELP_("add card"));
	#endif
	tb->AddTool(ID_CARD_REMOVE,		_(""), load_resource_tool_image(_("card_del")),		wxNullBitmap, wxITEM_NORMAL,_TOOLTIP_("remove card"),	_HELP_("remove card"));
	tb->AddSeparator();
	#if HAVE_TOOLBAR_DROPDOWN_MENU
		wxToolBarToolBase* rot = tb->AddTool(ID_CARD_ROTATE,		_(""), load_resource_tool_image(_("card_rotate")),	wxNullBitmap, wxITEM_DROPDOWN, _TOOLTIP_("rotate card"),	_HELP_("rotate card"));
		IconMenu* menuRotate = new IconMenu();
			menuRotate->Append(ID_CARD_ROTATE_0,		_("card_rotate_0"),		_MENU_("rotate 0"),		_HELP_("rotate 0"),		wxITEM_CHECK);
			menuRotate->Append(ID_CARD_ROTATE_270,		_("card_rotate_270"),	_MENU_("rotate 270"),	_HELP_("rotate 270"),	wxITEM_CHECK);
			menuRotate->Append(ID_CARD_ROTATE_90,		_("card_rotate_90"),	_MENU_("rotate 90"),	_HELP_("rotate 90"),	wxITEM_CHECK);
			menuRotate->Append(ID_CARD_ROTATE_180,		_("card_rotate_180"),	_MENU_("rotate 180"),	_HELP_("rotate 180"),	wxITEM_CHECK);
		rot->SetDropdownMenu(menuRotate);
	#else
		tb->AddTool(ID_CARD_ROTATE,		_(""), load_resource_tool_image(_("card_rotate")),	wxNullBitmap,wxITEM_NORMAL, _TOOLTIP_("rotate card"),	_HELP_("rotate card"));
	#endif
	// Filter/search textbox
	tb->AddSeparator();
	assert(!filter);
	if (!filter) {
		filter = new FilterCtrl(tb, ID_CARD_FILTER, _LABEL_("search cards"));
		// the control should show what the list is still filtered by
		filter->setFilter(filter_value);
	}
	wxLogDebug(L"%p -> %p", filter, tb);
	tb->AddControl(filter);
	tb->Realize();
	// Menus
	mb->Insert(2, menuCard,   _MENU_("cards"));
	mb->Insert(3, menuFormat, _MENU_("format"));
}

void CardsPanel::destroyUI(wxToolBar* tb, wxMenuBar* mb) {
	// Toolbar
	tb->DeleteTool(ID_FORMAT_BOLD);
	tb->DeleteTool(ID_FORMAT_ITALIC);
	tb->DeleteTool(ID_FORMAT_SYMBOL);
	tb->DeleteTool(ID_FORMAT_REMINDER);
	tb->DeleteTool(ID_CARD_ADD);
	tb->DeleteTool(ID_CARD_REMOVE);
	tb->DeleteTool(ID_CARD_ROTATE);
	// remember the value in the filter control, because the card list remains filtered
	// the control is destroyed by DeleteTool
	filter_value = filter->getFilterString();
	tb->DeleteTool(filter->GetId());
	filter = nullptr;
	// HACK: hardcoded size of rest of toolbar
	tb->DeleteToolByPos(12); // delete separator
	tb->DeleteToolByPos(12); // delete separator
	tb->DeleteToolByPos(12); // delete separator
	// Menus
	mb->Remove(3);
	mb->Remove(2);
	toolAddCard = nullptr;
}

void CardsPanel::onUpdateUI(wxUpdateUIEvent& ev) {
	switch (ev.GetId()) {
		case ID_CARD_PREV:       ev.Enable(card_list->canSelectPrevious());	break;
		case ID_CARD_NEXT:       ev.Enable(card_list->canSelectNext());		break;
		case ID_CARD_ROTATE_0: case ID_CARD_ROTATE_90: case ID_CARD_ROTATE_180: case ID_CARD_ROTATE_270: {
			StyleSheetSettings& ss = settings.stylesheetSettingsFor(set->stylesheetFor(card_list->getCard()));
			int a = ev.GetId() == ID_CARD_ROTATE_0   ? 0
			      : ev.GetId() == ID_CARD_ROTATE_90  ? 90
			      : ev.GetId() == ID_CARD_ROTATE_180 ? 180
			      :                                    270;
			ev.Check(ss.card_angle() == a);
			break;
		}
		case ID_CARD_ADD_MULT: {
			ev.Enable(insertManyCardsMenu->GetSubMenu() != nullptr);
			break;
		}
		case ID_CARD_REMOVE:     ev.Enable(card_list->canDelete());			break;
		case ID_FORMAT_BOLD: case ID_FORMAT_ITALIC: case ID_FORMAT_SYMBOL: case ID_FORMAT_REMINDER: {
			if (focused_control(this) == ID_EDITOR) {
				ev.Enable(editor->canFormat(ev.GetId()));
				ev.Check (editor->hasFormat(ev.GetId()));
			} else {
				ev.Enable(false);
				ev.Check(false);
			}
			break;
		}
		case ID_COLLAPSE_NOTES: {
			bool collapse = notes->GetSize().y > 0;
			collapse_notes->loadBitmaps(collapse ? _("btn_collapse") : _("btn_expand"));
			collapse_notes->SetHelpText(collapse ? _HELP_("collapse notes") : _HELP_("expand notes"));
			break;
		}
#if 0 //ifdef __WXGTK__ //crashes on GTK
		case ID_INSERT_SYMBOL: ev.Enable(false); break;
#else
		case ID_INSERT_SYMBOL: {
			wxMenu* menu = editor->getMenu(ID_INSERT_SYMBOL);
			ev.Enable(menu);
			break;
		}
#endif
	}
}

void CardsPanel::onMenuOpen(wxMenuEvent& ev) {
	if (ev.GetMenu() != menuFormat) return;
	wxMenu* menu = editor->getMenu(ID_INSERT_SYMBOL);
	if (insertSymbolMenu->GetSubMenu() != menu || (menu && menu->GetParent() != menuFormat)) {
		// re-add the menu
		fprintf(stderr,"insert1 %p %p\n", menuFormat,insertSymbolMenu);fflush(stderr);
		menuFormat->Remove(ID_INSERT_SYMBOL);
		fprintf(stderr,"insert2\n");fflush(stderr);
		insertSymbolMenu->SetSubMenu(menu);
		fprintf(stderr,"insert3\n");fflush(stderr);
		menuFormat->Append(insertSymbolMenu);
		fprintf(stderr,"insert4\n");fflush(stderr);
	}
}

void CardsPanel::onCommand(int id) {
	switch (id) {
		case ID_CARD_PREV:
			// Note: Forwarded events may cause this to occur even at the top.
			if (card_list->canSelectPrevious()) card_list->selectPrevious();
			break;
		case ID_CARD_NEXT:
			// Note: Forwarded events may cause this to occur even at the bottom.
			if (card_list->canSelectNext()) card_list->selectNext();
			break;
		case ID_CARD_ADD:
			set->actions.addAction(new AddCardAction(*set));
			break;
		case ID_CARD_REMOVE:
			card_list->doDelete();
			break;
		case ID_CARD_ROTATE:
		case ID_CARD_ROTATE_0: case ID_CARD_ROTATE_90: case ID_CARD_ROTATE_180: case ID_CARD_ROTATE_270: {
			StyleSheetSettings& ss = settings.stylesheetSettingsFor(set->stylesheetFor(card_list->getCard()));
			ss.card_angle.assign(
				  id == ID_CARD_ROTATE     ? sane_fmod(ss.card_angle() + 90, 360)
				: id == ID_CARD_ROTATE_0   ? 0
				: id == ID_CARD_ROTATE_90  ? 90
				: id == ID_CARD_ROTATE_180 ? 180
				:                            270
			);
			set->actions.tellListeners(DisplayChangeAction(),true);
			break;
		}
		case ID_SELECT_COLUMNS: {
			card_list->selectColumns();
		}
		case ID_FORMAT_BOLD: case ID_FORMAT_ITALIC: case ID_FORMAT_SYMBOL: case ID_FORMAT_REMINDER: {
			if (focused_control(this) == ID_EDITOR) {
				editor->doFormat(id);
			}
			break;
		}
		case ID_COLLAPSE_NOTES: {
			bool collapse = notes->GetSize().y > 0;
			if (collapse) {
				splitter->SetSashPosition(-1);
				card_list->SetFocus();
			} else {
				splitter->SetSashPosition(-150);
				notes->SetFocus();
			}
			break;
		}
		case ID_CARD_FILTER: {
			// card filter has changed, update the card list
			card_list->setFilter(filter->getFilter<Card>());
			break;
		}
		default: {
			if (id >= ID_INSERT_SYMBOL_MENU_MIN && id <= ID_INSERT_SYMBOL_MENU_MAX) {
				// pass on to editor
				editor->onCommand(id);
			} else if (id >= ID_ADD_CARDS_MENU_MIN && id <= ID_ADD_CARDS_MENU_MAX) {
				// add multiple cards
				AddCardsScriptP script = set->game->add_cards_scripts.at(id - ID_ADD_CARDS_MENU_MIN);
				script->perform(*set);
			}
		}
	}
}

// ----------------------------------------------------------------------------- : Actions

bool CardsPanel::wantsToHandle(const Action&, bool undone) const {
	return false;
}

// ----------------------------------------------------------------------------- : Clipboard

// determine what control to use for clipboard actions
#define CUT_COPY_PASTE(op,return)									\
	int id = focused_control(this);									\
	if      (id == ID_EDITOR)    { return editor->op();    }		\
	else if (id == ID_CARD_LIST) { return card_list->op(); }		\
	else if (id == ID_NOTES)     { return notes->op();     }		\
	else                         { return false;           }

bool CardsPanel::canCut()   const { CUT_COPY_PASTE(canCut,   return) }
bool CardsPanel::canCopy()  const { CUT_COPY_PASTE(canCopy,  return) }
void CardsPanel::doCut()          { CUT_COPY_PASTE(doCut,    return (void)) }
void CardsPanel::doCopy()         { CUT_COPY_PASTE(doCopy,   return (void)) }

// always alow pasting cards, even if something else is selected
bool CardsPanel::canPaste() const {
	if (card_list->canPaste()) return true;
	int id = focused_control(this);
	if      (id == ID_EDITOR) return editor->canPaste();
	else if (id == ID_NOTES)  return notes->canPaste();
	else                      return false;
}
void CardsPanel::doPaste() {
	if (card_list->canPaste()) {
		card_list->doPaste();
	} else {
		int id = focused_control(this);
		if      (id == ID_EDITOR) editor->doPaste();
		else if (id == ID_NOTES)  notes->doPaste();
	}
}

// ----------------------------------------------------------------------------- : Searching

class CardsPanel::SearchFindInfo : public FindInfo {
  public:
	SearchFindInfo(CardsPanel& panel, wxFindReplaceData& what) : FindInfo(what), panel(panel) {}
	virtual bool handle(const CardP& card, const TextValueP& value, size_t pos, bool was_selection) {
		// Select the card
		panel.card_list->setCard(card);
		return true;
	}
  private:
	CardsPanel& panel;
};

class CardsPanel::ReplaceFindInfo : public FindInfo {
  public:
	ReplaceFindInfo(CardsPanel& panel, wxFindReplaceData& what) : FindInfo(what), panel(panel) {}
	virtual bool handle(const CardP& card, const TextValueP& value, size_t pos, bool was_selection) {
		// Select the card
		panel.card_list->setCard(card);
		// Replace
		if (was_selection) {
			panel.editor->insert(escape(what.GetReplaceString()), _("Replace"));
			return false;
		} else {
			return true;
		}
	}
	virtual bool searchSelection() const { return true; }
  private:
	CardsPanel& panel;
};

bool CardsPanel::doFind(wxFindReplaceData& what) {
	SearchFindInfo find(*this, what);
	return search(find, false);
}
bool CardsPanel::doReplace(wxFindReplaceData& what) {
	ReplaceFindInfo find(*this, what);
	return search(find, false);
}
bool CardsPanel::doReplaceAll(wxFindReplaceData& what) {
	return false; // TODO
}

bool CardsPanel::search(FindInfo& find, bool from_start) {
	bool include = from_start;
	CardP current = card_list->getCard();
	for (size_t i = 0 ; i < set->cards.size() ; ++i) {
		CardP card = card_list->getCard( (long) (find.forward() ? i : set->cards.size() - i - 1) );
		if (card == current) include = true;
		if (include) {
			editor->setCard(card);
			if (editor->search(find, from_start || card != current)) {
				return true; // done
			}
		}
	}
	editor->setCard(current);
	return false;
}

// ----------------------------------------------------------------------------- : Selection

CardP CardsPanel::selectedCard() const {
	return card_list->getCard();
}
void CardsPanel::selectCard(const CardP& card) {
	if (!set) return; // we want onChangeSet first
	card_list->setCard(card);
	editor->setCard(card);
	notes->setValue(card ? &card->notes : nullptr);
	Layout();
	updateNotesPosition();
}

void CardsPanel::selectFirstCard() {
	if (!set) return; // we want onChangeSet first
	card_list->selectFirst();
}
