//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2017 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/field/color.hpp>
#include <script/script.hpp>

DECLARE_TYPEOF_COLLECTION(ColorField::ChoiceP);

// ----------------------------------------------------------------------------- : ColorField

ColorField::ColorField()
  : allow_custom(true)
  , default_name(_("Default"))
{}

IMPLEMENT_FIELD_TYPE(Color, "color");

void ColorField::initDependencies(Context& ctx, const Dependency& dep) const {
  Field        ::initDependencies(ctx, dep);
  script        .initDependencies(ctx, dep);
  default_script.initDependencies(ctx, dep);
}


IMPLEMENT_REFLECTION(ColorField) {
  REFLECT_BASE(Field);
  REFLECT(script);
  REFLECT_N("default", default_script);
  REFLECT(initial);
  REFLECT(default_name);
  REFLECT(allow_custom);
  REFLECT(choices);
}

// ----------------------------------------------------------------------------- : ColorField::Choice

IMPLEMENT_REFLECTION(ColorField::Choice) {
  if (tag.reading() && !tag.isComplex()) {
    REFLECT_NAMELESS(name);
    color = parse_color(name);
  } else {
    REFLECT(name);
    REFLECT(color);
  }
}

// ----------------------------------------------------------------------------- : ColorStyle

ColorStyle::ColorStyle(const ColorFieldP& field)
  : Style(field)
  , radius(0)
  , left_width(100000), right_width (100000)
  , top_width (100000), bottom_width(100000)
  , combine(COMBINE_NORMAL)
{}

IMPLEMENT_REFLECTION(ColorStyle) {
  REFLECT_BASE(Style);
  REFLECT(radius);
  REFLECT(left_width);
  REFLECT(right_width);
  REFLECT(top_width);
  REFLECT(bottom_width);
  REFLECT(combine);
}

int ColorStyle::update(Context& ctx) {
  return Style::update(ctx);
}

// ----------------------------------------------------------------------------- : ColorValue

ColorValue::ColorValue(const ColorFieldP& field)
  : Value(field)
  , value( !field->initial.isDefault() ? field->initial()
         : !field->choices.empty()     ? field->choices[0]->color
         :                               *wxBLACK
         , true)
{}
  
String ColorValue::toString() const {
  if (value.isDefault()) return field().default_name;
  // is this a named color?
  FOR_EACH(c, field().choices) {
    if (value() == c->color) return c->name;
  }
  return _("<color>");
}
bool ColorValue::update(Context& ctx) {
  bool change = field().default_script.invokeOnDefault(ctx, value)
              | field().        script.invokeOn(ctx, value);
  Value::update(ctx);
  return change;
}

IMPLEMENT_REFLECTION_NAMELESS(ColorValue) {
  if (fieldP->save_value || tag.scripting() || tag.reading()) REFLECT_NAMELESS(value);
}
