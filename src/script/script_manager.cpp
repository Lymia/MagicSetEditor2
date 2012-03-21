//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <script/script_manager.hpp>
#include <script/to_value.hpp>
#include <script/functions/functions.hpp>
#include <script/profiler.hpp>
#include <data/set.hpp>
#include <data/stylesheet.hpp>
#include <data/game.hpp>
#include <data/card.hpp>
#include <data/field.hpp>
#include <data/action/set.hpp>
#include <data/action/value.hpp>
#include <data/action/keyword.hpp>
#include <util/error.hpp>

typedef map<const StyleSheet*,Context*> Contexts;
DECLARE_TYPEOF(Contexts);
DECLARE_TYPEOF_COLLECTION(CardP);
DECLARE_TYPEOF_COLLECTION(FieldP);
DECLARE_TYPEOF_COLLECTION(Dependency);
DECLARE_TYPEOF_NO_REV(IndexMap<FieldP COMMA StyleP>);
DECLARE_TYPEOF_NO_REV(IndexMap<FieldP COMMA ValueP>);

//#define LOG_UPDATES

// ----------------------------------------------------------------------------- : SetScriptContext : initialization

SetScriptContext::SetScriptContext(Set& set)
	: set(set)
{}

SetScriptContext::~SetScriptContext() {
	// destroy contexts
	FOR_EACH(sc, contexts) {
		delete sc.second;
	}
}

Context& SetScriptContext::getContext(const StyleSheetP& stylesheet) {
	Contexts::iterator it = contexts.find(stylesheet.get());
	if (it != contexts.end()) {
		return *it->second; // we already have a context
	} else {
		// create a new context
		Context* ctx = new Context();
		contexts.insert(make_pair(stylesheet.get(), ctx));
		// variables
		//  NOTE: do not use a smart pointer for the pointer to the set, because the set owns this
		//        which would lead to a reference cycle.
		init_script_functions(*ctx);
		ctx->setVariable(SCRIPT_VAR_set,        intrusive(new ScriptObject<Set*>(&set)));
		ctx->setVariable(SCRIPT_VAR_game,       to_script(set.game));
		ctx->setVariable(SCRIPT_VAR_stylesheet, to_script(stylesheet));
		ctx->setVariable(SCRIPT_VAR_card_style, to_script(&stylesheet->card_style));
		ctx->setVariable(SCRIPT_VAR_card,       set.cards.empty() ? script_nil : to_script(set.cards.front())); // dummy value
		ctx->setVariable(SCRIPT_VAR_styling,    to_script(&set.stylingDataFor(*stylesheet)));
		try {
			// perform init scripts, don't use a scope, variables stay bound in the context
			set.game  ->init_script.invoke(*ctx, false);
			stylesheet->init_script.invoke(*ctx, false);
		} catch (const Error& e) {
			handle_error(e);
		}
		onInit(stylesheet, ctx);
		return *ctx;
	}
}
Context& SetScriptContext::getContext(const CardP& card) {
	StyleSheetP stylesheet = set.stylesheetForP(card);
	Context& ctx = getContext(stylesheet);
	if (card) {
		ctx.setVariable(SCRIPT_VAR_card,    to_script(card));
		ctx.setVariable(SCRIPT_VAR_styling, to_script(&set.stylingDataFor(card)));
	} else {
		ctx.setVariable(SCRIPT_VAR_card,    script_nil);
		ctx.setVariable(SCRIPT_VAR_styling, to_script(&set.stylingDataFor(*stylesheet)));
	}
	return ctx;
}

// ----------------------------------------------------------------------------- : SetScriptManager : initialization

SetScriptManager::SetScriptManager(Set& set)
	: SetScriptContext(set)
	, delay(0)
{
	// add as an action listener for the set, so we receive actions
	set.actions.addListener(this);
}

SetScriptManager::~SetScriptManager() {
	set.actions.removeListener(this);
}

