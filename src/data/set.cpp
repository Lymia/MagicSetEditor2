//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/set.hpp>
#include <data/game.hpp>
#include <data/stylesheet.hpp>
#include <data/card.hpp>
#include <data/keyword.hpp>
#include <data/pack.hpp>
#include <data/field.hpp>
#include <data/field/text.hpp>    // for 0.2.7 fix
#include <data/field/information.hpp>
#include <util/tagged_string.hpp> // for 0.2.7 fix
#include <util/order_cache.hpp>
#include <util/delayed_index_maps.hpp>
#include <script/script_manager.hpp>
#include <script/profiler.hpp>
#include <wx/sstream.h>

DECLARE_TYPEOF_COLLECTION(CardP);
DECLARE_TYPEOF_NO_REV(IndexMap<FieldP COMMA ValueP>);

// ----------------------------------------------------------------------------- : Set

Set::Set()
	: vcs (intrusive(new VCS()))
	, script_manager(new SetScriptManager(*this))
{}

Set::Set(const GameP& game)
	: game(game)
	, vcs (intrusive(new VCS()))
	, script_manager(new SetScriptManager(*this))
{
	data.init(game->set_fields);
}

Set::Set(const StyleSheetP& stylesheet)
	: game(stylesheet->game)
	, stylesheet(stylesheet)
	, vcs (intrusive(new VCS()))
	, script_manager(new SetScriptManager(*this))
{
	data.init(game->set_fields);
}

Set::~Set() {}


Context& Set::getContext() {
	assert(wxThread::IsMain());
	return script_manager->getContext(stylesheet);
}
Context& Set::getContext(const CardP& card) {
	assert(wxThread::IsMain());
	return script_manager->getContext(card);
}
void Set::updateStyles(const CardP& card, bool only_content_dependent) {
	script_manager->updateStyles(card, only_content_dependent);
}
void Set::updateDelayed() {
	script_manager->updateDelayed();
}

Context& Set::getContextForThumbnails() {
	assert(!wxThread::IsMain());
	if (!thumbnail_script_context) {
		thumbnail_script_context.reset(new SetScriptContext(*this));
	}
	return thumbnail_script_context->getContext(stylesheet);
}
Context& Set::getContextForThumbnails(const CardP& card) {
	assert(!wxThread::IsMain());
	if (!thumbnail_script_context) {
		thumbnail_script_context.reset(new SetScriptContext(*this));
	}
	return thumbnail_script_context->getContext(card);
}
Context& Set::getContextForThumbnails(const StyleSheetP& stylesheet) {
	assert(!wxThread::IsMain());
	if (!thumbnail_script_context) {
		thumbnail_script_context.reset(new SetScriptContext(*this));
	}
	return thumbnail_script_context->getContext(stylesheet);
}

const StyleSheet& Set::stylesheetFor(const CardP& card) {
	if (card && card->stylesheet) return *card->stylesheet;
	else                          return *stylesheet;
}
StyleSheetP Set::stylesheetForP(const CardP& card) {
	if (card && card->stylesheet) return card->stylesheet;
	else                          return stylesheet;
}

IndexMap<FieldP, ValueP>& Set::stylingDataFor(const StyleSheet& stylesheet) {
	return styling_data.get(stylesheet.name(), stylesheet.styling_fields);
}
IndexMap<FieldP, ValueP>& Set::stylingDataFor(const CardP& card) {
	if (card && card->has_styling) return card->styling_data;
	else                           return stylingDataFor(stylesheetFor(card));
}

String Set::identification() const {
	// an identifying field
	FOR_EACH_CONST(v, data) {
		if (v->fieldP->identifying) {
			return v->toFriendlyString();
		}
	}
	// otherwise the first non-information field
	FOR_EACH_CONST(v, data) {
		if (!dynamic_pointer_cast<InfoValue>(v)) {
			return v->toFriendlyString();
		}
	}
	return wxEmptyString;
}

ScriptValueP& Set::value(const String& name) {
	for(IndexMap<FieldP, ValueP>::iterator it = data.begin() ; it != data.end() ; ++it) {
		if ((*it)->fieldP->name == name) {
			return (*it)->value;
		}
	}
	throw InternalError(_("Expected a set field with name '")+name+_("'"));
}


String Set::typeName() const { return _("set"); }
Version Set::fileVersion() const { return file_version_set; }

void Set::validate(Version file_app_version) {
	Packaged::validate(file_app_version);
	// are the
	if (!game) {
		throw Error(_ERROR_1_("no game specified",_TYPE_("set")));
	}
	if (!stylesheet) {
		// TODO : Allow user to select a different style
		throw Error(_ERROR_("no stylesheet specified for the set"));
	}
	if (stylesheet->game != game) {
		throw Error(_ERROR_("stylesheet and set refer to different game"));
	}
	
	// we want at least one card
	if (cards.empty()) cards.push_back(intrusive(new Card(*game)));
	// update scripts
	script_manager->updateAll();
}

