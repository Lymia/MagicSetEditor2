//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <script/functions/functions.hpp>
#include <script/functions/util.hpp>
#include <util/tagged_string.hpp>
#include <data/set.hpp>
#include <data/game.hpp>
#include <data/card.hpp>
#include <data/stylesheet.hpp>
#include <data/field/text.hpp>
#include <data/field/choice.hpp>
#include <data/field/multiple_choice.hpp>
#include <data/action/value.hpp>

DECLARE_TYPEOF_COLLECTION(FieldP);
DECLARE_TYPEOF_COLLECTION(TextValue*);
DECLARE_TYPEOF_COLLECTION(String);
DECLARE_TYPEOF_COLLECTION(pair<String COMMA bool>);
DECLARE_TYPEOF_COLLECTION(ChoiceField::ChoiceP);

// ----------------------------------------------------------------------------- : Combined editor

// Combining multiple (text) values into a single one
// The combined value is  value1 <sep>something</sep> value2 <sep>something</sep> value3
// 

SCRIPT_FUNCTION_WITH_DEP(combined_editor) {
	// read 'field#' arguments
	vector<TextValue*> values;
	for (int i = 0 ; ; ++i) {
		String name = _("field"); if (i > 0) name = name << i;
		SCRIPT_OPTIONAL_PARAM_N(ValueP, name, value) {
			TextValue* text_value = dynamic_cast<TextValue*>(value.get());
			if (!text_value) throw ScriptError(_("Argument '")+name+_("' should be a text field")); 
			values.push_back(text_value);
		} else if (i > 0) break;
	}
	if (values.empty()) {
		throw ScriptError(_("No fields specified for combined_editor"));
	}
	// read 'separator#' arguments
	vector<String> separators;
	for (int i = 0 ; ; ++i) {
		String name = _("separator"); if (i > 0) name = name << i;
		SCRIPT_OPTIONAL_PARAM_N(String, name, separator) {
			separators.push_back(separator);
		} else if (i > 0) break;
	}
	if (separators.size() < values.size() - 1) {
		throw ScriptError(String::Format(_("Not enough separators for combine_editor, expected %d"), values.size()-1));
	}
	// the value
	SCRIPT_PARAM_C(String, value);
	// remove suffix/prefix
	SCRIPT_OPTIONAL_PARAM_(String, prefix);
	SCRIPT_OPTIONAL_PARAM_(String, suffix);
	if (is_substr(value,0,_("<prefix"))) {
		value = value.substr(min(value.size(), match_close_tag_end(value, 0)));
	}
	size_t pos = value.rfind(_("<suffix"));
	if (pos != String::npos && match_close_tag_end(value,pos) >= value.size()) {
		value = value.substr(0, pos);
	}
	// split the value
	vector<pair<String,bool> > value_parts; // (value part, is empty)
	pos = value.find(_("<sep"));
	while (pos != String::npos) {
		String part = value.substr(0, pos);
		value_parts.push_back(make_pair(part, false));
		value = value.substr(min(match_close_tag_end(value,pos), value.size()));
		pos = value.find(_("<sep"));
	}
	value_parts.push_back(make_pair(value, false));
	value_parts.resize(values.size()); // TODO: what if there are more value_parts than values?
	// update the values if our input value is newer?
	Age new_value_update = last_update_age();
	FOR_EACH_2(v, values, nv, value_parts) {
		if (v->last_modified < new_value_update) {
			bool changed = v->value->toString() != nv.first;
			if (changed) v->value = to_script(nv.first);
			v->last_modified = new_value_update;
			changed |= v->update(ctx);
			if (changed) { // notify of change
				SCRIPT_OPTIONAL_PARAM_(CardP, card);
				SCRIPT_PARAM(Set*, set);
				ScriptValueEvent change(card.get(), v);
				set->actions.tellListeners(change, false);
			}
		}
		nv.first = v->value->toString();
		nv.second = index_to_untagged(nv.first, nv.first.size()) == 0;
	}
	// options
	SCRIPT_PARAM_DEFAULT(bool, hide_when_empty,   false);
	SCRIPT_PARAM_DEFAULT(bool, soft_before_empty, false);
	// recombine the parts
	String new_value = value_parts.front().first;
	bool   new_value_empty = value_parts.front().second;
	size_t size_before_last = 0;
	for (size_t i = 1 ; i < value_parts.size() ; ++i) {
		size_before_last = new_value.size();
		if (value_parts[i].second && new_value_empty && hide_when_empty) {
			// no separator
		} else if (value_parts[i].second && soft_before_empty) {
			// soft separator
			new_value += _("<sep-soft>") + separators[i - 1] + _("</sep-soft>");
			new_value_empty = false;
		} else {
			// normal separator
			new_value += _("<sep>")      + separators[i - 1] + _("</sep>");
			new_value_empty = false;
		}
		new_value += value_parts[i].first;
	}
	if (!new_value_empty || !hide_when_empty) {
		if (!suffix.empty()) {
			if (is_substr(new_value, size_before_last, _("<sep-soft>")) && value_parts.size() >= 2) {
				// If the value ends in a soft separator, we have this situation:
				//   [blah]<sep-soft>ABC</sep-soft><suffix>XYZ</suffix>
				// This renderes as:
				//   [blah]   XYZ
				// Which looks bad, so instead change the text to
				//   [blah]<sep>XYZ<soft>ABC</soft></sep>
				// Which might be slightly incorrect, but soft text doesn't matter anyway.
				size_t after = min(new_value.size(), match_close_tag_end(new_value, size_before_last));
				new_value = new_value.substr(0, size_before_last)
				          + _("<sep>")
				          + suffix
				          + _("<soft>")
				          + separators[value_parts.size() - 2]
				          + _("</soft></sep>")
				          + new_value.substr(after);
			} else {
				new_value += _("<suffix>") + suffix + _("</suffix>");
			}
		}
		if (!prefix.empty()) {
			new_value = _("<prefix>") + prefix + _("</prefix>") + new_value;
		}
	}
	SCRIPT_RETURN(new_value);
}