void SetScriptManager::onInit(const StyleSheetP& stylesheet, Context* ctx) {
	assert(wxThread::IsMain());
	// initialize dependencies
	try {
		// find script dependencies
		initDependencies(*ctx, *set.game);
		initDependencies(*ctx, *stylesheet);
	} catch (const Error& e) {
		handle_error(e);
	}
}

void SetScriptManager::initDependencies(Context& ctx, Game& game) {
	if (game.dependencies_initialized) return;
	game.dependencies_initialized = true;
	// find dependencies of card fields
	FOR_EACH(f, game.card_fields) {
		f->initDependencies(ctx, Dependency(DEP_CARD_FIELD, f->index));
	}
	// find dependencies of set fields
	FOR_EACH(f, game.set_fields) {
		f->initDependencies(ctx, Dependency(DEP_SET_FIELD, f->index));
	}
}


void SetScriptManager::initDependencies(Context& ctx, StyleSheet& stylesheet) {
	if (stylesheet.dependencies_initialized) return;
	stylesheet.dependencies_initialized = true;
	// find dependencies of extra card fields
	FOR_EACH(f, stylesheet.extra_card_fields) {
		f->initDependencies(ctx, Dependency(DEP_EXTRA_CARD_FIELD, f->index, &stylesheet));
	}
	// find dependencies of choice images and other style stuff
	FOR_EACH(s, stylesheet.card_style) {
		s->initDependencies(ctx, Dependency(DEP_CARD_STYLE, s->fieldP->index, &stylesheet));
		// are there dependencies of this style on other style properties?
		Dependency test(DEP_DUMMY, false);
		s->checkContentDependencies(ctx, test);
		if (test.index) s->content_dependent = true;
	}
	FOR_EACH(s, stylesheet.extra_card_style) {
		// are there dependencies of this style on other style properties?
		Dependency test(DEP_DUMMY, false);
		s->checkContentDependencies(ctx, test);
		if (test.index) s->content_dependent = true;
	}
}

// ----------------------------------------------------------------------------- : ScriptManager : updating

void SetScriptManager::onAction(const Action& action, bool undone) {
	TYPE_CASE(action, ValueAction) {
		if (action.card) {
			// we can just turn the Card* into a CardP
			updateValue(*action.valueP, intrusive_from_existing(const_cast<Card*>(action.card)), &action);
			return;
		} else {
			// is it a keyword's fake value?
			KeywordTextValue* value = dynamic_cast<KeywordTextValue*>(action.valueP.get());
			if (value) {
				if (value->underlying == &value->keyword.match) {
					// script
					Context& ctx = getContext(set.stylesheet);
					value->update(ctx);
					// changed the 'match' string of a keyword, rebuild database and regex so matching is correct
					value->keyword.prepare(set.game->keyword_parameter_types, true);
					set.keyword_db.clear();
				}
				delay |= DELAY_KEYWORDS;
				return;
			}
			// a set or styling value
			updateValue(*action.valueP, CardP(), &action);
		}
	}
	TYPE_CASE_(action, ScriptValueEvent) {
		return; // Don't go into an infinite loop because of our own events
	}
	TYPE_CASE(action, AddCardAction) {
		if (action.action.adding != undone) {
			// update the added cards specificly
			FOR_EACH_CONST(step, action.action.steps) {
				const CardP& card = step.item;
				Context& ctx = getContext(card);
				FOR_EACH(v, card->data) {
					v->update(ctx,&action);
				}
			}
		}
		// note: fallthrough
	}
	TYPE_CASE_(action, CardListAction) {
		#ifdef LOG_UPDATES
			wxLogDebug(_("Card dependencies"));
		#endif
		updateAllDependend(set.game->dependent_scripts_cards);
		#ifdef LOG_UPDATES
			wxLogDebug(_("-------------------------------\n"));
		#endif
	}
	TYPE_CASE_(action, KeywordListAction) {
		updateAllDependend(set.game->dependent_scripts_keywords);
		return;
	}
	TYPE_CASE_(action, ChangeKeywordModeAction) {
		updateAllDependend(set.game->dependent_scripts_keywords);
		return;
	}
	TYPE_CASE(action, ChangeCardStyleAction) {
		updateAllDependend(set.game->dependent_scripts_stylesheet, action.card);
	}
	TYPE_CASE_(action, ChangeSetStyleAction) {
		updateAllDependend(set.game->dependent_scripts_stylesheet);
		return;
	}
}

