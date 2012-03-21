//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <script/value.hpp>
#include <script/to_value.hpp>
#include <script/context.hpp>
#include <gfx/generated_image.hpp>
#include <util/error.hpp>
#include <util/tagged_string.hpp>
#include <boost/pool/singleton_pool.hpp>

DECLARE_TYPEOF_COLLECTION(pair<Variable COMMA ScriptValueP>);

// ----------------------------------------------------------------------------- : ScriptValue
// Base cases

String       ScriptValue::toString()                        const { throw ScriptErrorConversion(typeName(), _TYPE_("string" )); }
int          ScriptValue::toInt()                           const { throw ScriptErrorConversion(typeName(), _TYPE_("integer" )); }
bool         ScriptValue::toBool()                          const { throw ScriptErrorConversion(typeName(), _TYPE_("boolean" )); }
double       ScriptValue::toDouble()                        const { throw ScriptErrorConversion(typeName(), _TYPE_("double"  )); }
AColor       ScriptValue::toColor()                         const { throw ScriptErrorConversion(typeName(), _TYPE_("color"   )); }
wxDateTime   ScriptValue::toDateTime()                      const { throw ScriptErrorConversion(typeName(), _TYPE_("date"    )); }
GeneratedImageP ScriptValue::toImage()                      const { throw ScriptErrorConversion(typeName(), _TYPE_("image"   )); }
String       ScriptValue::toCode()                          const { return toString(); }
String       ScriptValue::toFriendlyString()                const { return toString(); }
ScriptValueP ScriptValue::do_eval(Context&, bool)           const { return delay_error(ScriptErrorConversion(typeName(), _TYPE_("function"))); }
ScriptValueP ScriptValue::next(ScriptValueP* key_out)             { throw InternalError(_("Can't convert from ")+typeName()+_(" to iterator")); }
ScriptValueP ScriptValue::makeIterator()                    const { return delay_error(ScriptErrorConversion(typeName(), _TYPE_("collection"))); }
int          ScriptValue::itemCount()                       const { throw ScriptErrorConversion(typeName(), _TYPE_("collection")); }
CompareWhat  ScriptValue::compareAs(String& compare_str, void const*& compare_ptr) const {
	compare_str = toCode();
	return COMPARE_AS_STRING;
}
ScriptValueP ScriptValue::getMember(const String& name) const {
	long index;
	if (name.ToLong(&index)) {
		return getIndex(index);
	} else {
		return delay_error(ScriptErrorNoMember(typeName(), name));
	}
}
ScriptValueP ScriptValue::getIndex(int index) const {
	return delay_error(ScriptErrorNoMember(typeName(), String()<<index));
}


ScriptValueP ScriptValue::simplifyClosure(ScriptClosure&) const { return ScriptValueP(); }

ScriptValueP ScriptValue::dependencyMember(const String& name, const Dependency&) const { return dependency_dummy; }
ScriptValueP ScriptValue::dependencyName(const ScriptValue& container, const Dependency& dep) const {
	return container.dependencyMember(toString(),dep);
}
ScriptValueP ScriptValue::dependencies(Context&,               const Dependency&) const { return dependency_dummy; }
void ScriptValue::dependencyThis(const Dependency& dep) {}

bool approx_equal(double a, double b) {
	return a == b || fabs(a - b) < 1e-14;
}

