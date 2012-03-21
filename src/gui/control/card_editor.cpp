//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/control/card_editor.hpp>
#include <gui/value/editor.hpp>
#include <gui/icon_menu.hpp>
#include <data/field.hpp>
#include <data/stylesheet.hpp>
#include <data/settings.hpp>
#include <util/find_replace.hpp>
#include <util/window_id.hpp>
#include <wx/caret.h>
#include <boost/iterator/filter_iterator.hpp>

DECLARE_TYPEOF_COLLECTION(ValueViewerP);
DECLARE_TYPEOF_COLLECTION(ValueViewer*);

// ----------------------------------------------------------------------------- : DataEditor

DataEditor::DataEditor(Window* parent, int id, long style)
	: CardViewer(parent, id, style | wxWANTS_CHARS)
	, next_in_tab_order(nullptr)
	, current_viewer(nullptr)
	, current_editor(nullptr)
	, hovered_viewer(nullptr)
{
	// Create a caret
	SetCaret(new wxCaret(this,1,1));
}

ValueViewerP DataEditor::makeViewer(const StyleP& style) {
	return style->makeEditor(*this);
}

// ----------------------------------------------------------------------------- : Utility for ValueViewers

DrawWhat DataEditor::drawWhat(const ValueViewer* viewer) const {
	int what = DRAW_NORMAL
	         | DRAW_ACTIVE * viewerIsCurrent(viewer)
	         | DRAW_HOVER * (viewer == hovered_viewer);
	if (nativeLook()) {
		what |= DRAW_BOXES | DRAW_EDITING | DRAW_NATIVELOOK | DRAW_ERRORS;
	} else {
		StyleSheetSettings& ss = settings.stylesheetSettingsFor(set->stylesheetFor(card));
		what |= DRAW_BORDERS * ss.card_borders()
		     |  (DRAW_BOXES | DRAW_EDITING) * ss.card_draw_editing()
		     |  DRAW_ERRORS;
	}
	return (DrawWhat)what;
}

bool DataEditor::viewerIsCurrent(const ValueViewer* viewer) const {
	return viewer == current_viewer && FindFocus() == this;
}


void DataEditor::addAction(Action* action) {
	set->actions.addAction(action);
}

// ----------------------------------------------------------------------------- : Algorithms (TODO: move)

/// Swap the order of comparison, i.e. greater-than instead of less-than
template <typename Comp>
struct SwapCompare {
	Comp comp;
	SwapCompare(Comp comp) : comp(comp) {}
	template <typename T, typename U> inline bool operator () (T x, U y) {
		return comp(y,x);
	}
};

// Return the next element in a collection after x, when using comp as ordering
template <typename It, typename V, typename Comp>
It next_element(It first, It last, V const& x, Comp comp) {
	It best = last;
	for (It it = first ; it != last ; ++it) {
		if (comp(x,*it)) {
			// this is a candidate
			if (best == last || comp(*it,*best)) {
				best = it;
			}
		}
	}
	return best;
}

template <typename It, typename V, typename Comp>
It prev_element(It first, It last, V const& x, Comp comp) {
	return next_element(first,last,x,SwapCompare<Comp>(comp));
}

// ----------------------------------------------------------------------------- : Selection

bool DataEditor::AcceptsFocus() const {
	return wxWindow::AcceptsFocus();
}

void DataEditor::select(ValueViewer* new_viewer) {
	ValueEditor* old_editor = current_editor;
	if (new_viewer) {
		current_viewer = new_viewer;
		current_editor = new_viewer->getEditor();
	} else {
		current_viewer = nullptr;
		current_editor = nullptr;
	}
	if (current_editor != old_editor) {
		// selection has changed
		if (old_editor)     old_editor->onLoseFocus();
		if (current_editor) current_editor->onFocus();
		onChange();
	}
}

bool DataEditor::selectWithTab(vector<ValueViewerP>::iterator const& it) {
	if (it != viewers.end()) {
		select(it->get());
		return true;
	} else {
		select(nullptr);
		return false;
	}
}

