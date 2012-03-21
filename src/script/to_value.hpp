//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#ifndef HEADER_SCRIPT_TO_VALUE
#define HEADER_SCRIPT_TO_VALUE

// ----------------------------------------------------------------------------- : Includes

#include <script/value.hpp>
#include <script/script.hpp>
#include <script/profiler.hpp>
#include <util/reflect.hpp>
#include <util/error.hpp>
#include <util/io/get_member.hpp>
#include <gfx/generated_image.hpp> // we need the dtor of GeneratedImage
#if USE_SCRIPT_PROFILING
	#include <typeinfo>
#endif

// ----------------------------------------------------------------------------- : Overloadable templates

/// Number of items in some collection like object, can be overloaded
template <typename T>
int item_count(const T& v) {
	return -1;
}
/// Return an iterator for some collection, can be overloaded
template <typename T>
ScriptValueP make_iterator(const T& v) {
	return ScriptValueP();
}

/// Mark a dependency on a member of value, can be overloaded
template <typename T>
void mark_dependency_member(const T& value, const String& name, const Dependency& dep) {}

/// Mark a dependency on an object, can be overloaded
template <typename T>
void mark_dependency_value(const T& value, const Dependency& dep) {}

/// Type name of an object, for error messages
template <typename T> inline String type_name(const T&) {
	return _TYPE_("object");
}
template <typename T> inline String type_name(const intrusive_ptr<T>& p) {
	return type_name(*p.get());
}
template <typename K, typename V> inline String type_name(const pair<K,V>& p) {
	return type_name(p.second); // for maps
}

/// Script code for an object, or anything else we can show
template <typename T> inline String to_code(const T& v) {
	return format_string(_("<%s>"),type_name(v));
}
template <typename T> inline String to_code(const intrusive_ptr<T>& p) {
	return type_name(*p.get());
}

// ----------------------------------------------------------------------------- : Errors

/// A delayed error message.
/** Only when trying to use the object will the error be thrown.
 *  This can be 'caught' by the "or else" construct
 */
class ScriptDelayedError : public ScriptValue {
  public:
	inline ScriptDelayedError(const ScriptError& error) : error(error) {}

	virtual ScriptType type() const;// { return SCRIPT_ERROR; }

	// all of these throw
	virtual String typeName() const;
	virtual String toString() const;
	virtual double toDouble() const;
	virtual int    toInt()    const;
	virtual bool   toBool()   const;
	virtual AColor toColor()  const;
	virtual int itemCount() const;
	virtual CompareWhat compareAs(String&, void const*&) const;
	// these can propagate the error
	virtual ScriptValueP getMember(const String& name) const;
	virtual ScriptValueP dependencyMember(const String& name, const Dependency&) const;
	virtual ScriptValueP dependencies(Context&, const Dependency&) const;
	virtual ScriptValueP makeIterator() const;

  protected:
	virtual ScriptValueP do_eval(Context&, bool openScope) const;

  private:
	ScriptError error; // the error message
};

inline ScriptValueP delay_error(const String& m) {
	return intrusive(new ScriptDelayedError(ScriptError(m)));
}
inline ScriptValueP delay_error(const ScriptError& error) {
	return intrusive(new ScriptDelayedError(error));
}

// ----------------------------------------------------------------------------- : Marked ('default') values

/// A script value that is in a specially marked 'default' state.
class ScriptDefault : public ScriptValue {
  public:
	inline ScriptDefault(ScriptValueP const& un_default) : un_default(un_default) {}
	
	virtual ScriptType type() const { return un_default->type(); }
	virtual String typeName() const { return un_default->typeName(); }
	virtual String toString() const { return un_default->toString(); }
	virtual String toFriendlyString() const { return un_default->toFriendlyString(); }
	virtual String toCode()   const { return _("mark_default(") + un_default->toCode() + _(")"); }
	virtual double toDouble() const { return un_default->toDouble(); }
	virtual int    toInt()    const { return un_default->toInt(); }
	virtual bool   toBool()   const { return un_default->toBool(); }
	virtual AColor toColor()  const { return un_default->toColor(); }
	virtual GeneratedImageP toImage() const { return un_default->toImage(); }
	// ignore defaultness for comparison
	virtual CompareWhat compareAs(String& compare_str, void const*& compare_ptr) const { return un_default->compareAs(compare_str, compare_ptr); }
	
	/// The actual value
	ScriptValueP un_default;
};
inline ScriptDefault const* is_default(ScriptValue const* x) {
	return dynamic_cast<ScriptDefault const*>(x);
}
inline ScriptDefault const* is_default(ScriptValueP const& x) {
	return is_default(x.get());
}
inline ScriptValueP make_default(ScriptValueP const& x) {
	return is_default(x) ? x : intrusive_ptr<ScriptValue>(new ScriptDefault(x));
}
inline ScriptValueP with_defaultness_of(ScriptValueP const& of, ScriptValueP const& x) {
	return is_default(of) ? make_default(x) : x;
}
extern ScriptValueP script_default_nil;

