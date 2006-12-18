//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2006 Twan van Laarhoven                           |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <gui/value/text.hpp>
#include <data/action/value.hpp>
#include <util/tagged_string.hpp>
#include <util/window_id.hpp>
#include <wx/clipbrd.h>
#include <wx/caret.h>

// ----------------------------------------------------------------------------- : TextValueEditorScrollBar

/// A scrollbar to scroll a TextValueEditor
/** implemented as the scrollbar of a Window because that functions better */
class TextValueEditorScrollBar : public wxWindow {
  public:
	TextValueEditorScrollBar(TextValueEditor& tve);
  private:
	DECLARE_EVENT_TABLE();
	TextValueEditor& tve;
	
	void onScroll(wxScrollWinEvent&);
	void onMotion(wxMouseEvent&);
};


TextValueEditorScrollBar::TextValueEditorScrollBar(TextValueEditor& te)
	: wxWindow(&tve.editor(), wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNO_BORDER | wxVSCROLL | wxALWAYS_SHOW_SB)
	, tve(te)
{}

void TextValueEditorScrollBar::onScroll(wxScrollWinEvent& ev) {
	if (ev.GetOrientation() == wxVERTICAL) {
		tve.scrollTo(ev.GetPosition());
	}
}
void TextValueEditorScrollBar::onMotion(wxMouseEvent& ev) {
	tve.editor().SetCursor(*wxSTANDARD_CURSOR);
	ev.Skip();
}

BEGIN_EVENT_TABLE(TextValueEditorScrollBar, wxEvtHandler)
	EVT_SCROLLWIN    (TextValueEditorScrollBar::onScroll)
	EVT_MOTION       (TextValueEditorScrollBar::onMotion)
END_EVENT_TABLE  ()



// ----------------------------------------------------------------------------- : TextValueEditor

IMPLEMENT_VALUE_EDITOR(Text)
	, selection_start(0), selection_end(0)
	, select_words(false)
	, scrollbar(nullptr)
{}

TextValueEditor::~TextValueEditor() {
	delete scrollbar;
}

// ----------------------------------------------------------------------------- : Mouse

void TextValueEditor::onLeftDown(const RealPoint& pos, wxMouseEvent& ev) {
	select_words = false;
	moveSelection(v.indexAt(style().getRotation().trInv(pos)), !ev.ShiftDown(), MOVE_MID);
}
void TextValueEditor::onLeftUp(const RealPoint& pos, wxMouseEvent&) {
	// TODO: lookup position of click?
}

void TextValueEditor::onMotion(const RealPoint& pos, wxMouseEvent& ev) {
	if (ev.LeftIsDown()) {
		size_t index = v.indexAt(style().getRotation().trInv(pos));
		if (select_words) {
			// TODO: on the left, swap start and end
			moveSelection(index < selection_start ? prevWordBoundry(index) : nextWordBoundry(index), false, MOVE_MID);
		} else {
			moveSelection(index, false, MOVE_MID);
		}
	}
}

void TextValueEditor::onLeftDClick(const RealPoint& pos, wxMouseEvent& ev) {
	select_words = true;
	size_t index = v.indexAt(style().getRotation().trInv(pos));
	moveSelection(prevWordBoundry(index), true, MOVE_MID);
	moveSelection(nextWordBoundry(index), false, MOVE_MID);
}

void TextValueEditor::onRightDown(const RealPoint& pos, wxMouseEvent& ev) {
	size_t index = v.indexAt(style().getRotation().trInv(pos));
	if (index < min(selection_start, selection_end) ||
		index > max(selection_start, selection_end)) {
		// only move cursor when outside selection
		moveSelection(index, !ev.ShiftDown(), MOVE_MID);
	}
}

// ----------------------------------------------------------------------------- : Keyboard