struct CompareTabOrder {
	bool operator() (ValueViewer* a, ValueViewer* b) {
		assert(a && b);
		Style& as = *a->getStyle(), &bs = *b->getStyle();
		// if tab_index differs, use that
		if (as.tab_index < as.tab_index) return true;
		if (as.tab_index > as.tab_index) return false;
		// otherwise look at the positions
		// TODO: does this actually give a total order?
		double vertical_overlap   = min(bs.bottom - as.top, as.bottom - bs.top);
		double horizontal_overlap = min(bs.right - as.left, as.right - bs.left);
		if (vertical_overlap > 0 && vertical_overlap > horizontal_overlap) {
			// fields overlap (mostly) vertically
			// the fields are almost on the same 'row'
			// compare horizontally first
			if (as.left < bs.left) return true; // horizontal sorting
			if (as.left > bs.left) return false;
			if (as.top  < bs.top)  return true; // vertical sorting
		} else {
			// compare vertically first
			if (as.top  < bs.top)  return true; // vertical sorting
			if (as.top  > bs.top)  return false;
			if (as.left < bs.left) return true; // horizontal sorting
		}
		// arbitrary order otherwise
		return a < b;
	}
	bool operator() (ValueViewerP const& a, ValueViewer* b) {
		return operator () (a.get(),b);
	}
	bool operator() (ValueViewer* a, ValueViewerP const& b) {
		return operator () (a,b.get());
	}
	bool operator() (ValueViewerP const& a, ValueViewerP const& b) {
		return operator () (a.get(),b.get());
	}
};

bool is_enabled(ValueViewerP const& v) {
	return v->getField()->editable && v->getStyle()->isVisible();
}

bool DataEditor::selectFirst() {
	// This would be nicer with boost::range, but filtered adaptor was only introduced in boost 1.42(?)
	return selectWithTab(std::min_element(
		boost::make_filter_iterator(is_enabled,viewers.begin(),viewers.end()),
		boost::make_filter_iterator(is_enabled,viewers.end(),viewers.end()),
		CompareTabOrder()).base());
}
bool DataEditor::selectLast() {
	return selectWithTab(std::max_element(
		boost::make_filter_iterator(is_enabled,viewers.begin(),viewers.end()),
		boost::make_filter_iterator(is_enabled,viewers.end(),viewers.end()),
		CompareTabOrder()).base());
}
bool DataEditor::selectNext() {
	if (!current_viewer) return selectFirst();
	return selectWithTab(next_element(
		boost::make_filter_iterator(is_enabled,viewers.begin(),viewers.end()),
		boost::make_filter_iterator(is_enabled,viewers.end(),viewers.end()),
		current_viewer,
		CompareTabOrder()).base());
}
bool DataEditor::selectPrevious() {
	if (!current_viewer) return selectLast();
	return selectWithTab(prev_element(
		boost::make_filter_iterator(is_enabled,viewers.begin(),viewers.end()),
		boost::make_filter_iterator(is_enabled,viewers.end(),viewers.end()),
		current_viewer,
		CompareTabOrder()).base());
}


void DataEditor::onInit() {
	current_viewer = nullptr;
	current_editor = nullptr;
	hovered_viewer = nullptr;
	// hide caret if it is shown
	wxCaret* caret = GetCaret();
	if (caret->IsVisible()) caret->Hide();
}
// ----------------------------------------------------------------------------- : Search / replace

bool DataEditor::search(FindInfo& find, bool from_start) {
	bool include = from_start;
	for (size_t i = 0 ; i < by_tab_index.size() ; ++i) {
		ValueViewer& viewer = *by_tab_index[find.forward() ? i : by_tab_index.size() - i - 1];
		if (&viewer == current_viewer) include = true;
		if (include && viewer.getField()->editable && viewer.getStyle()->isVisible()) {
			ValueEditor* editor = viewer.getEditor();
			if (editor && editor->search(find, from_start || &viewer != current_viewer)) {
				return true; // done
			}
		}
	}
	return false; // not done
}

// ----------------------------------------------------------------------------- : Clipboard & Formatting

bool DataEditor::canCut()            const { return current_editor && current_editor->canCut();        }
bool DataEditor::canCopy()           const { return current_editor && current_editor->canCopy();       }
bool DataEditor::canPaste()          const { return current_editor && current_editor->canPaste();      }
bool DataEditor::canFormat(int type) const { return current_editor && current_editor->canFormat(type); }
bool DataEditor::hasFormat(int type) const { return current_editor && current_editor->hasFormat(type); }

void DataEditor::doCut()                   { if    (current_editor)   current_editor->doCut();         }
void DataEditor::doCopy()                  { if    (current_editor)   current_editor->doCopy();        }
void DataEditor::doPaste()                 { if    (current_editor)   current_editor->doPaste();       }
void DataEditor::doFormat(int type)        { if    (current_editor)   current_editor->doFormat(type);  }


wxMenu* DataEditor::getMenu(int type) const {
	if (current_editor) {
		return current_editor->getMenu(type);
	} else {
		return nullptr;
	}
}
void DataEditor::onCommand(int id) {
	if (current_editor) {
		current_editor->onCommand(id);
	}
}

