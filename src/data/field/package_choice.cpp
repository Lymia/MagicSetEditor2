//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2010 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/field/package_choice.hpp>
#include <util/io/package_manager.hpp>

// ----------------------------------------------------------------------------- : PackageChoiceField

IMPLEMENT_FIELD_TYPE(PackageChoice, "package choice");

void PackageChoiceField::initDependencies(Context& ctx, const Dependency& dep) const {
	Field ::initDependencies(ctx, dep);
	script. initDependencies(ctx, dep);
}

IMPLEMENT_REFLECTION(PackageChoiceField) {
	REFLECT_BASE(Field);
	REFLECT(script);
	REFLECT(match);
	REFLECT(initial);
	REFLECT(required);
	REFLECT(empty_name);
}

// ----------------------------------------------------------------------------- : PackageChoiceStyle

PackageChoiceStyle::PackageChoiceStyle(const PackageChoiceFieldP& field)
	: Style(field)
{}

int PackageChoiceStyle::update(Context& ctx) {
	return Style     ::update(ctx)
	     | font       .update(ctx) * CHANGE_OTHER;
}
/*void PackageChoiceStyle::initDependencies(Context& ctx, const Dependency& dep) const {
	Style     ::initDependencies(ctx, dep);
//	font       .initDependencies(ctx, dep);
}*/

IMPLEMENT_REFLECTION(PackageChoiceStyle) {
	REFLECT_BASE(Style);
	REFLECT(font);
}

// ----------------------------------------------------------------------------- : PackageChoiceValue

String PackageChoiceValue::toString() const {
	PackagedP pack = getPackage();
	if (pack.get()) return pack->short_name;
	else      return _("");
}

PackagedP PackageChoiceValue::getPackage() const {
	if (package_name.empty()) return nullptr;
	else return package_manager.openAny(package_name, true);
}

bool PackageChoiceValue::update(Context& ctx) {
	bool change = field().script.invokeOn(ctx, package_name);
	Value::update(ctx);
	return change;
}

void PackageChoiceValue::reflect(Reader& reflector) {
	REFLECT_NAMELESS(package_name);
}
void PackageChoiceValue::reflect(Writer& reflector) {
	REFLECT_NAMELESS(package_name);
}
void PackageChoiceValue::reflect(GetDefaultMember& reflector) {
	if (package_name.empty()) {
		REFLECT_NAMELESS(package_name);
	} else if(package_name != field().initial) {
		// add a space to the name, to indicate the dependency doesn't have to be marked
		// see also PackageManager::openFileFromPackage and SymbolFontRef::loadFont
		REFLECT_NAMELESS(_("/:NO-WARN-DEP:") + package_name);
	} else {
		REFLECT_NAMELESS(_("/") + package_name);
	}
}
void PackageChoiceValue::reflect(GetMember& reflector) {}