/*
// ----------------------------------------------------------------------------- : File names

class Packaged;

/// A filename internal to a package (usually the set)
/// Filenames are essentially arbitrary, they just give a unique id for a file
class ScriptLocalFileName : public ScriptValue {
  public:
	enum Type { IMAGE, SYMBOL };
	inline ScriptLocalFileName(Packaged* package, String const& fn, Type file_type = IMAGE) : package(package), fn(fn), file_type(file_type) {}
	
	virtual ScriptType type() const { return SCRIPT_FILENAME; }
	virtual String typeName() const { return _("filename"); }
	// TODO: need to add a "to_image" function
	
	Packaged* package; //< Package the file is in
	String    fn; //< The filename
	Type      file_type;
};
*/

// ----------------------------------------------------------------------------- : Iterators

// Iterator over a collection
struct ScriptIterator : public ScriptValue {
	virtual ScriptType type() const;// { return SCRIPT_ITERATOR; }
	virtual String typeName() const;// { return "iterator"; }
	virtual CompareWhat compareAs(String&, void const*&) const; // { return COMPARE_NO; }

	/// Return the next item for this iterator, or ScriptValueP() if there is no such item
	virtual ScriptValueP next(ScriptValueP* key_out = nullptr) = 0;
	virtual ScriptValueP makeIterator() const;
};

// make an iterator over a range
ScriptValueP rangeIterator(int start, int end);

// ----------------------------------------------------------------------------- : Collections

ScriptValueP to_script(int);

class ScriptCollectionBase : public ScriptValue {
  public:
	virtual ScriptType type() const { return SCRIPT_COLLECTION; }
	virtual String typeName() const { return _TYPE_("collection"); }
	virtual String toCode() const;
};

// Iterator over a collection
template <typename Collection>
class ScriptCollectionIterator : public ScriptIterator {
  public:
	ScriptCollectionIterator(const Collection* col) : pos(0), col(col) {}
	virtual ScriptValueP next(ScriptValueP* key_out) {
		if (pos < col->size()) {
			if (key_out) *key_out = to_script((int)pos);
			return to_script(col->at(pos++));
		} else {
			return ScriptValueP();
		}
	}
  private:
	size_t pos;
	const Collection* col;
};

/// Script value containing a collection
template <typename Collection>
class ScriptCollection : public ScriptCollectionBase {
  public:
	inline ScriptCollection(const Collection* v) : value(v) {}
	virtual String typeName() const { return _TYPE_1_("collection of", type_name(*value->begin())); }
	virtual ScriptValueP getIndex(int index) const {
		if (index >= 0 && index < (int)value->size()) {
			return to_script(value->at(index));
		} else {
			return ScriptValue::getIndex(index);
		}
	}
	virtual ScriptValueP makeIterator() const {
		return intrusive(new ScriptCollectionIterator<Collection>(value));
	}
	virtual int itemCount() const { return (int)value->size(); }
	/// Collections can be compared by comparing pointers
	virtual CompareWhat compareAs(String&, void const*& compare_ptr) const {
		compare_ptr = value;
		return COMPARE_AS_POINTER;
	}
  private:
	/// Store a pointer to a collection, collections are only ever used for structures owned outside the script
	const Collection* value;
};

// ----------------------------------------------------------------------------- : Collections : maps

template <typename V>
ScriptValueP get_member(const map<String,V>& m, const String& name) {
	typename map<String,V>::const_iterator it = m.find(name);
	if (it != m.end()) {
		return to_script(it->second);
	} else {
		return delay_error(ScriptErrorNoMember(_TYPE_("collection"), name));
	}
}

template <typename K, typename V>
ScriptValueP get_member(const IndexMap<K,V>& m, const String& name) {
	typename IndexMap<K,V>::const_iterator it = m.find(name);
	if (it != m.end()) {
		return to_script(*it);
	} else {
		return delay_error(ScriptErrorNoMember(_TYPE_("collection"), name));
	}
}

/// Script value containing a map-like collection
template <typename Collection>
class ScriptMap : public ScriptValue {
  public:
	inline ScriptMap(const Collection* v) : value(v) {}
	virtual ScriptType type() const { return SCRIPT_COLLECTION; }
	virtual String typeName() const { return _TYPE_1_("collection of", type_name(value->begin())); }
	virtual ScriptValueP getMember(const String& name) const {
		return get_member(*value, name);
	}
	virtual int itemCount() const { return (int)value->size(); }
	virtual ScriptValueP dependencyMember(const String& name, const Dependency& dep) const {
		mark_dependency_member(*value, name, dep);
		return getMember(name);
	}
	/// Collections can be compared by comparing pointers
	virtual CompareWhat compareAs(String&, void const*& compare_ptr) const {
		compare_ptr = value;
		return COMPARE_AS_POINTER;
	}
  private:
	/// Store a pointer to a collection, collections are only ever used for structures owned outside the script
	const Collection* value;
};

