//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <gui/value/multiple_choice.hpp>
#include <gui/thumbnail_thread.hpp>
#include <gui/util.hpp>
#include <data/action/value.hpp>

// ----------------------------------------------------------------------------- : DropDownMultipleChoiceList

/// A drop down list of color choices
class DropDownMultipleChoiceList : public DropDownChoiceListBase {
  public:
	DropDownMultipleChoiceList(Window* parent, bool is_submenu, ValueViewer& cve, ChoiceField::ChoiceP group);
	
  protected:
	virtual void   onShow();
	virtual void   select(size_t item);
	virtual size_t selection() const;
	virtual bool stayOpen(size_t selection) const { return true; }
	virtual DropDownList* createSubMenu(ChoiceField::ChoiceP group) const;
	virtual void drawIcon(DC& dc, int x, int y, size_t item, bool selected) const;
};

DropDownMultipleChoiceList::DropDownMultipleChoiceList
		(Window* parent, bool is_submenu, ValueViewer& cve, ChoiceField::ChoiceP group)
	: DropDownChoiceListBase(parent, is_submenu, cve, group)
{
	icon_size.width += 16;
}

void DropDownMultipleChoiceList::select(size_t item) {
	MultipleChoiceValueEditor& mcve = dynamic_cast<MultipleChoiceValueEditor&>(cve);
	if (isFieldDefault(item)) {
		mcve.toggleDefault();
	} else {
		ChoiceField::ChoiceP choice = getChoice(item);
		mcve.toggle(choice->first_id);
	}
	// keep the box open
	DropDownChoiceListBase::onShow(); // update 'enabled'
}

void DropDownMultipleChoiceList::drawIcon(DC& dc, int x, int y, size_t item, bool selected) const {
	// is this item active/checked?
	bool active = false;
	bool radio  = false;
	if (!isFieldDefault(item)) {
		ChoiceField::ChoiceP choice = getChoice(item);
		active = dynamic_cast<MultipleChoiceValueEditor&>(cve).active[choice->first_id];
		radio  = choice->type == CHOICE_TYPE_RADIO;
	} else {
		active = is_default(dynamic_cast<MultipleChoiceValueEditor&>(cve).value().value);
	}
	// draw checkbox
	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.SetBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
	dc.DrawRectangle(x,y,16,16);
	wxRect rect = RealRect(x+2,y+2,12,12);
	if (radio) {
		draw_radiobox(nullptr, dc, rect, active, itemEnabled(item));
	} else {
		draw_checkbox(nullptr, dc, rect, active, itemEnabled(item));
	}
	// draw icon
	DropDownChoiceListBase::drawIcon(dc, x + 16, y, item, selected);
}

void DropDownMultipleChoiceList::onShow() {
	DropDownChoiceListBase::onShow();
	// we need thumbnail images soon
	const_cast<DropDownMultipleChoiceList*>(this)->generateThumbnailImages();
}

size_t DropDownMultipleChoiceList::selection() const {
	return NO_SELECTION; // we don't know the selection
}

DropDownList* DropDownMultipleChoiceList::createSubMenu(ChoiceField::ChoiceP group) const {
	return new DropDownMultipleChoiceList(static_cast<Window*>(const_cast<DropDownMultipleChoiceList*>(this)), true, cve, group);
}

// ----------------------------------------------------------------------------- : MultipleChoiceValueEditor

IMPLEMENT_VALUE_EDITOR(MultipleChoice) {}

MultipleChoiceValueEditor::~MultipleChoiceValueEditor() {
	thumbnail_thread.abort(this);
}

DropDownList& MultipleChoiceValueEditor::initDropDown() {
	if (!drop_down) {
		drop_down.reset(new DropDownMultipleChoiceList(&editor(), false, *this, field().choices));
	}
	return *drop_down;
}

void MultipleChoiceValueEditor::determineSize(bool force_fit) {
	if (!nativeLook()) return;
	// item height
	item_height = 16;
	// height depends on number of items and item height
	int item_count = field().choices->lastId();
	style().height = item_count * item_height;
}

bool MultipleChoiceValueEditor::onLeftDown(const RealPoint& pos, wxMouseEvent& ev) {
	// find item under cursor
	if (style().render_style & RENDER_CHECKLIST) {
		// TODO: determine actual item height
		if (item_height == 0) item_height = 16;
		
		int id = (int)(pos.y / item_height);
		int end = field().choices->lastId();
		if (id >= 0 && id < end) {
			toggle(id);
			return true;
		}
	} else {
		// open a drop down menu
		return initDropDown().onMouseInParent(ev, style().popup_style == POPUP_DROPDOWN_IN_PLACE && !nativeLook());
	}
	return false;
}
bool MultipleChoiceValueEditor::onChar(wxKeyEvent& ev) {
	if (style().render_style & RENDER_CHECKLIST) {
		// todo;
		return false;
	} else {
		return initDropDown().onCharInParent(ev);
	}
}
void MultipleChoiceValueEditor::onLoseFocus() {
	if (drop_down) drop_down->hide(false);
}

void MultipleChoiceValueEditor::onValueChange() {
	MultipleChoiceValueViewer::onValueChange();
	// determine active values
	active.clear();
	vector<String> selected;
	value().get(selected);
	vector<String>::iterator select_it = selected.begin();
	// for each choice...
	int end = field().choices->lastId();
	for (int i = 0 ; i < end ; ++i) {
		String choice = field().choices->choiceName(i);
		bool is_active = select_it != selected.end() && *select_it == choice;
		if (is_active) select_it++;
		active.push_back(is_active);
	}
}

void MultipleChoiceValueEditor::toggle(int id) {
	String new_value;
	String toggled_choice;
	// old selection
	vector<String> selected;
	value().get(selected);
	vector<String>::iterator select_it = selected.begin();
	// copy selected choices to new value
	int end = field().choices->lastId();
	for (int i = 0 ; i < end ; ++i) {
		String choice = field().choices->choiceName(i);
		bool active = select_it != selected.end() && *select_it == choice;
		if (active) select_it++;
		if (active != (i == id)) {
			if (!new_value.empty()) new_value += _(", ");
			new_value += choice;
		}
		if (i == id) toggled_choice = choice;
	}
	// store value
	addAction(value_action(valueP(), to_script(new_value), toggled_choice));
}

void MultipleChoiceValueEditor::toggleDefault() {
	if (ScriptDefault const* dv = is_default(value().value)) {
		addAction(value_action(valueP(), dv->un_default));
	} else {
		addAction(value_action(valueP(), make_default(value().value)));
	}
}