void SetScriptManager::updateStyles(const CardP& card, bool only_content_dependent) {
	assert(card);
	const StyleSheet& stylesheet = set.stylesheetFor(card);
	Context& ctx = getContext(card);
	if (!only_content_dependent) {
		// update extra card fields
		IndexMap<FieldP,ValueP>& extra_data = card->extraDataFor(stylesheet);
		FOR_EACH(v, extra_data) {
			if (v->update(ctx)) {
				// changed, send event
				ScriptValueEvent change(card.get(), v.get());
				set.actions.tellListeners(change, false);
			}
		}
	}
	// update all styles
	updateStyles(ctx, stylesheet.card_style,       only_content_dependent);
	updateStyles(ctx, stylesheet.extra_card_style, only_content_dependent);
}
void SetScriptManager::updateStyles(Context& ctx, const IndexMap<FieldP,StyleP>& styles, bool only_content_dependent) {
	FOR_EACH_CONST(s, styles) {
		if (only_content_dependent && !s->content_dependent) continue;
		try {
			if (int change = s->update(ctx)) {
				// style has changed, tell listeners
				s->tellListeners(change | (only_content_dependent ? CHANGE_ALREADY_PREPARED : 0) );
			}
		} catch (const ScriptError& e) {
			// NOTE: don't handle errors now, we are likely in an onPaint handler
			handle_error(ScriptError(e.what() + _("\n  while updating styles for '") + s->fieldP->name + _("'")));
		}
	}
}

void SetScriptManager::updateDelayed() {
	if (delay & DELAY_KEYWORDS) {
		updateAllDependend(set.game->dependent_scripts_keywords);
	}
	delay = 0;
}

void SetScriptManager::updateValue(Value& value, const CardP& card, Action const* action) {
	Age starting_age = Age::next(); // the start of the update process, use next(), so the modified value also gets a chance to be updated
	deque<ToUpdate> to_update;
	// execute script for initial changed value
	value.last_modified = starting_age;
	value.update(getContext(card), action);
	#ifdef LOG_UPDATES
		wxLogDebug(_("Start:     %s"), value.fieldP->name);
	#endif
	// update dependent scripts
	alsoUpdate(to_update, value.fieldP->dependent_scripts, card);
	updateRecursive(to_update, starting_age);
	#ifdef LOG_UPDATES
		wxLogDebug(_("-------------------------------\n"));
	#endif
}

void SetScriptManager::updateAll() {
	#ifdef LOG_UPDATES
		wxLogDebug(_("Update all"));
	#endif
	wxBusyCursor busy;
	// update set data
	Context& ctx = getContext(set.stylesheet);
	FOR_EACH(v, set.data) {
		try {
			PROFILER2( v->fieldP.get(), _("update set.") + v->fieldP->name );
			v->update(ctx);
		} catch (const ScriptError& e) {
			handle_error(ScriptError(e.what() + _("\n  while updating set value '") + v->fieldP->name + _("'")));
		}
	}
	// update card data of all cards
	FOR_EACH(card, set.cards) {
		Context& ctx = getContext(card);
		FOR_EACH(v, card->data) {
			try {
				#if USE_SCRIPT_PROFILING
					Timer t;
					Profiler prof(t, v->fieldP.get(), _("update card.") + v->fieldP->name);
				#endif
				v->update(ctx);
			} catch (const ScriptError& e) {
				handle_error(ScriptError(e.what() + _("\n  while updating card value '") + v->fieldP->name + _("'")));
			}
		}
	}
	// update things that depend on the card list
	updateAllDependend(set.game->dependent_scripts_cards);
	#ifdef LOG_UPDATES
		wxLogDebug(_("-------------------------------\n"));
	#endif
}

