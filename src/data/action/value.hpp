//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2010 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#ifndef HEADER_DATA_ACTION_VALUE
#define HEADER_DATA_ACTION_VALUE

/** @file data/action/value.hpp
 *
 *  Actions operating on Values (and derived classes, "*Value")
 */

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/action_stack.hpp>
#include <util/defaultable.hpp>
#include <data/field.hpp>

class Card;
class StyleSheet;
class LocalFileName;
DECLARE_POINTER_TYPE(Set);
DECLARE_POINTER_TYPE(Value);
DECLARE_POINTER_TYPE(Style);
DECLARE_POINTER_TYPE(AnyValue);
typedef AnyValue TextValue;
typedef AnyValueP TextValueP;
DECLARE_POINTER_TYPE(MultipleChoiceValue);

// ----------------------------------------------------------------------------- : ValueAction (based class)

/// An Action the changes a Value
class ValueAction : public Action {
  public:
	inline ValueAction(const ValueP& value)
		: valueP(value), card(nullptr), old_time_modified(wxDateTime::Now())
	{}
	
	virtual String getName(bool to_undo) const;
	virtual void perform(bool to_undo);
	
	/// We know that the value is on the given card, add that information
	void isOnCard(Card* card);
	
	const ValueP valueP; ///< The modified value
	const Card*  card;   ///< The card the value is on, or null if it is not a card value
  private:
	wxDateTime old_time_modified;
};

// ----------------------------------------------------------------------------- : Simple

/// Action that updates a Value to a new value
ValueAction* value_action(const AnyValueP&            value, const ScriptValueP&        new_value);
ValueAction* value_action(const MultipleChoiceValueP& value, const ScriptValueP& new_value, const String& last_change);

// ----------------------------------------------------------------------------- : MultipleChoice

class MultipleChoiceValueAction : public ValueAction {
  public:
	inline MultipleChoiceValueAction(const ValueP& value, const ScriptValueP& new_value, const String& changed_choice)
		: ValueAction(value), changed_choice(changed_choice), new_value(new_value)
	{}
	
	virtual void perform(bool to_undo);
	
	const String changed_choice; ///< What choice was toggled by this action (if any)
  private:
	ScriptValueP new_value;
};

// ----------------------------------------------------------------------------- : Text

/// An action that changes a TextValue
class TextValueAction : public ValueAction {
  public:
	TextValueAction(const TextValueP& value, size_t start, size_t end, size_t new_end, const ScriptValueP& new_value, const String& name);
	
	virtual String getName(bool to_undo) const;
	virtual void perform(bool to_undo);
	virtual bool merge(const Action& action);
	
	inline String newValue() const { return new_value->toString(); }
	
	/// The modified selection
	size_t selection_start, selection_end;
  private:
	inline TextValue& value() const;
	
	size_t new_selection_end;
	ScriptValueP new_value;
	String name;
};

/// Action for toggling some formating tag on or off in some range
TextValueAction* toggle_format_action(const TextValueP& value, const String& tag, size_t start_i, size_t end_i, size_t start, size_t end, const String& action_name);

/// Typing in a TextValue, replace the selection [start...end) with replacement
/** start and end are cursor positions, start_i and end_i are indices*/
TextValueAction* typing_action(const TextValueP& value, size_t start_i, size_t end_i, size_t start, size_t end, const String& replacement, const String& action_name);

// ----------------------------------------------------------------------------- : Reminder text

/// Toggle reminder text for a keyword on or off
class TextToggleReminderAction : public ValueAction {
  public:
	TextToggleReminderAction(const TextValueP& value, size_t pos);
	
	virtual String getName(bool to_undo) const;
	virtual void perform(bool to_undo);
	
  private:	
	size_t pos;  ///< Position of "<kw-"
	bool enable; ///< Should the reminder text be turned on or off?
	Char old;    ///< Old value of the <kw- tag
};

// ----------------------------------------------------------------------------- : Replace all

/// A TextValueAction without the start and end stuff
class SimpleTextValueAction : public ValueAction {
  public:
	SimpleTextValueAction(const Card* card, const TextValueP& value, const Defaultable<String>& new_value);
	virtual void perform(bool to_undo);
	bool merge(const SimpleTextValueAction& action);
  private:
	Defaultable<String> new_value;
};

/// An action from "Replace All"; just a bunch of value actions performed in sequence
class ReplaceAllAction : public Action {
  public:
	~ReplaceAllAction();
	
	virtual String getName(bool to_undo) const;
	virtual void perform(bool to_undo);
	
	vector<SimpleTextValueAction> actions;
};

// ----------------------------------------------------------------------------- : Event

/// Notification that a script caused a value to change
class ScriptValueEvent : public Action {
  public:
	inline ScriptValueEvent(const Card* card, const Value* value) : card(card), value(value) {}
		
	virtual String getName(bool to_undo) const;
	virtual void perform(bool to_undo);
	
	const Card* card;   ///< Card the value is on
	const Value* value; ///< The modified value
};

/// Notification that a script caused a style to change
class ScriptStyleEvent : public Action {
  public:
	inline ScriptStyleEvent(const StyleSheet* stylesheet, const Style* style)
		: stylesheet(stylesheet), style(style)
	{}
	
	virtual String getName(bool to_undo) const;
	virtual void perform(bool to_undo);
	
	const StyleSheet* stylesheet; ///< StyleSheet the style is for
	const Style*      style;      ///< The modified style
};


// ----------------------------------------------------------------------------- : Action performer

/// A loose object for performing ValueActions on a certain value.
/** Used to reduce coupling */
class ValueActionPerformer {
  public:
	ValueActionPerformer(const ValueP& value, Card* card, const SetP& set);
	~ValueActionPerformer();
	/// Perform an action. The performer takes ownerwhip of the action.
	void addAction(ValueAction* action);
	
	const ValueP value; ///< The value
	Package& getLocalPackage();
  private:
	Card* card; ///< Card the value is on (if any)
	SetP  set;  ///< Set for the actions
};

// ----------------------------------------------------------------------------- : EOF
#endif