/// compare script values for equallity
bool equal(const ScriptValueP& a, const ScriptValueP& b) {
	if (a == b) return true;
	ScriptType at = a->type(), bt = b->type();
	if (at == bt && at == SCRIPT_INT) {
		return a->toInt() == b->toInt();
	} else if (at == bt && at == SCRIPT_BOOL) {
		return a->toBool() == b->toBool();
	} else if ((at == SCRIPT_INT || at == SCRIPT_DOUBLE) &&
	           (bt == SCRIPT_INT || bt == SCRIPT_DOUBLE)) {
		return approx_equal( a->toDouble(), b->toDouble());
	} else if (at == SCRIPT_COLLECTION && bt == SCRIPT_COLLECTION) {
		// compare each element
		if (a->itemCount() != b->itemCount()) return false;
		ScriptValueP a_it = a->makeIterator();
		ScriptValueP b_it = b->makeIterator();
		while (true) {
			ScriptValueP a_v = a_it->next();
			ScriptValueP b_v = b_it->next();
			if (!a_v || !b_v) return a_v == b_v;
			if (!equal(a_v, b_v)) return false;
		}
	} else {
		String      as,  bs;
		const void* ap, *bp;
		CompareWhat aw = a->compareAs(as, ap);
		CompareWhat bw = b->compareAs(bs, bp);
		// compare pointers or strings
		if (aw == COMPARE_AS_STRING || bw == COMPARE_AS_STRING) {
			return as == bs;
		} else if (aw == COMPARE_AS_POINTER || bw == COMPARE_AS_POINTER) {
			return ap == bp;
		} else {
			return false;
		}
	}
}

// ----------------------------------------------------------------------------- : Errors

ScriptType ScriptDelayedError::type() const { return SCRIPT_ERROR; }

String ScriptDelayedError::typeName()  const            { throw error; }
String ScriptDelayedError::toString()  const            { throw error; }
double ScriptDelayedError::toDouble()  const            { throw error; }
int    ScriptDelayedError::toInt()     const            { throw error; }
bool   ScriptDelayedError::toBool()    const            { throw error; }
AColor ScriptDelayedError::toColor()   const            { throw error; }
int    ScriptDelayedError::itemCount() const            { throw error; }
CompareWhat ScriptDelayedError::compareAs(String&, void const*&) const { throw error; }
ScriptValueP ScriptDelayedError::getMember(const String&) const                           { return intrusive_from_existing(const_cast<ScriptDelayedError*>(this)); }
ScriptValueP ScriptDelayedError::dependencyMember(const String&, const Dependency&) const { return intrusive_from_existing(const_cast<ScriptDelayedError*>(this)); }
ScriptValueP ScriptDelayedError::do_eval(Context&, bool) const                            { return intrusive_from_existing(const_cast<ScriptDelayedError*>(this)); }
ScriptValueP ScriptDelayedError::dependencies(Context&, const Dependency&) const          { return intrusive_from_existing(const_cast<ScriptDelayedError*>(this)); }
ScriptValueP ScriptDelayedError::makeIterator() const                                     { return intrusive_from_existing(const_cast<ScriptDelayedError*>(this)); }


// ----------------------------------------------------------------------------- : Iterators

ScriptType ScriptIterator::type() const { return SCRIPT_ITERATOR; }
String ScriptIterator::typeName() const { return _("iterator"); }
CompareWhat ScriptIterator::compareAs(String&, void const*&) const { return COMPARE_NO; }
ScriptValueP ScriptIterator::makeIterator() const { return intrusive_from_existing(const_cast<ScriptIterator*>(this)); }

// Iterator over a range of integers
class ScriptRangeIterator : public ScriptIterator {
  public:
	// Construct a range iterator with the given bounds (inclusive)
	ScriptRangeIterator(int start, int end)
		: pos(start), start(start), end(end) {}
	virtual ScriptValueP next(ScriptValueP* key_out) {
		if (pos <= end) {
			if (key_out) *key_out = to_script(pos-start);
			return to_script(pos++);
		} else {
			return ScriptValueP();
		}
	}
  private:
	int pos, start, end;
};

ScriptValueP rangeIterator(int start, int end) {
	return intrusive(new ScriptRangeIterator(start, end));
}

// ----------------------------------------------------------------------------- : Integers

#define USE_POOL_ALLOCATOR

// Integer values
class ScriptInt : public ScriptValue {
  public:
	ScriptInt(int v) : value(v) {}
	virtual ScriptType type() const { return SCRIPT_INT; }
	virtual String typeName() const { return _TYPE_("integer"); }
	virtual String toString() const { return String() << value; }
	virtual double toDouble() const { return value; }
	virtual int    toInt()    const { return value; }
  protected:
#ifdef USE_POOL_ALLOCATOR
	virtual void destroy() {
		boost::singleton_pool<ScriptValue, sizeof(ScriptInt)>::free(this);
	}
#endif
  private:
	int value;
};