// ----------------------------------------------------------------------------- : Collections : from script

/// Script value containing a custom collection, returned from script functions
class ScriptCustomCollection : public ScriptCollectionBase {
  public:
	virtual ScriptValueP getMember(const String& name) const;
	virtual ScriptValueP getIndex(int index) const;
	virtual ScriptValueP makeIterator() const;
	virtual int itemCount() const { return (int)value.size(); }
	/// Collections can be compared by comparing pointers
	virtual CompareWhat compareAs(String&, void const*& compare_ptr) const {
		compare_ptr = this;
		return COMPARE_AS_POINTER;
	}

	/// The collection as a list (contains only the values that don't have a key)
	vector<ScriptValueP> value;
	/// The collection as a map (contains only the values that have a key)
	map<String,ScriptValueP> key_value;
};

DECLARE_POINTER_TYPE(ScriptCustomCollection);

// ----------------------------------------------------------------------------- : Collections : concatenation

/// Script value containing the concatenation of two collections
class ScriptConcatCollection : public ScriptCollectionBase {
  public:
	inline ScriptConcatCollection(ScriptValueP a, ScriptValueP b) : a(a), b(b) {}
	virtual ScriptValueP getMember(const String& name) const;
	virtual ScriptValueP getIndex(int index) const;
	virtual ScriptValueP makeIterator() const;
	virtual int itemCount() const { return a->itemCount() + b->itemCount(); }
	/// Collections can be compared by comparing pointers
	virtual CompareWhat compareAs(String&, void const*& compare_ptr) const {
		compare_ptr = this;
		return COMPARE_AS_POINTER;
	}

  private:
	ScriptValueP a,b;
	friend class ScriptConcatCollectionIterator;
};

// ----------------------------------------------------------------------------- : Objects

/// Script value containing an object (pointer)
template <typename T>
class ScriptObject : public ScriptValue {
  public:
	inline ScriptObject(const T& v) : value(v) {}
	virtual ScriptType type() const { ScriptValueP d = getDefault(); return d ? d->type() : SCRIPT_OBJECT; }
	virtual String typeName() const { return type_name(*value); }
	virtual String toString() const { ScriptValueP d = getDefault(); return d ? d->toString() : ScriptValue::toString(); }
	virtual int    toInt()    const { ScriptValueP d = getDefault(); return d ? d->toInt()    : ScriptValue::toInt(); }
	virtual double toDouble() const { ScriptValueP d = getDefault(); return d ? d->toDouble() : ScriptValue::toDouble(); }
	virtual bool   toBool()   const { ScriptValueP d = getDefault(); return d ? d->toBool()   : ScriptValue::toBool(); }
	virtual AColor toColor()  const { ScriptValueP d = getDefault(); return d ? d->toColor()  : ScriptValue::toColor(); }
	virtual String toCode()   const { ScriptValueP d = getDefault(); return d ? d->toCode()   : to_code(*value); }
	virtual GeneratedImageP toImage() const {
		ScriptValueP d = getDefault(); return d ? d->toImage() : ScriptValue::toImage();
	}
	virtual ScriptValueP getMember(const String& name) const {
		#if USE_SCRIPT_PROFILING
			Timer t;
			Profiler prof(t, (void*)mangled_name(typeid(T)), _("get member of ") + type_name(*value));
		#endif
		GetMember gm(name);
		gm.handle(*value);
		if (gm.result()) return gm.result();
		else {
			// try nameless member
			ScriptValueP d = getDefault();
			if (d) {
				return d->getMember(name);
			} else {
				return ScriptValue::getMember(name);
			}
		}
	}
	virtual ScriptValueP getIndex(int index) const { ScriptValueP d = getDefault(); return d ? d->getIndex(index) : ScriptValue::getIndex(index); }
	virtual ScriptValueP dependencyMember(const String& name, const Dependency& dep) const {
		mark_dependency_member(*value, name, dep);
		return getMember(name);
	}
	virtual void dependencyThis(const Dependency& dep) {
		mark_dependency_value(*value, dep);
	}
	virtual ScriptValueP makeIterator() const {
		ScriptValueP it = make_iterator(*value);
		if (it) return it;
		ScriptValueP d = getDefault();
		if (d) return d->makeIterator();
		return ScriptValue::makeIterator();
	}
	virtual int itemCount() const {
		int i = item_count(*value);
		if (i >= 0) return i;
		ScriptValueP d = getDefault();
		if (d) return d->itemCount();
		return ScriptValue::itemCount();
	}
	/// Objects can be compared by comparing pointers
	virtual CompareWhat compareAs(String& compare_str, void const*& compare_ptr) const {
		ScriptValueP d = getDefault();
		if (d) {
			return d->compareAs(compare_str, compare_ptr);
		} else {
			compare_ptr = &*value;
			return COMPARE_AS_POINTER;
		}
	}
	/// Get access to the value
	inline T getValue() const { return value; }
  private:
	T value; ///< The object
	ScriptValueP getDefault() const {
		GetDefaultMember gdm;
		gdm.handle(*value);
		return gdm.result();
	}
};