void TextValueEditor::onChar(wxKeyEvent& ev) {
	fixSelection();
	switch (ev.GetKeyCode()) {
		case WXK_LEFT:
			// move left (selection?)
			if (ev.ControlDown()) {
				moveSelection(prevWordBoundry(selection_end),!ev.ShiftDown(), MOVE_LEFT);
			} else {
				moveSelection(prevCharBoundry(selection_end),!ev.ShiftDown(), MOVE_LEFT);
			}
			break;
		case WXK_RIGHT:
			// move left (selection?)
			if (ev.ControlDown()) {
				moveSelection(nextWordBoundry(selection_end),!ev.ShiftDown(), MOVE_RIGHT);
			} else {
				moveSelection(nextCharBoundry(selection_end),!ev.ShiftDown(), MOVE_RIGHT);
			}
			break;
		case WXK_UP:
			moveSelection(v.moveLine(selection_end, -1), !ev.ShiftDown(), MOVE_LEFT);
			break;
		case WXK_DOWN:
			moveSelection(v.moveLine(selection_end, +1), !ev.ShiftDown(), MOVE_RIGHT);
			break;
		case WXK_HOME:
			// move to begining of line / all (if control)
			if (ev.ControlDown()) {
				moveSelection(0,                          !ev.ShiftDown(), MOVE_LEFT);
			} else {
				moveSelection(v.lineStart(selection_end), !ev.ShiftDown(), MOVE_LEFT);
			}
			break;
		case WXK_END:
			// move to end of line / all (if control)
			if (ev.ControlDown()) {
				moveSelection(value().value().size(),   !ev.ShiftDown(), MOVE_RIGHT);
			} else {
				moveSelection(v.lineEnd(selection_end), !ev.ShiftDown(), MOVE_RIGHT);
			}
			break;
		case WXK_BACK:
			if (selection_start == selection_end) {
				// if no selection, select previous character
				moveSelectionNoRedraw(prevCharBoundry(selection_end), false);
				if (selection_start == selection_end) {
					// Walk over a <sep> as if we are the LEFT key
					moveSelection(prevCharBoundry(selection_end), true, MOVE_LEFT);
					return;
				}
			}
			replaceSelection(wxEmptyString, _("Backspace"));
			break;
		case WXK_DELETE:
			if (selection_start == selection_end) {
				// if no selection select next
				moveSelectionNoRedraw(nextCharBoundry(selection_end), false);
				if (selection_start == selection_end) {
					// Walk over a <sep> as if we are the RIGHT key
					moveSelection(nextCharBoundry(selection_end), true, MOVE_RIGHT);
				}
			}
			replaceSelection(wxEmptyString, _("Delete"));
			break;
		case WXK_RETURN:
			if (field().multi_line) {
				replaceSelection(_("\n"), _("Enter"));
			}
			break;
		default:
			if (ev.GetKeyCode() >= _(' ') && ev.GetKeyCode() == (int)ev.GetRawKeyCode()) {
				// TODO: Find a more correct way to determine normal characters,
				//       this might not work for internationalized input.
				//       It might also not be portable!
				replaceSelection(String(ev.GetUnicodeKey(), 1), _("Typing"));
			}
	}
}

// ----------------------------------------------------------------------------- : Other events

void TextValueEditor::onFocus() {
	showCaret();
}
void TextValueEditor::onLoseFocus() {
	// hide caret
	wxCaret* caret = editor().GetCaret();
	assert(caret);
	if (caret->IsVisible()) caret->Hide();
	// hide selection
	selection_start = selection_end = 0;
}

bool TextValueEditor::onContextMenu(wxMenu& m, wxContextMenuEvent& ev) {
	// in a keword? => "reminder text" option
	size_t kwpos = in_tag(value().value(), _("<kw-"), selection_start, selection_start);
	if (kwpos != String::npos) {
		Char c = String(value().value()).GetChar(kwpos + 4);
		m.AppendSeparator();
		m.AppendCheckItem(ID_FORMAT_REMINDER, _("&Reminder text"), _("Show or hide reminder text for this keyword"));
		m.Check(ID_FORMAT_REMINDER, c == _('1') || c == _('A')); // reminder text currently shown
	}
	// always show the menu
	return true;
}
void TextValueEditor::onMenu(wxCommandEvent& ev) {
	if (ev.GetId() == ID_FORMAT_REMINDER) {
		// toggle reminder text
		size_t kwpos = in_tag(value().value(), _("<kw-"), selection_start, selection_start);
		if (kwpos != String::npos) {
//			getSet().actions.add(new TextToggleReminderAction(value, kwpos));
		}
	} else {
		ev.Skip();
	}
}