ScriptValueP to_script(int v) {
#ifdef USE_POOL_ALLOCATOR
	return intrusive(
			new(boost::singleton_pool<ScriptValue, sizeof(ScriptInt)>::malloc())
				ScriptInt(v));
#else
	return intrusive(new ScriptInt(v));
#endif
}

// ----------------------------------------------------------------------------- : Booleans

// Boolean values
class ScriptBool : public ScriptValue {
  public:
	ScriptBool(bool v) : value(v) {}
	virtual ScriptType type() const { return SCRIPT_BOOL; }
	virtual String typeName() const { return _TYPE_("boolean"); }
	virtual String toString() const { return value ? _("true") : _("false"); }
	// bools don't autoconvert to int
	virtual bool   toBool()   const { return value; }
  private:
	bool value;
};

// use integers to represent true/false
/* NOTE: previous versions used ScriptInts as booleans, this gives problems
 * when we use a pool allocator for them, because the pool is destroyed before these globals.
 */
ScriptValueP script_true (new ScriptBool(true));
ScriptValueP script_false(new ScriptBool(false));


// ----------------------------------------------------------------------------- : Doubles

// Double values
class ScriptDouble : public ScriptValue {
  public:
	ScriptDouble(double v) : value(v) {}
	virtual ScriptType type() const { return SCRIPT_DOUBLE; }
	virtual String typeName() const { return _TYPE_("double"); }
	virtual String toString() const { return String() << value; }
	virtual double toDouble() const { return value; }
	virtual int    toInt()    const { return (int)value; } // TODO: do we want this automatic conversion?
  private:
	double value;
};

ScriptValueP to_script(double v) {
	return intrusive(new ScriptDouble(v));
}

// ----------------------------------------------------------------------------- : String type

String quote_string(String const& str) {
	String out;
	out.reserve(str.size() + 2);
	out += _('"');
	FOR_EACH_CONST(c, str) {
		if      (c == _('"') || c == _('\\')) { out += _('\\'); out += c; }
		else if (c == _('\1')) out += _("\\<");
		else if (c == _('\n')) out += _("\\n");
		else if (c == _('\r')) out += _("\\r");
		else if (c == _('\t')) out += _("\\t");
		else out += c;
	}
	out += _('"');
	return out;
}

// String values
class ScriptString : public ScriptValue {
  public:
	ScriptString(const String& v) : value(v) {}
	virtual ScriptType type() const { return SCRIPT_STRING; }
	virtual String typeName() const { return _TYPE_("string") + _(" (\"") + (value.size() < 30 ? value : value.substr(0,30) + _("...")) + _("\")"); }
	virtual String toString() const { return value; }
	virtual String toCode() const {
		return quote_string(value);
	}
	virtual String toFriendlyString() const {
		// assume that the string can contain tags
		// TODO: distinguish between strings with tags and strings without
		return untag(value);
	}
	virtual double toDouble() const {
		double d;
		if (value.ToDouble(&d)) {
			return d;
		} else {
			throw ScriptErrorConversion(value, typeName(), _TYPE_("double"));
		}
	}
	virtual int toInt() const {
		long l;
		if (value.ToLong(&l)) {
			return l;
		} else {
			throw ScriptErrorConversion(value, typeName(), _TYPE_("integer"));
		}
	}
	virtual bool toBool() const {
		if (value == _("yes") || value == _("true")) {
			return true;
		} else if (value == _("no") || value == _("false") || value.empty()) {
			return false;
		} else {
			throw ScriptErrorConversion(value, typeName(), _TYPE_("boolean"));
		}
	}
	virtual AColor toColor() const {
		AColor c = parse_acolor(value);
		if (!c.Ok()) {
			throw ScriptErrorConversion(value, typeName(), _TYPE_("color"));
		}
		return c;
	}
	virtual wxDateTime toDateTime() const {
		wxDateTime date;
		if (!date.ParseDateTime(value.c_str())) {
			throw ScriptErrorConversion(value, typeName(), _TYPE_("date"));
		}
		return date;
	}
	virtual GeneratedImageP toImage() const {
		if (value.empty()) {
			return intrusive(new BlankImage());
		} else {
			return intrusive(new PackagedImage(value));
		}
	}
	virtual int itemCount() const { return (int)value.size(); }
	virtual ScriptValueP getMember(const String& name) const {
		// get member returns characters
		long index;
		if (name.ToLong(&index) && index >= 0 && (size_t)index < value.size()) {
			return to_script(String(1,value[index]));
		} else {
			return delay_error(_ERROR_2_("has no member value", value, name));
		}
	}
  private:
	String value;
};

