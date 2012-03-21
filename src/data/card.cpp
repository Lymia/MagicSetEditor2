//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/card.hpp>
#include <data/game.hpp>
#include <data/stylesheet.hpp>
#include <data/field.hpp>
#include <util/error.hpp>
#include <util/reflect.hpp>
#include <util/delayed_index_maps.hpp>

DECLARE_TYPEOF_COLLECTION(FieldP);
DECLARE_TYPEOF_NO_REV(IndexMap<FieldP COMMA ValueP>);

// ----------------------------------------------------------------------------- : Card

Card::Card()
	  // for files made before we saved these times, set the time to 'yesterday'
	: time_created (wxDateTime::Now().Subtract(wxDateSpan::Day()).ResetTime())
	, time_modified(wxDateTime::Now().Subtract(wxDateSpan::Day()).ResetTime())
	, has_styling(false)
{
	if (!game_for_reading()) {
		throw InternalError(_("game_for_reading not set"));
	}
	data.init(game_for_reading()->card_fields);
}

Card::Card(const Game& game)
	: time_created (wxDateTime::Now())
	, time_modified(wxDateTime::Now())
	, has_styling(false)
{
	data.init(game.card_fields);
}

String Card::identification() const {
	// an identifying field
	FOR_EACH_CONST(v, data) {
		if (v->fieldP->identifying) {
			return v->toFriendlyString();
		}
	}
	// otherwise the first field
	if (!data.empty()) {
		return data.at(0)->toFriendlyString();
	} else {
		return wxEmptyString;
	}
}

bool Card::contains(String const& query) const {
	FOR_EACH_CONST(v, data) {
		if (find_i(v->toFriendlyString(),query) != String::npos) return true;
	}
	if (find_i(notes,query) != String::npos) return true;
	return false;
}

ScriptValueP& Card::value(const String& name) {
	for (IndexMap<FieldP, ValueP>::iterator it = data.begin() ; it != data.end() ; ++it) {
		if ((*it)->fieldP->name == name) {
			return (*it)->value;
		}
	}
	throw InternalError(_("Expected a card field with name '")+name+_("'"));
}
const ScriptValueP& Card::value(const String& name) const {
	for (IndexMap<FieldP, ValueP>::const_iterator it = data.begin() ; it != data.end() ; ++it) {
		if ((*it)->fieldP->name == name) {
			return (*it)->value;
		}
	}
	throw InternalError(_("Expected a card field with name '")+name+_("'"));
}

IndexMap<FieldP, ValueP>& Card::extraDataFor(const StyleSheet& stylesheet) {
	return extra_data.get(stylesheet.name(), stylesheet.extra_card_fields);
}

void mark_dependency_member(const Card& card, const String& name, const Dependency& dep) {
	mark_dependency_member(card.data, name, dep);
}

IMPLEMENT_REFLECTION(Card) {
	REFLECT(stylesheet);
	REFLECT(has_styling);
	if (has_styling) {
		if (stylesheet) {
			REFLECT_IF_READING styling_data.init(stylesheet->styling_fields);
			REFLECT(styling_data);
		} else if (stylesheet_for_reading()) {
			REFLECT_IF_READING styling_data.init(stylesheet_for_reading()->styling_fields);
			REFLECT(styling_data);
		} else if (reflector.isReading()) {
			has_styling = false; // We don't know the style, this can be because of copy/pasting
		}
	}
	REFLECT(notes);
	REFLECT(time_created);
	REFLECT(time_modified);
	REFLECT(extra_data); // don't allow scripts to depend on style specific data
	REFLECT_NAMELESS(data);
}