// ----------------------------------------------------------------------------- : Other overrides

void TextValueEditor::draw(RotatedDC& dc) {
	TextValueViewer::draw(dc);
	if (isCurrent()) {
		v.drawSelection(dc, style(), selection_start, selection_end);
		
		// show caret, onAction() would be a better place
		// but it has to be done after the viewer has updated the TextViewer
		// we could do that ourselfs, but we need a dc for that
		fixSelection();
		showCaret();
	}
	// DEBUG, TODO: REMOVEME
	Rotater r(dc, style().getRotation());
	/*dc.SetPen(*wxRED_PEN);
	dc.SetBrush(*wxTRANSPARENT_BRUSH);
	dc.SetTextForeground(*wxGREEN);
	dc.SetFont(wxFont(6,wxFONTFAMILY_SWISS,wxNORMAL,wxNORMAL));
	for (size_t i = 0 ; i < value().value().size() ; i += 10) {
		RealRect r = v.charRect(i);
		r.width = max(r.width,1.);
		dc.DrawRectangle(r);
		dc.DrawText(String()<<(int)i, r.position()+RealSize(1,5));
	}*/
	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.SetBrush(*wxWHITE_BRUSH);
	dc.SetTextForeground(*wxBLUE);
	dc.SetFont(wxFont(6,wxFONTFAMILY_SWISS,wxNORMAL,wxNORMAL));
	dc.DrawRectangle(RealRect(style().width-50,style().height-10,50,10));
	dc.DrawText(String::Format(_("%d - %d"),selection_start, selection_end), RealPoint(style().width-50,style().height-10));
}

wxCursor rotated_ibeam;

wxCursor TextValueEditor::cursor() const {
	if (viewer.getRotation().sideways() ^ style().getRotation().sideways()) { // 90 or 270 degrees
		if (!rotated_ibeam.Ok()) {
			rotated_ibeam = wxCursor(_("CUR_ROT_IBEAM"));
		}
		return rotated_ibeam;
	} else {
		return wxCURSOR_IBEAM;
	}
}

void TextValueEditor::onValueChange() {
	TextValueViewer::onValueChange();
	selection_start = 0;
	selection_end   = 0;
}

void TextValueEditor::onAction(const ValueAction& action, bool undone) {
	TextValueViewer::onAction(action, undone);
	TYPE_CASE(action, TextValueAction) {
		selection_start = action.selection_start;
		selection_end   = action.selection_end;
	}
}

// ----------------------------------------------------------------------------- : Clipboard

bool TextValueEditor::canPaste() const {
	return wxTheClipboard->IsSupported(wxDF_TEXT);
}

bool TextValueEditor::canCopy() const {
	return selection_start != selection_end; // text is selected
}

bool TextValueEditor::doPaste() {
	// get data
	if (!wxTheClipboard->Open()) return false;
	wxTextDataObject data;
	bool ok = wxTheClipboard->GetData(data);
	wxTheClipboard->Close();
	if (!ok) return false;
	// paste
	replaceSelection(escape(data.GetText()), _("Paste"));
	return true;
}

bool TextValueEditor::doCopy() {
	// determine string to store
	if (selection_start > value().value().size()) selection_start = value().value().size();
	if (selection_end   > value().value().size()) selection_end   = value().value().size();
	size_t start = min(selection_start, selection_end);
	size_t end   = max(selection_start, selection_end);
	String str = untag(value().value().substr(start, end - start));
	if (str.empty()) return false; // no data to copy
	// set data
	if (!wxTheClipboard->Open()) return false;
	bool ok = wxTheClipboard->SetData(new wxTextDataObject(str));
	wxTheClipboard->Close();
	return ok;
}

bool TextValueEditor::doDelete() {
	replaceSelection(wxEmptyString, _("Cut"));
	return true;
}

// ----------------------------------------------------------------------------- : Formatting