ScriptValueP to_script(const String& v) {
	return intrusive(new ScriptString(v));
}


// ----------------------------------------------------------------------------- : Color

// AColor values
class ScriptAColor : public ScriptValue {
  public:
	ScriptAColor(const AColor& v) : value(v) {}
	virtual ScriptType type() const { return SCRIPT_COLOR; }
	virtual String typeName() const { return _TYPE_("color"); }
	virtual AColor toColor() const { return value; }
	// colors don't auto convert to int, use to_int to force
	virtual String toString() const {
		return format_acolor(value);
	}
  private:
	AColor value;
};

ScriptValueP to_script(Color v) {
	return intrusive(new ScriptAColor(v));
}
ScriptValueP to_script(AColor v) {
	return intrusive(new ScriptAColor(v));
}


// ----------------------------------------------------------------------------- : DateTime

// wxDateTime values
class ScriptDateTime : public ScriptValue {
  public:
	ScriptDateTime(const wxDateTime& v) : value(v) {}
	virtual ScriptType type() const { return SCRIPT_DATETIME; }
	virtual String typeName() const { return _TYPE_("date"); }
	virtual wxDateTime toDateTime() const { return value; }
	virtual String toString() const {
		return value.Format(_("%Y-%m-%d %H:%M:%S"));
	}
  private:
	wxDateTime value;
};

ScriptValueP to_script(wxDateTime v) {
	return intrusive(new ScriptDateTime(v));
}


// ----------------------------------------------------------------------------- : Nil type

// the nil object
class ScriptNil : public ScriptValue {
  public:
	virtual ScriptType type() const { return SCRIPT_NIL; }
	virtual String typeName() const { return _TYPE_("nil"); }
	virtual String toString() const { return wxEmptyString; }
	virtual String toCode()   const { return _("nil"); }
	virtual double toDouble() const { return 0.0; }
	virtual int    toInt()    const { return 0; }
	virtual bool   toBool()   const { return false; }
	virtual AColor toColor()  const { return AColor(0,0,0,0); }
	virtual GeneratedImageP toImage() const {
		return intrusive(new BlankImage());
	}

  protected:
	virtual ScriptValueP do_eval(Context& ctx, bool) const {
		// nil(input) == input
		return ctx.getVariable(SCRIPT_VAR_input);
	}
};

/// The preallocated nil value
ScriptValueP script_nil(new ScriptNil);

/// Preallocated nil value marked as default
ScriptValueP script_default_nil(new ScriptDefault(script_nil));

// ----------------------------------------------------------------------------- : Collection base

String ScriptCollectionBase::toCode() const {
	String ret = _("[");
	bool first = true;
	ScriptValueP it = makeIterator();
	while (ScriptValueP v = it->next()) {
		if (!first) ret += _(",");
		first = false;
		// todo: include keys
		ret += v->toCode();
	}
	ret += _("]");
	return ret;
}

// ----------------------------------------------------------------------------- : Custom collection

// Iterator over a custom collection
class ScriptCustomCollectionIterator : public ScriptIterator {
  public:	
	ScriptCustomCollectionIterator(ScriptCustomCollectionP col)
		: col(col), pos(0), it(col->key_value.begin()) {}
	virtual ScriptValueP next(ScriptValueP* key_out) {
		if (pos < col->value.size()) {
			if (key_out) *key_out = to_script((int)pos);
			return col->value.at(pos++);
		} else if (it != col->key_value.end()) {
			if (key_out) *key_out = to_script(it->first);
			return (it++)->second;
		} else {
			return ScriptValueP();
		}
	}
  private:
	ScriptCustomCollectionP col;
	size_t pos;
	map<String,ScriptValueP>::const_iterator it;
};