SCRIPT_FUNCTION_DEPENDENCIES(combined_editor) {
	// read 'field#' arguments
	vector<FieldP> fields;
	for (int i = 0 ; ; ++i) {
		String name = _("field"); if (i > 0) name = name << i;
		SCRIPT_OPTIONAL_PARAM_N(ValueP, name, value) {
			fields.push_back(value->fieldP);
		} else if (i > 0) break;
	}
	// Find the target field
	SCRIPT_PARAM_C(Set*, set);
	GameP game = set->game;
	FieldP target_field;
	if      (dep.type == DEP_CARD_FIELD) target_field = game->card_fields[dep.index];
	else if (dep.type == DEP_SET_FIELD)  target_field = game->set_fields[dep.index];
	else if (dep.type == DEP_EXTRA_CARD_FIELD) {
		SCRIPT_PARAM_C(StyleSheetP, stylesheet);
		target_field = stylesheet->extra_card_fields[dep.index];
	}
	else                                 throw InternalError(_("Finding dependencies of combined error for non card/set field"));
	// Add dependencies, from target_field on field#
	// For card fields
	size_t j = 0;
	FOR_EACH(f, game->card_fields) {
		Dependency dep(DEP_CARD_COPY_DEP, j++);
		FOR_EACH(fn, fields) {
			if (f == fn) {
				target_field->dependent_scripts.add(dep);
				break;
			}
		}
	}
	// For set fields
	j = 0;
	FOR_EACH(f, game->set_fields) {
		Dependency dep(DEP_SET_COPY_DEP, j++);
		FOR_EACH(fn, fields) {
			if (f == fn) {
				target_field->dependent_scripts.add(dep);
				break;
			}
		}
	}
	return dependency_dummy;
}

// ----------------------------------------------------------------------------- : Choice values