bool TextValueEditor::canFormat(int type) const {
	switch (type) {
		case ID_FORMAT_BOLD: case ID_FORMAT_ITALIC:
			return !style().always_symbol && style().allow_formating;
		case ID_FORMAT_SYMBOL:
			return !style().always_symbol && style().allow_formating && style().symbol_font.valid();
		case ID_FORMAT_REMINDER:
			return false; // TODO
		default:
			return false;
	}
}

bool TextValueEditor::hasFormat(int type) const {
	switch (type) {
		case ID_FORMAT_BOLD:
			return in_tag(value().value(), _("<b"),   selection_start, selection_end) != String::npos;
		case ID_FORMAT_ITALIC:
			return in_tag(value().value(), _("<i"),   selection_start, selection_end) != String::npos;
		case ID_FORMAT_SYMBOL:
			return in_tag(value().value(), _("<sym"), selection_start, selection_end) != String::npos;
		case ID_FORMAT_REMINDER:
			return false; // TODO
		default:
			return false;
	}
}

void TextValueEditor::doFormat(int type) {
	switch (type) {
		case ID_FORMAT_BOLD: {
			getSet().actions.add(toggle_format_action(valueP(), _("b"),   selection_start, selection_end, _("Bold")));
			break;
		}
		case ID_FORMAT_ITALIC: {
			getSet().actions.add(toggle_format_action(valueP(), _("i"),   selection_start, selection_end, _("Italic")));
			break;
		}
		case ID_FORMAT_SYMBOL: {
			getSet().actions.add(toggle_format_action(valueP(), _("sym"), selection_start, selection_end, _("Symbols")));
			break;
		}
	}
}

// ----------------------------------------------------------------------------- : Selection

void TextValueEditor::showCaret() {
	// Rotation
	Rotation rot(viewer.getRotation());
	Rotater rot2(rot, style().getRotation());
	// The caret
	wxCaret* caret = editor().GetCaret();
	// cursor rectangle
	RealRect cursor = v.charRect(selection_end);
	cursor.width = 0;
	// height may be 0 near a <line>
	// it is not 0 for empty text, because TextRenderer handles that case
	if (cursor.height == 0) {
		if (style().always_symbol && style().symbol_font.valid()) {
			RealSize s = style().symbol_font.font->defaultSymbolSize(viewer.getContext(), rot.trS(style().symbol_font.size));
			cursor.height = s.height;
		} else {
			cursor.height = v.heightOfLastLine();
			if (cursor.height == 0) {
				wxClientDC dc(&editor());
				// TODO : high quality?
				dc.SetFont(style().font.font);
				int hi;
				dc.GetTextExtent(_(" "), 0, &hi);
				cursor.height = rot.trS(hi);
			}
		}
	}
	// clip caret pos and size; show caret
	if (nativeLook()) {
		if (cursor.y + cursor.height <= 0 || cursor.y >= style().height) {
			// caret should be hidden
			if (caret->IsVisible()) caret->Hide();
			return;
		} else if (cursor.y < 0) {
			// caret partially hidden, clip
			cursor.height -= -cursor.y;
			cursor.y = 0;
		} else if (cursor.y + cursor.height >= style().height) {
			// caret partially hidden, clip
			cursor.height = style().height - cursor.y;
		}
	}
	// rotate
	cursor = rot.tr(cursor);
	// set size
	wxSize size = cursor.size();
	size.SetWidth (max(1, size.GetWidth()));
	size.SetHeight(max(1, size.GetHeight()));
	// resize, move, show
	if (size != caret->GetSize()) {
		caret->SetSize(size);
	}
	caret->Move(cursor.position());
	if (!caret->IsVisible()) caret->Show();
}