ScriptValueP ScriptCustomCollection::getMember(const String& name) const {
	map<String,ScriptValueP>::const_iterator it = key_value.find(name);
	if (it != key_value.end()) {
		return it->second;
	} else {
		return ScriptValue::getMember(name);
	}
}
ScriptValueP ScriptCustomCollection::getIndex(int index) const {
	if (index >= 0 && (size_t)index < value.size()) {
		return value.at(index);
	} else {
		return ScriptValue::getIndex(index);
	}
}
ScriptValueP ScriptCustomCollection::makeIterator() const {
	return intrusive(new ScriptCustomCollectionIterator(
	           intrusive_from_existing(const_cast<ScriptCustomCollection*>(this))
	       ));
}

// ----------------------------------------------------------------------------- : Concat collection

// Iterator over a concatenated collection
class ScriptConcatCollectionIterator : public ScriptIterator {
  public:	
	ScriptConcatCollectionIterator(const ScriptValueP& itA, const ScriptValueP& itB)
		: itA(itA), itB(itB) {}
	virtual ScriptValueP next(ScriptValueP* key_out) {
		if (itA) {
			ScriptValueP v = itA->next(key_out);
			if (v) return v;
			else   itA = ScriptValueP();
		}
		// TODO: somehow fix up the keys
		return itB->next(key_out);
	}
  private:
	ScriptValueP itA, itB;
};

ScriptValueP ScriptConcatCollection::getMember(const String& name) const {
	ScriptValueP member = a->getMember(name);
	if (member->type() != SCRIPT_ERROR) return member;
	long index;
	int  itemsInA = a->itemCount();
	if (name.ToLong(&index) && index - itemsInA >= 0 && index - itemsInA < b->itemCount()) { 
		// adjust integer index
		return b->getMember(String() << (index - itemsInA));
	} else {
		return b->getMember(name);
	}
}
ScriptValueP ScriptConcatCollection::getIndex(int index) const {
	int itemsInA = a->itemCount();
	if (index < itemsInA) { 
		return a->getIndex(index);
	} else {
		return b->getIndex(index - itemsInA);
	}
}
ScriptValueP ScriptConcatCollection::makeIterator() const {
	return intrusive(new ScriptConcatCollectionIterator(a->makeIterator(), b->makeIterator()));
}

// ----------------------------------------------------------------------------- : Default arguments / closure

ScriptType ScriptClosure::type() const {
	return SCRIPT_FUNCTION;
}
String ScriptClosure::typeName() const {
	return fun->typeName() + _(" closure");
}

void ScriptClosure::addBinding(Variable v, const ScriptValueP& value) {
	bindings.push_back(make_pair(v,value));
}
ScriptValueP ScriptClosure::getBinding(Variable v) const {
	FOR_EACH_CONST(b, bindings) {
		if (b.first == v) return b.second;
	}
	return ScriptValueP();
}

ScriptValueP ScriptClosure::simplify() {
	return fun->simplifyClosure(*this);
}

ScriptValueP ScriptClosure::do_eval(Context& ctx, bool openScope) const {
	scoped_ptr<LocalScope> scope(openScope ? new LocalScope(ctx) : nullptr);
	applyBindings(ctx);
	return fun->eval(ctx, openScope);
}
ScriptValueP ScriptClosure::dependencies(Context& ctx, const Dependency& dep) const {
	LocalScope scope(ctx);
	applyBindings(ctx);
	return fun->dependencies(ctx, dep);
}
void ScriptClosure::applyBindings(Context& ctx) const {
	FOR_EACH_CONST(b, bindings) {
		if (ctx.getVariableScope(b.first) != 0) {
			// variables passed as arguments (i.e. in scope 0) override these default bindings
			ctx.setVariable(b.first, b.second);
		}
	}
}


ScriptType ScriptRule::type() const { return SCRIPT_FUNCTION; }
String ScriptRule::typeName() const { return fun->typeName() + _(" rule"); }
ScriptValueP ScriptRule::do_eval(Context& ctx, bool openScope) const {
	return ctx.makeClosure(fun);
}