void DataEditor::insert(const String& text, const String& action_name) {
	if (current_editor) current_editor->insert(text, action_name);
}

// ----------------------------------------------------------------------------- : Mouse events

void DataEditor::onLeftDown(wxMouseEvent& ev) {
	ev.Skip(); // for focus
	CaptureMouse();
	// change selection?
	selectField(ev, &ValueEditor::onLeftDown);
}
void DataEditor::onLeftUp(wxMouseEvent& ev) {
	if (HasCapture()) ReleaseMouse();
	if (current_editor && current_viewer) {
		RealPoint pos = mousePoint(ev, *current_viewer);
		if (current_viewer->containsPoint(pos)) {
			current_editor->onLeftUp(pos, ev);
		}
	}
}
void DataEditor::onLeftDClick(wxMouseEvent& ev) {
	if (current_editor && current_viewer) {
		RealPoint pos = mousePoint(ev, *current_viewer);
		if (current_viewer->containsPoint(pos)) {
			current_editor->onLeftDClick(pos, ev);
		}
	}
}
void DataEditor::onRightDown(wxMouseEvent& ev) {
	ev.Skip(); // for context menu
	// change selection?
	selectField(ev, &ValueEditor::onRightDown);
}
void DataEditor::onMouseWheel(wxMouseEvent& ev) {
	if (current_editor && current_viewer) {
		RealPoint pos = mousePoint(ev, *current_viewer);
		if (current_viewer->containsPoint(pos)) {
			if (current_editor->onMouseWheel(pos, ev)) return;
		}
	}
	ev.Skip();
}

void DataEditor::onMotion(wxMouseEvent& ev) {
	if (current_editor && current_viewer) {
		RealPoint pos = mousePoint(ev, *current_viewer);
		current_editor->onMotion(pos, ev);
	}
	if (!HasCapture()) {
		// find editor under mouse
		ValueViewer* new_hovered_viewer = nullptr;
		FOR_EACH_REVERSE(v,viewers) { // find high z index fields first
			RealPoint pos = mousePoint(ev, *v);
			if (v->containsPoint(pos) && v->getField()->editable) {
				new_hovered_viewer = v.get();
				break;
			}
		}
		if (hovered_viewer && hovered_viewer != new_hovered_viewer) {
			ValueEditor* e = hovered_viewer->getEditor();
			RealPoint pos = mousePoint(ev, *hovered_viewer);
			if (e) e->onMouseLeave(pos, ev);
			if (hovered_viewer) redraw(*hovered_viewer);
			if (new_hovered_viewer) redraw(*new_hovered_viewer);
		}
		hovered_viewer = new_hovered_viewer;
		// change cursor and set status text
		wxFrame* frame = dynamic_cast<wxFrame*>( wxGetTopLevelParent(this) );
		if (hovered_viewer) {
			ValueEditor* e = hovered_viewer->getEditor();
			RealPoint pos = mousePoint(ev, *hovered_viewer);
			wxCursor c;
			if (e) c = e->cursor(pos);
			if (c.Ok()) SetCursor(c);
			else        SetCursor(wxCURSOR_ARROW);
			if (frame) frame->SetStatusText(hovered_viewer->getField()->description);
		} else {
			// no field under cursor
			SetCursor(wxCURSOR_ARROW);
			if (frame) frame->SetStatusText(wxEmptyString);
		}
	}
}

void DataEditor::onMouseLeave(wxMouseEvent& ev) {
	// on mouse leave for editor
	if (hovered_viewer) {
		ValueEditor* e = hovered_viewer->getEditor();
		if (e) e->onMouseLeave(mousePoint(ev,*hovered_viewer), ev);
		if (hovered_viewer) redraw(*hovered_viewer);
		hovered_viewer = nullptr;
	}
	// clear status text
	wxFrame* frame = dynamic_cast<wxFrame*>( wxGetTopLevelParent(this) );
	if (frame) frame->SetStatusText(wxEmptyString);
}

void DataEditor::selectField(wxMouseEvent& ev, bool (ValueEditor::*event)(const RealPoint&, wxMouseEvent&)) {
	// change viewer/editor
	ValueEditor* old_editor = current_editor;
	selectFieldNoEvents(ev);
	if (old_editor != current_editor) {
		// selection has changed, send focus events
		if (old_editor)     old_editor->onLoseFocus();
		if (current_editor) current_editor->onFocus();
	}
	// pass event
	if (current_editor && current_viewer) {
		RealPoint pos = mousePoint(ev, *current_viewer);
		if (current_viewer->containsPoint(pos)) {
			(current_editor->*event)(pos, ev);
		}
	}
	// refresh?
	if (old_editor != current_editor) {
		// selection has changed, refresh viewers
		// NOTE: after passing mouse down event, otherwise opening combo box produces flicker
		onChange();
	}
}
void DataEditor::selectFieldNoEvents(const wxMouseEvent& ev) {
	FOR_EACH_EDITOR_REVERSE { // find high z index fields first
		int y;
		if (v->getField()->editable && (v->containsPoint(mousePoint(ev,*v)) ||
		        (nativeLook() && (y = ev.GetY() + GetScrollPos(wxVERTICAL)) >= v->getStyle()->top
		                      && y < v->getStyle()->bottom) )) {
			current_viewer = v.get();
			current_editor = e;
			return;
		}
	}
}