void TextValueEditor::replaceSelection(const String& replacement, const String& name) {
	if (replacement.empty() && selection_start == selection_end) {
		// no text selected, nothing to delete
		return;
	}
	// fix the selection, it may be changed by undo/redo
	if (selection_end < selection_start) swap(selection_end, selection_start);
	fixSelection();
	// execute the action before adding it to the stack,
	// because we want to run scripts before action listeners see the action
	ValueAction* action = typing_action(valueP(), selection_start, selection_end, replacement, name);
	if (!action) {
		// nothing changed, but move the selection anyway
		moveSelection(selection_start);
		return;
	}
	// perform the action
	// NOTE: this calls our onAction, invalidating the text viewer and moving the selection around the new text
	getSet().actions.add(action);
	// move cursor
	if (field().move_cursor_with_sort && replacement.size() == 1) {
		String val = value().value();
		Char typed  = replacement.GetChar(0);
		Char typedU = toUpper(typed);
		Char cur    = val.GetChar(selection_start);
		// the cursor may have moved because of sorting...
		// is 'replacement' just after the current cursor?
		if (selection_start >= 0 && selection_start < val.size() && (cur == typed || cur == typedU)) {
			// no need to move cursor in a special way
			selection_end = selection_start = min(selection_end, selection_start) + 1;
		} else {
			// find the last occurence of 'replacement' in the value
			size_t pos = val.find_last_of(typed);
			if (pos == String::npos) {
				// try upper case
				pos = val.find_last_of(typedU);
			}
			if (pos != String::npos) {
				selection_end = selection_start = pos + 1;
			} else {
				selection_end = selection_start;
			}
		}
	} else {
		selection_end = selection_start = min(selection_end, selection_start) + replacement.size();
	}
	// scroll with next update
//	scrollWithCursor = true;
}

void TextValueEditor::moveSelection(size_t new_end, bool also_move_start, Movement dir) {
	if (!isCurrent()) {
		// selection is only visible for curent editor, we can do a move the simple way
		moveSelectionNoRedraw(new_end, also_move_start, dir);
		return;
	}
	// Hide caret
	wxCaret* caret = editor().GetCaret();
	if (caret->IsVisible()) caret->Hide();
	// Move selection
	shared_ptr<DC> dc = editor().overdrawDC();
	RotatedDC rdc(*dc, viewer.getRotation(), false);
	if (nativeLook()) {
		// clip the dc to the region of this control
		rdc.SetClippingRegion(style().getRect());
	}
	// clear old selection by drawing it again
	v.drawSelection(rdc, style(), selection_start, selection_end);
	// move
	moveSelectionNoRedraw(new_end, also_move_start, dir);
	// scroll?
//	scrollWithCursor = true;
//	if (onMove()) {
//		// we can't redraw just the selection because we must scroll
//		updateScrollbar();
//		editor.refreshEditor();
//	} else {
		// draw new selection
		v.drawSelection(rdc, style(), selection_start, selection_end);
//	}
	showCaret();
	// TODO; DEBUG!!
	Rotater r(rdc, style().getRotation());
	rdc.SetPen(*wxTRANSPARENT_PEN);
	rdc.SetBrush(*wxWHITE_BRUSH);
	rdc.SetTextForeground(*wxBLUE);
	rdc.SetFont(wxFont(6,wxFONTFAMILY_SWISS,wxNORMAL,wxNORMAL));
	rdc.DrawRectangle(RealRect(style().width-50,style().height-10,50,10));
	rdc.DrawText(String::Format(_("%d - %d"),selection_start, selection_end), RealPoint(style().width-50,style().height-10));
}

void TextValueEditor::moveSelectionNoRedraw(size_t new_end, bool also_move_start, Movement dir) {
	selection_end = new_end;
	if (also_move_start) selection_start = selection_end;
	fixSelection(dir);
}

