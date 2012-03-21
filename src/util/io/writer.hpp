//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#ifndef HEADER_UTIL_IO_WRITER
#define HEADER_UTIL_IO_WRITER

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <wx/txtstrm.h>

template <typename T> class Defaultable;
template <typename T> class Scriptable;
DECLARE_POINTER_TYPE(Game);
DECLARE_POINTER_TYPE(StyleSheet);

// ----------------------------------------------------------------------------- : Writer

/// The Writer can be used for writing (serializing) objects
class Writer {
  public:
	/// Construct a writer that writes to the given output stream
	Writer(wxOutputStream& output, Version file_app_version);
	
	/// Tell the reflection code we are not reading
	inline bool isReading() const { return false; }
	inline bool isWriting() const { return true; }
	inline bool isCompound() const { return true; }
	inline Version formatVersion() const { return app_version; }
	
	// --------------------------------------------------- : Handling objects
	/// Handle an object: write it under the given name
	template <typename T>
	void handle(const Char* name, const T& object) {
		enterBlock(name);
		handle(object);
		exitBlock();
	}
	/// Handle a value
	template <typename T>
	inline void handleNoScript(const Char* name, T& value) { handle(name,value); }
	
	/// Write a vector to the output stream
	template <typename T>
	void handle(const Char* name, const vector<T>& vector);
	
	/// Write a string to the output stream
	void handle(const String& str);
	void handle(const Char* str) { handle(String(str)); }
	
	/// Write an object of type T to the output stream
	template <typename T> void handle(const T&);
	/// Write a intrusive_ptr to the output stream
	template <typename T> void handle(const intrusive_ptr<T>&);
	/// Write a map to the output stream
	template <typename K, typename V> void handle(const map<K,V>&);
	/// Write an IndexMap to the output stream
	template <typename K, typename V> void handle(const IndexMap<K,V>&);
	template <typename K, typename V> void handle(const DelayedIndexMaps<K,V>&);
	template <typename K, typename V> void handle(const DelayedIndexMapsData<K,V>&);
	template <typename T> void handle(const Defaultable<T>&);
	template <typename T> void handle(const Scriptable<T>&);
	void handle(const ScriptValueP&);
	// special behaviour
	void handle(const GameP&);
	void handle(const StyleSheetP&);
	
  private:
	// --------------------------------------------------- : Data
	/// Indentation of the current block
	int indentation;
	/// Blocks opened to which nothing has been written
	vector<const Char*> pending_opened;
	
	/// Text stream wrapping the output stream we are writing to
	wxTextOutputStream stream;
	
	// --------------------------------------------------- : Writing to the stream
	
	/// Start a new block with the given name
	void enterBlock(const Char* name);
	/// Leave the block we are in
	void exitBlock();
	
	/// Write the pending_opened with the required indentation
	void writePending();
	/// Output some taps to represent the indentation level
	void writeIndentation();
};

// ----------------------------------------------------------------------------- : Container types

template <typename T>
void Writer::handle(const Char* name, const vector<T>& vec) {
	String vectorKey = singular_form(name);
	for (typename vector<T>::const_iterator it = vec.begin() ; it != vec.end() ; ++it) {
		handle(vectorKey, *it);
	}
}

template <typename T>
void Writer::handle(const intrusive_ptr<T>& pointer) {
	if (pointer) handle(*pointer);
}

template <typename K, typename V>
void Writer::handle(const map<K,V>& m) {
	for (typename map<K,V>::const_iterator it = m.begin() ; it != m.end() ; ++it) {
		handle(it->first.c_str(), it->second);
	}
}

template <typename K, typename V>
void Writer::handle(const IndexMap<K,V>& m) {
	for (typename IndexMap<K,V>::const_iterator it = m.begin() ; it != m.end() ; ++it) {
		handle(get_key_name(*it).c_str(), *it);
	}
}


// ----------------------------------------------------------------------------- : Reflection

/// Implement reflection as used by Writer
#define REFLECT_OBJECT_WRITER(Cls)								\
	template<> void Writer::handle<Cls>(const Cls& object) {	\
		const_cast<Cls&>(object).reflect(*this);				\
	}															\
	void Cls::reflect(Writer& writer) {							\
		reflect_impl(writer);									\
	}

// ----------------------------------------------------------------------------- : Reflection for enumerations

/// Implement enum reflection as used by Writer
#define REFLECT_ENUM_WRITER(Enum)								\
	template<> void Writer::handle<Enum>(const Enum& enum_) {	\
		EnumWriter writer(*this);								\
		reflect_ ## Enum(const_cast<Enum&>(enum_), writer);		\
	}

/// 'Tag' to be used when reflecting enumerations for Writer
class EnumWriter {
  public:
	inline EnumWriter(Writer& writer)
		: writer(writer) {}
	
	/// Handle a possible value for the enum, if the name matches the name in the input
	template <typename Enum>
	inline void handle(const Char* name, Enum value, Enum enum_) {
		if (enum_ == value) {
			writer.handle(name);
		}
	}
	
  private:
	Writer& writer;  ///< The writer to write output to
};

// ----------------------------------------------------------------------------- : EOF
#endif