IMPLEMENT_REFLECTION(Set) {
	REFLECT(game);
	if (game) {
		REFLECT_IF_READING {
			data.init(game->set_fields);
		}
		WITH_DYNAMIC_ARG(game_for_reading, game.get());
		REFLECT(stylesheet);
		WITH_DYNAMIC_ARG(stylesheet_for_reading, stylesheet.get());
		REFLECT_N("set_info", data);
		if (stylesheet) {
			REFLECT_N("styling", styling_data);
		}
		// Experimental: save each card to a different file
		reflect_cards(reflector);
		REFLECT(keywords);
		REFLECT(pack_types);
	}
	reflect_set_info_get_member(reflector,data);
	REFLECT_NO_SCRIPT_N("version_control", vcs);
	REFLECT(apprentice_code);
}

// TODO: make this a more generic function to be used elsewhere
template <typename Reflector>
void Set::reflect_cards (Reflector& reflector) {
	REFLECT(cards);
}

template <>
void Set::reflect_cards<Writer> (Writer& reflector) {
	// When writing to a directory, we write each card in a separate file.
	// We don't do this in zipfiles because it leads to bloat.
	if (isZipfile()) {
		REFLECT(cards);
	} else {
		set<String> used;
		FOR_EACH(card, cards) {
			// pick a unique filename for this card
			// can't use Package::newFileName, because then we get conflicts with the previous save of the same card
			String filename = _("card ") + normalize_internal_filename(clean_filename(card->identification()));
			String full_name = filename;
			int i = 0;

			while (used.find(full_name) != used.end()) {
				full_name = String(filename) << _(".") << ++i;
			}
			used.insert(full_name);

			// writeFile won't quite work because we'd need
			// include file: card: filename
			// to do that
			{
				OutputStreamP stream = openOut(full_name);
				Writer writer(*stream, app_version);
				writer.handle(_("card"), card);
			}
			referenceFile(full_name);
			REFLECT_N("include_file", full_name);
		}
	}
}

// ----------------------------------------------------------------------------- : Script utilities

ScriptValueP make_iterator(const Set& set) {
	return intrusive(new ScriptCollectionIterator<vector<CardP> >(&set.cards));
}

void mark_dependency_member(const Set& set, const String& name, const Dependency& dep) {
	// is it the card list?
	if (name == _("cards")) {
		set.game->dependent_scripts_cards.add(dep);
		return;
	}
	// is it the keywords?
	if (name == _("keywords")) {
		set.game->dependent_scripts_keywords.add(dep);
		return;
	}
	// is it in the set data?
	mark_dependency_member(set.data, name, dep);
}

// in scripts, set.something is read from the set_info
template <typename Reflector>
void reflect_set_info_get_member(Reflector& reflector, const IndexMap<FieldP, ValueP>& data) {}
void reflect_set_info_get_member(GetMember& reflector, const IndexMap<FieldP, ValueP>& data) {
	REFLECT_NAMELESS(data);
}

int Set::positionOfCard(const CardP& card, const ScriptValueP& order_by, const ScriptValueP& filter) {
	// TODO : Lock the map?
	assert(order_by);
	OrderCacheP& order = order_cache[make_pair(order_by,filter)];
	if (!order) {
		// 1. make a list of the order value for each card
		vector<String> values; values.reserve(cards.size());
		vector<int>    keep;   if(filter) keep.reserve(cards.size());
		FOR_EACH_CONST(c, cards) {
			Context& ctx = getContext(c);
			values.push_back(order_by->eval(ctx)->toString());
			if (filter) {
				keep.push_back(filter->eval(ctx)->toBool());
			}
		}
		#if USE_SCRIPT_PROFILING
			Timer t;
			Profiler prof(t, order_by.get(), _("init order cache"));
		#endif
		// 3. initialize order cache
		order = intrusive(new OrderCache<CardP>(cards, values, filter ? &keep : nullptr));
	}
	return order->find(card);
}
int Set::numberOfCards(const ScriptValueP& filter) {
	if (!filter) return (int)cards.size();
	map<ScriptValueP,int>::const_iterator it = filter_cache.find(filter);
	if (it !=filter_cache.end()) {
		return it->second;
	} else {
		int n = 0;
		FOR_EACH_CONST(c, cards) {
			if (filter->eval(getContext(c))->toBool()) ++n;
		}
		filter_cache.insert(make_pair(filter,n));
		return n;
	}
}
void Set::clearOrderCache() {
	order_cache.clear();
	filter_cache.clear();
}

// ----------------------------------------------------------------------------- : SetView

SetView::SetView() {}

SetView::~SetView() {
	if (set) set->actions.removeListener(this);
}

void SetView::setSet(const SetP& newSet) {
	// no longer listening to old set
	if (set) {
		onBeforeChangeSet();
		set->actions.removeListener(this);
	}
	set = newSet;
	// start listening to new set
	if (set) set->actions.addListener(this);
	onChangeSet();
}