RealPoint DataEditor::mousePoint(const wxMouseEvent& ev, const ValueViewer& viewer) {
	Rotation rot = getRotation();
	Rotater r(rot,viewer.getRotation());
	return rot.trInv(RealPoint(ev.GetX(), ev.GetY()));
}

void DataEditor::onLoseCapture(wxMouseCaptureLostEvent&) {
	// We already test for wrong release with HasCapture()
	// but stupid wxwidget people decided to throw assertion failures
}

// ----------------------------------------------------------------------------- : Keyboard events

void DataEditor::onChar(wxKeyEvent& ev) {
	if (ev.GetKeyCode() == WXK_TAB) {
		if (!ev.ShiftDown()) {
			// try to select the next editor
			if (selectNext()) return;
			// send a navigation event to our parent, to select another control
			wxNavigationKeyEvent evt;
			GetParent()->HandleWindowEvent(evt);
		} else {
			// try to select the previos editor
			if (selectPrevious()) return;
			// send a navigation event to our parent, to select another control
			wxNavigationKeyEvent evt;
			evt.SetDirection(false);
			GetParent()->HandleWindowEvent(evt);
		}
	} else if (current_editor) {
		current_editor->onChar(ev);
	}
}

// ----------------------------------------------------------------------------- : Menu events

void DataEditor::onContextMenu(wxContextMenuEvent& ev) {
	if (current_editor) {
		IconMenu m;
		m.Append(ID_EDIT_CUT,	_("cut"),	_MENU_("cut"),		_HELP_("cut"));
		m.Append(ID_EDIT_COPY,	_("copy"),	_MENU_("copy"),		_HELP_("copy"));
		m.Append(ID_EDIT_PASTE, _("paste"),	_MENU_("paste"),	_HELP_("paste"));
		m.Enable(ID_EDIT_CUT,   canCut());
		m.Enable(ID_EDIT_COPY,  canCopy());
		m.Enable(ID_EDIT_PASTE, canPaste());
		if (current_editor->onContextMenu(m, ev)) {
			PopupMenu(&m);
		}
	}
}
void DataEditor::onMenu(wxCommandEvent& ev) {
	if (current_editor) {
		if (!current_editor->onCommand(ev.GetId())) {
			ev.Skip();
		}
	} else {
		ev.Skip();
	}
}

// ----------------------------------------------------------------------------- : Focus events

void DataEditor::onFocus(wxFocusEvent& ev) {
	if (current_editor) {
		current_editor->onFocus();
		onChange();
	} else {
		if (ev.GetWindow() && ev.GetWindow() == next_in_tab_order) {
			selectLast();
		} else {
			selectFirst();
		}
	}
}
void DataEditor::onLoseFocus(wxFocusEvent& ev) {
	if (current_editor) {
		current_editor->onLoseFocus();
		onChange();
	}
}

// ----------------------------------------------------------------------------- : Event table

BEGIN_EVENT_TABLE(DataEditor, CardViewer)
	EVT_LEFT_DOWN      (DataEditor::onLeftDown)
	EVT_LEFT_UP        (DataEditor::onLeftUp)
	EVT_LEFT_DCLICK    (DataEditor::onLeftDClick)
	EVT_RIGHT_DOWN     (DataEditor::onRightDown)
	EVT_MOTION         (DataEditor::onMotion)
	EVT_MOUSEWHEEL     (DataEditor::onMouseWheel)
	EVT_LEAVE_WINDOW   (DataEditor::onMouseLeave)
	EVT_CONTEXT_MENU   (DataEditor::onContextMenu)
	EVT_MENU           (wxID_ANY, DataEditor::onMenu)
	EVT_CHAR           (DataEditor::onChar)
	EVT_SET_FOCUS      (DataEditor::onFocus)
	EVT_KILL_FOCUS     (DataEditor::onLoseFocus)
	EVT_MOUSE_CAPTURE_LOST(DataEditor::onLoseCapture)
END_EVENT_TABLE  ()