// convert a full choice name into the name of the top level group it is in
SCRIPT_FUNCTION(primary_choice) {
	SCRIPT_PARAM_C(ValueP,input);
	ChoiceValueP value = dynamic_pointer_cast<ChoiceValue>(input);
	if (!value) {
		throw ScriptError(_("Argument to 'primary_choice' should be a choice value")); 
	}
	ChoiceFieldP field = dynamic_pointer_cast<ChoiceField>(value->fieldP);
	if (!field) {
		throw ScriptError(_("Argument to 'primary_choice' should be a choice value")); 
	}
	// determine choice
	int id = field->choices->choiceId(value->value->toString());
	// find the last group that still contains id
	const vector<ChoiceField::ChoiceP>& choices = field->choices->choices;
	FOR_EACH_CONST_REVERSE(c, choices) {
		if (id >= c->first_id) {
			SCRIPT_RETURN(c->name);
		}
	}
	SCRIPT_RETURN(_(""));
}

// ----------------------------------------------------------------------------- : Multiple choice values

/// Iterate over a comma separated list
bool next_choice(size_t& start, size_t& end, const String& input) {
	if (start >= input.size()) return false;
	// ingore whitespace
	while (start < input.size() && input.GetChar(start) == _(' ')) ++start;
	// find end
	end = input.find_first_of(_(','), start);
	if (end == String::npos) end = input.size();
	return true;
}

/// Is the given choice active?
bool chosen(const String& choice, const String& input) {
	for (size_t start = 0, end = 0 ; next_choice(start,end,input) ; start = end+1 ) {
		// does this choice match the one asked about?
		if (end - start == choice.size() && is_substr(input, start, choice)) {
			return true;
		}
	}
	return false;
}

// is the given choice active?
SCRIPT_FUNCTION(chosen) {
	SCRIPT_PARAM_C(String,choice);
	SCRIPT_PARAM_C(String,input);
	SCRIPT_RETURN(chosen(choice, input));
}

/// Filter the choices
/** Keep at most max elements of choices,
 *  and at least min. Use prefered as choice to add/keep in case of conflicts.
 */
String filter_choices(const String& input, const vector<String>& choices, int min, int max, String prefered) {
	if (choices.empty()) return input; // do nothing, shouldn't happen, but better make sure
	String ret;
	int count = 0;
	vector<bool> seen(choices.size()); // which choices have we seen in input?
	// walk over the input
	for (size_t start = 0, end = 0 ; next_choice(start,end,input) ; start = end +1) {
		// is this choice in choices
		bool in_choices = false;
		for (size_t i = 0 ; i < choices.size() ; ++i) {
			if (!seen[i] && is_substr(input, start, choices[i])) {
				seen[i] = true; ++count;
				in_choices = true;
				break;
			}
		}
		// is this choice unaffected?
		if (!in_choices) {
			if (!ret.empty()) ret += _(", ");
			ret += input.substr(start, end - start);
		}
	}
	// keep more choices
	if (count < min) {
		// add prefered choice
		for (size_t i = 0 ; i < choices.size() ; ++i) {
			if (choices[i] == prefered) {
				if (!seen[i]) {
					seen[i] = true; ++count;
				}
				break;
			}
		}
		// add more choices
		for (size_t i = 0 ; i < choices.size() ; ++i) {
			if (count >= min) break;
			if (!seen[i]) {
				seen[i] = true; ++count;
			}
		}
	}
	// keep less choices
	if (count > max) {
		for (size_t i = choices.size() - 1 ; i >= 0 ; --i) {
			if (count <= max) break;
			if (seen[i]) {
				if (max > 0 && choices[i] == prefered) continue; // we would rather not remove prefered choice
				seen[i] = false; --count;
			}
		}
	}
	// add the 'seen' choices to ret
	for (size_t i = 0 ; i < choices.size() ; ++i) {
		if (seen[i]) {
			if (!ret.empty()) ret += _(", ");
			ret += choices[i];
		}
	}
	return ret;
}

