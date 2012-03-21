//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#ifndef HEADER_DATA_EXPORT_TEMPLATE
#define HEADER_DATA_EXPORT_TEMPLATE

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/io/package.hpp>
#include <script/scriptable.hpp>

DECLARE_POINTER_TYPE(Game);
DECLARE_POINTER_TYPE(Set);
DECLARE_POINTER_TYPE(Field);
DECLARE_POINTER_TYPE(Style);
DECLARE_POINTER_TYPE(ExportTemplate);
DECLARE_POINTER_TYPE(Package);

// ----------------------------------------------------------------------------- : ExportTemplate

/// A template for exporting sets to HTML or text format
class ExportTemplate : public Packaged {
  public:
	ExportTemplate();
	
	GameP                   game;				///< Game this template is for
	String                  file_type;			///< Type of the created file, in "name|*.ext" format
	bool                    create_directory;	///< The export creates a directory for additional data files
	vector<FieldP>          option_fields;		///< Options for exporting
	IndexMap<FieldP,StyleP> option_style;		///< Style of the options
	OptionalScript          script;				///< Export script, for multi file templates and initialization
	
	static String typeNameStatic();
	virtual String typeName() const;
	Version fileVersion() const;
	virtual void validate(Version = app_version);
	
	/// Loads the export template with a particular name
	static ExportTemplateP byName(const String& name);
	
  private:
	DECLARE_REFLECTION();
};

// ----------------------------------------------------------------------------- : ExportInfo

/// Information that can be used by export functions
struct ExportInfo {
	ExportInfo();
	
	SetP               set;                ///< The set that is being exported
	PackageP           export_template;    ///< The export template used
	                                       ///  When using the CLI, this can be a fake package to allow reading from the cwd
	String             directory_relative; ///< The directory for storing extra files (or "" if !export->create_directory)
	                                       ///  This is just the directory name
	String             directory_absolute; ///< The absolute path of the directory
	map<String,wxSize> exported_images;	   ///< Images (from symbol font) already exported, and their size
	bool               allow_writes_outside; ///< Can files outside the directory be written to?
};

DECLARE_DYNAMIC_ARG(ExportInfo*, export_info);

// ----------------------------------------------------------------------------- : EOF
#endif