void TextValueEditor::fixSelection(Movement dir) {
	const String& val = value().value();
	// value may have become smaller because of undo/redo
	// make sure the selection stays inside the text
	size_t size = val.size();
	selection_end   = min(size, selection_end);
	selection_start = min(size, selection_start);
	// start and end must be on the same side of separators
	size_t seppos = val.find(_("<sep"));
	while (seppos != String::npos) {
		size_t sepend = match_close_tag(val, seppos);
		if (selection_start <= seppos && selection_end > seppos) selection_end = seppos; // not on same side
		if (selection_start >= sepend && selection_end < sepend) selection_end = sepend; // not on same side
		if (selection_start > seppos && selection_start < sepend) {
			// start inside separator
			selection_start = move(selection_start, seppos, sepend, dir);
		}
		if (selection_end > seppos && selection_end < sepend) {
			// end inside separator
			selection_end = selection_start < sepend ? seppos : sepend;
		}
		// find next separator
		seppos = val.find(_("<sep"), seppos + 1);
	}
	// start or end in an <atom>? if so, move them out
	size_t atompos = val.find(_("<atom"));
	while (atompos != String::npos) {
		size_t atomend = match_close_tag(val, atompos);
		if (selection_start > atompos && selection_start < atomend) { // start inside atom
			selection_start = move(selection_start, atompos, atomend, dir);
		}
		if (selection_end   > atompos && selection_end   < atomend) { // end inside atom
			selection_end   = move(selection_end,   atompos, atomend, dir);
		}
		// find next atom
		atompos = val.find(_("<atom"), atompos + 1);
	}
	// start and end must not be inside or between tags
	selection_start = v.firstVisibleChar(selection_start, dir == MOVE_LEFT ? -1 : +1);
	selection_end   = v.firstVisibleChar(selection_end,   dir == MOVE_LEFT ? -1 : +1);
	//  TODO
}


size_t TextValueEditor::prevCharBoundry(size_t pos) const {
	return max(0, (int)pos - 1);
}
size_t TextValueEditor::nextCharBoundry(size_t pos) const {
	return min(value().value().size(), pos + 1);
}
size_t TextValueEditor::prevWordBoundry(size_t pos) const {
	const String& val = value().value();
	size_t p = val.find_last_not_of(_(" ,.:;()\n"), max(0, (int)(pos - 1))); //note: pos-1 might be < 0
	if (p == String::npos) return 0;
	p = val.find_last_of(_(" ,.:;()\n"), p);
	if (p == String::npos) return 0;
	return p + 1;
}
size_t TextValueEditor::nextWordBoundry(size_t pos) const {
	const String& val = value().value();
	size_t p = val.find_first_of(_(" ,.:;()\n"), pos);
	if (p == String::npos) return val.size();
	p = val.find_first_not_of(_(" ,.:;()\n"), p);
	if (p == String::npos) return val.size();
	return p;
}

void TextValueEditor::select(size_t start, size_t end) {
	selection_start = start;
	selection_end   = end;
	// TODO : redraw?
}

size_t TextValueEditor::move(size_t pos, size_t start, size_t end, Movement dir) {
	if (dir == MOVE_LEFT)      return start;
	if (dir == MOVE_RIGHT)     return end;
	if (pos * 2 > start + end) return end; // past the middle
	else                       return start;
}

// ----------------------------------------------------------------------------- : Native look / scrollbar

void TextValueEditor::determineSize() {
	if (!nativeLook()) return;
	style().angle = 0; // no rotation in nativeLook
	if (scrollbar) {
		// muliline, determine scrollbar size
		style().height = 100;
		int sbw = wxSystemSettings::GetMetric(wxSYS_VSCROLL_X);
		scrollbar->SetSize(
			style().left + style().width - sbw + 1,
			style().top - 1,
			sbw,
			style().height + 2);
//		r.reset();
	} else {
		// Height depends on font
		wxMemoryDC dc;
		Bitmap bmp(1,1);
		dc.SelectObject(bmp);
		dc.SetFont(style().font.font);
		style().height = dc.GetCharHeight() + 2;
	}
}

void TextValueEditor::onShow(bool showing) {
	if (scrollbar) {
		// show/hide our scrollbar
		scrollbar->Show(showing);
	}
}

void TextValueEditor::onMouseWheel(const RealPoint& pos, wxMouseEvent& ev) {
	if (scrollbar) {
		int toScroll = ev.GetWheelRotation() * ev.GetLinesPerAction() / ev.GetWheelDelta(); // note: up is positive
		int target = min(max(scrollbar->GetScrollPos(wxVERTICAL) - toScroll, 0),
			             scrollbar->GetScrollRange(wxVERTICAL) - scrollbar->GetScrollThumb(wxVERTICAL));
		scrollTo(target);
	}
}

void TextValueEditor::scrollTo(int pos) {
	// scroll
//	r.scrollTo(pos);
	// move the cursor if needed
	// refresh
//	editor.refreshEditor();
}