// ----------------------------------------------------------------------------- : Default arguments / closure

/// A wrapper around a function that gives default arguments
class ScriptClosure : public ScriptValue {
  public:
	ScriptClosure(ScriptValueP fun) : fun(fun) {}

	virtual ScriptType type() const;
	virtual String typeName() const;
	virtual ScriptValueP dependencies(Context& ctx, const Dependency& dep) const;

	/// Add a binding
	void addBinding(Variable v, const ScriptValueP& value);
	/// Is there a binding for the given variable? If so, retrieve it
	ScriptValueP getBinding(Variable v) const;

	/// Try to simplify this closure, returns a value if successful
	ScriptValueP simplify();

	/// The wrapped function
	ScriptValueP                          fun;
	/// The default argument bindings
	vector<pair<Variable,ScriptValueP> >  bindings;

  protected:
	virtual ScriptValueP do_eval(Context& ctx, bool openScope) const;

  private:
	/// Apply the bindings in a context
	void applyBindings(Context& ctx) const;
};

/// Turn a script function into a rule, a.k.a. a delayed closure
class ScriptRule : public ScriptValue {
  public:
	inline ScriptRule(const ScriptValueP& fun) : fun(fun) {}
	virtual ScriptType type() const;
	virtual String typeName() const;

  protected:
	virtual ScriptValueP do_eval(Context& ctx, bool openScope) const;

  private:
	ScriptValueP fun;
};

// ----------------------------------------------------------------------------- : Creating

/// Convert a value to a script value
       ScriptValueP to_script(int           v);
inline ScriptValueP to_script(long          v) { return to_script((int) v); }
       ScriptValueP to_script(double        v);
       ScriptValueP to_script(const String& v);
       ScriptValueP to_script(Color         v);
       ScriptValueP to_script(AColor        v);
       ScriptValueP to_script(wxDateTime    v);
inline ScriptValueP to_script(bool          v) { return v ? script_true : script_false; }
inline ScriptValueP to_script(ScriptValueP const& v) { return v; }
template <typename T>
inline ScriptValueP to_script(const vector<T>*     v) { return intrusive(new ScriptCollection<vector<T> >(v)); }
template <typename K, typename V>
inline ScriptValueP to_script(const map<K,V>*      v) { return intrusive(new ScriptMap<map<K,V> >(v)); }
template <typename K, typename V>
inline ScriptValueP to_script(const IndexMap<K,V>* v) { return intrusive(new ScriptMap<IndexMap<K,V> >(v)); }
template <typename T>
inline ScriptValueP to_script(const intrusive_ptr<T>& v) { return intrusive(new ScriptObject<intrusive_ptr<T> >(v)); }
template <typename T>
inline ScriptValueP to_script(const Defaultable<T>& v) { return to_script(v()); }

// ----------------------------------------------------------------------------- : Destructing

/// Convert a value from a script value to a normal value
template <typename T> inline T  from_script              (const ScriptValueP& value) {
	ScriptObject<T>* o = dynamic_cast<ScriptObject<T>*>(value.get());
	if (!o) {
		throw ScriptErrorConversion(value->typeName(), _TYPE_("object" ));
	}
	return o->getValue();
}
template <> inline ScriptValueP from_script<ScriptValueP>(const ScriptValueP& value) { return value;  }
template <> inline String       from_script<String>      (const ScriptValueP& value) { return value->toString(); }
template <> inline int          from_script<int>         (const ScriptValueP& value) { return value->toInt(); }
template <> inline double       from_script<double>      (const ScriptValueP& value) { return value->toDouble(); }
template <> inline bool         from_script<bool>        (const ScriptValueP& value) { return value->toBool(); }
template <> inline Color        from_script<Color>       (const ScriptValueP& value) { return value->toColor(); }
template <> inline AColor       from_script<AColor>      (const ScriptValueP& value) { return value->toColor(); }
template <> inline wxDateTime   from_script<wxDateTime>  (const ScriptValueP& value) { return value->toDateTime(); }
template <> inline GeneratedImageP from_script<GeneratedImageP>(const ScriptValueP& value) { return value->toImage(); }

// ----------------------------------------------------------------------------- : EOF
#endif