// read 'choice#' arguments
void read_choices_param(Context& ctx, vector<String>& choices_out) {
	SCRIPT_OPTIONAL_PARAM_C(String,choices) {
		for (size_t start = 0, end = 0 ; next_choice(start,end,choices) ; start = end + 1) {
			choices_out.push_back(choices.substr(start,end-start));
		}
	} else {
		// old style: separate arguments
		SCRIPT_OPTIONAL_PARAM_C(String,choice) {
			choices_out.push_back(choice);
		} else {
			for (int i = 1 ; ; ++i) {
				String name = _("choice");
				SCRIPT_OPTIONAL_PARAM_N(String, name << i, choice) {
					choices_out.push_back(choice);
				} else break;
			}
		}
	}
}

// add the given choice if it is not already active
SCRIPT_FUNCTION(require_choice) {
	SCRIPT_PARAM_C(String,input);
	SCRIPT_OPTIONAL_PARAM_(String,last_change);
	vector<String> choices;
	read_choices_param(ctx, choices);
	SCRIPT_RETURN(filter_choices(input, choices, 1, (int)choices.size(), last_change));
}

// make sure at most one of the choices is active
SCRIPT_FUNCTION(exclusive_choice) {
	SCRIPT_PARAM_C(String,input);
	SCRIPT_OPTIONAL_PARAM_(String,last_change);
	vector<String> choices;
	read_choices_param(ctx, choices);
	SCRIPT_RETURN(filter_choices(input, choices, 0, 1, last_change));
}

// make sure exactly one of the choices is active
SCRIPT_FUNCTION(require_exclusive_choice) {
	SCRIPT_PARAM_C(String,input);
	SCRIPT_OPTIONAL_PARAM_(String,last_change);
	vector<String> choices;
	read_choices_param(ctx, choices);
	SCRIPT_RETURN(filter_choices(input, choices, 1, 1, last_change));
}

// make sure none of the choices are active
SCRIPT_FUNCTION(remove_choice) {
	SCRIPT_PARAM_C(String,input);
	vector<String> choices;
	read_choices_param(ctx, choices);
	SCRIPT_RETURN(filter_choices(input, choices, 0, 0, _("")));
}

// count how many choices are active
SCRIPT_FUNCTION(count_chosen) {
	SCRIPT_PARAM_C(String,input);
	SCRIPT_OPTIONAL_PARAM_C(String,choices) {
		// only count specific choices
		int count = 0;
		for (size_t start = 0, end = 0 ; next_choice(start,end,choices) ; start = end + 1) {
			if (chosen(choices.substr(start,end-start),input)) ++count;
		}
		SCRIPT_RETURN(count);
	} else {
		// count all choices => count comma's + 1
		if (input.empty()) {
			SCRIPT_RETURN(0);
		} else {
			int count = 1;
			FOR_EACH(c, input) if (c == _(',')) ++count;
			SCRIPT_RETURN(count);
		}
	}
}

// ----------------------------------------------------------------------------- : Default values

// is the given choice active?
SCRIPT_FUNCTION(mark_default) {
	SCRIPT_PARAM_C(ScriptValueP,input);
	return intrusive(new ScriptDefault(input));
}
SCRIPT_FUNCTION(is_default) {
	SCRIPT_PARAM_C(ScriptValueP,input);
	return intrusive(new ScriptDefault(input));
}

// ----------------------------------------------------------------------------- : Init

void init_script_editor_functions(Context& ctx) {
	ctx.setVariable(_("forward_editor"),           script_combined_editor); // compatability
	ctx.setVariable(_("combined_editor"),          script_combined_editor);
	ctx.setVariable(_("primary_choice"),           script_primary_choice);
	ctx.setVariable(_("chosen"),                   script_chosen);
	ctx.setVariable(_("count_chosen"),             script_count_chosen);
	ctx.setVariable(_("require_choice"),           script_require_choice);
	ctx.setVariable(_("exclusive_choice"),         script_exclusive_choice);
	ctx.setVariable(_("require_exclusive_choice"), script_require_exclusive_choice);
	ctx.setVariable(_("remove_choice"),            script_remove_choice);
	ctx.setVariable(_("is_default"),               script_is_default);
	ctx.setVariable(_("mark_default"),             script_mark_default);
}