void SetScriptManager::updateAllDependend(const vector<Dependency>& dependent_scripts, const CardP& card) {
	deque<ToUpdate> to_update;
	Age starting_age = Age::next();
	alsoUpdate(to_update, dependent_scripts, card);
	updateRecursive(to_update, starting_age);
}

void SetScriptManager::updateRecursive(deque<ToUpdate>& to_update, Age starting_age) {
	if (to_update.empty()) return;
	set.clearOrderCache(); // clear caches before evaluating a round of scripts
	while (!to_update.empty()) {
		updateToUpdate(to_update.front(), to_update, starting_age);
		to_update.pop_front();
	}
}

void SetScriptManager::updateToUpdate(const ToUpdate& u, deque<ToUpdate>& to_update, Age starting_age) {
	Age& age = u.value->last_modified;
	if (starting_age <= age)  return; // this value was already updated
	age = starting_age; // mark as updated
	Context& ctx = getContext(u.card);
	bool changes = false;
	try {
		changes = u.value->update(ctx);
	} catch (const ScriptError& e) {
		handle_error(ScriptError(e.what() + _("\n  while updating value '") + u.value->fieldP->name + _("'")));
	}
	if (changes) {
		// changed, send event
		ScriptValueEvent change(u.card.get(), u.value);
		set.actions.tellListeners(change, false);
		// u.value has changed, also update values with a dependency on u.value
		alsoUpdate(to_update, u.value->fieldP->dependent_scripts, u.card);
	#ifdef LOG_UPDATES
		wxLogDebug(_("Changed: %s"), u.value->fieldP->name);
	#endif
	}
	#ifdef LOG_UPDATES
	else
		wxLogDebug(_("Same:    %s"), u.value->fieldP->name);
	#endif
}

void SetScriptManager::alsoUpdate(deque<ToUpdate>& to_update, const vector<Dependency>& deps, const CardP& card) {
	FOR_EACH_CONST(d, deps) {
		switch (d.type) {
			case DEP_SET_FIELD: {
				ValueP value = set.data.at(d.index);
				to_update.push_back(ToUpdate(value.get(), CardP()));
				break;
			} case DEP_CARD_FIELD: {
				if (card) {
					ValueP value = card->data.at(d.index);
					to_update.push_back(ToUpdate(value.get(), card));
					break;
				} else {
					// There is no card, so the update should affect all cards (fall through).
				}
			} case DEP_CARDS_FIELD: {
				// something invalidates a card value for all cards, so all cards need updating
				FOR_EACH(card, set.cards) {
					ValueP value = card->data.at(d.index);
					to_update.push_back(ToUpdate(value.get(), card));
				}
				break;
			} case DEP_CARD_STYLE: {
				// a generated image has become invalid, there is not much we can do
				// because the index is not exact enough, it only gives the field
				StyleSheet* stylesheet = reinterpret_cast<StyleSheet*>(d.data);
				StyleP style = stylesheet->card_style.at(d.index);
				style->invalidate();
				// something changed, send event
				ScriptStyleEvent change(stylesheet, style.get());
				set.actions.tellListeners(change, false);
				break;
			} case DEP_EXTRA_CARD_FIELD: {
			/*	// Not needed, extra card fields are handled in updateStyles()
				if (card) {
					StyleSheet* stylesheet = reinterpret_cast<StyleSheet*>(d.data);
					StyleSheet* stylesheet_card = &set.stylesheetFor(card);
					if (stylesheet == stylesheet_card) {
						ValueP value = card->extra_data.at(d.index);
						to_update.push_back(ToUpdate(value.get(), card));
					}
				}*/
				break;
			} case DEP_CARD_COPY_DEP: {
				// propagate dependencies from another field
				FieldP f = set.game->card_fields[d.index];
				alsoUpdate(to_update, f->dependent_scripts, card);
				break;
			} case DEP_SET_COPY_DEP: {
				// propagate dependencies from another field
				FieldP f = set.game->set_fields[d.index];
				alsoUpdate(to_update, f->dependent_scripts, card);
				break;
			} default:
				assert(false);
		}
	}
}
