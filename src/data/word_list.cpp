//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/word_list.hpp>

// ----------------------------------------------------------------------------- : WordList

WordListWord::WordListWord()
	: line_below(false)
	, is_prefix(false)
{}

IMPLEMENT_REFLECTION_NO_SCRIPT(WordListWord) {
	REFLECT_IF_READING_COMPOUND_OR(line_below || is_prefix || isGroup() || script) {
		// complex value
		REFLECT(name);
		REFLECT(line_below);
		REFLECT(is_prefix);
		REFLECT(words);
		REFLECT(script);
	} else {
		REFLECT_NAMELESS(name);
	}
}

IMPLEMENT_REFLECTION_NO_SCRIPT(WordList) {
	REFLECT(name);
	REFLECT(words);
}


// ----------------------------------------------------------------------------- : Auto replace words

AutoReplace::AutoReplace()
	: enabled(true)
	, whole_word(true)
	, custom(true)
{}

IMPLEMENT_REFLECTION_NO_SCRIPT(AutoReplace) {
	REFLECT(enabled);
	REFLECT(whole_word);
	REFLECT(match);
	REFLECT(replace);
}
