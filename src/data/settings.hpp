//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#ifndef HEADER_DATA_SETTINGS
#define HEADER_DATA_SETTINGS

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/reflect.hpp>
#include <util/defaultable.hpp>
#include <util/angle.hpp>

class Game;
class StyleSheet;
class ExportTemplate;
class Field;

DECLARE_POINTER_TYPE(GameSettings);
DECLARE_POINTER_TYPE(StyleSheetSettings);
DECLARE_POINTER_TYPE(Field);
DECLARE_POINTER_TYPE(Value);
DECLARE_POINTER_TYPE(AutoReplace);

// For now, use the old style update checker
#define USE_OLD_STYLE_UPDATE_CHECKER 1

// ----------------------------------------------------------------------------- : Extra data structures

/// When to check for updates?
enum CheckUpdates
{	CHECK_ALWAYS
,	CHECK_IF_CONNECTED
,	CHECK_NEVER
};

/// Where to install to?
enum InstallType
{	INSTALL_DEFAULT	// the platform default.
,	INSTALL_LOCAL	// install to the user's files
,	INSTALL_GLOBAL	// install to the global files
};

void parse_enum(const String&, InstallType&);
bool is_install_local(InstallType type);

/// How to handle filename conflicts
enum FilenameConflicts
{	CONFLICT_KEEP_OLD			// always keep old file
,	CONFLICT_OVERWRITE			// always overwrite
,	CONFLICT_NUMBER				// always add numbers ("file.1.something")
,	CONFLICT_NUMBER_OVERWRITE	// only add numbers for conflicts inside a set, overwrite old stuff
};

/// Settings of a single column in the card list
class ColumnSettings {
  public:
	ColumnSettings();
	UInt width;
	int  position;
	bool visible;
	
	DECLARE_REFLECTION();
};

/// Settings for a Game
class GameSettings : public IntrusivePtrBase<GameSettings> {
  public:
	GameSettings();
	
	/// Where the settings have defaults, initialize with the values from the game
	void initDefaults(const Game& g);
	
	String                      default_stylesheet;
	String                      default_export;
	map<String, ColumnSettings> cardlist_columns;
	String                      sort_cards_by;
	bool                        sort_cards_ascending;
	String                      images_export_filename;
	FilenameConflicts           images_export_conflicts;
	bool                        use_auto_replace;
	vector<AutoReplaceP>        auto_replaces;     ///< Things to autoreplace in textboxes
	map<String, int>            pack_amounts;
	bool                        pack_seed_random;
	int                         pack_seed;
	
	DECLARE_REFLECTION();
  private:
	bool initialized;
};

/// Settings for a StyleSheet
class StyleSheetSettings : public IntrusivePtrBase<StyleSheetSettings> {
  public:
	StyleSheetSettings();
	
	// Rendering/display settings
	Defaultable<double> card_zoom;
	Defaultable<Degrees> card_angle;
	Defaultable<bool>   card_anti_alias;
	Defaultable<bool>   card_borders;
	Defaultable<bool>   card_draw_editing;
	Defaultable<bool>   card_normal_export;
	Defaultable<bool>   card_spellcheck_enabled;
	
	/// Where the settings are the default, use the value from ss
	void useDefault(const StyleSheetSettings& ss);
	
	DECLARE_REFLECTION();
};

// ----------------------------------------------------------------------------- : Printing settings

enum PageLayoutType
{	LAYOUT_NO_SPACE
,	LAYOUT_EQUAL_SPACE
//,	LAYOUT_CUSTOM
};

// ----------------------------------------------------------------------------- : Settings

/// Class that holds MSE settings.
/** There is a single global instance of this class.
 *  Settings are loaded at startup, and stored at shutdown.
 */
class Settings {
  public:
	/// Default constructor initializes default settings
	Settings();
	
	// --------------------------------------------------- : Locale
	
	String locale;
	
	// --------------------------------------------------- : Recently opened sets
	vector<String> recent_sets;
	static const UInt max_recent_sets = 9; // store this many recent sets
	
	/// Add a file to the list of recent files
	void addRecentFile(const String& filename);
	
	// --------------------------------------------------- : Files/directories
	String default_set_dir;    ///< Where to look for .mse-set files
	String default_image_dir;  ///< Where to look for images to import
	String default_symbol_dir; ///< Where to look for .mse-symbol files
	String default_export_dir; ///< Where to export to by default
	
	// --------------------------------------------------- : Set window
	bool set_window_maximized;
	UInt set_window_width;
	UInt set_window_height;
	UInt card_notes_height;
	bool open_sets_in_new_window;
	
	// --------------------------------------------------- : Symbol editor
	UInt symbol_grid_size;
	bool symbol_grid;
	bool symbol_grid_snap;
	
	// --------------------------------------------------- : Default pacakge selections
	String default_game;
	
	// --------------------------------------------------- : Game/stylesheet specific
	
	/// Get the settings object for a specific game
	GameSettings&       gameSettingsFor      (const Game& game);
	/// Get the settings for a column for a specific field in a game
	ColumnSettings&     columnSettingsFor    (const Game& game, const Field& field);
	/// Get the settings object for a specific stylesheet
	StyleSheetSettings& stylesheetSettingsFor(const StyleSheet& stylesheet);
	
  private:
	map<String,GameSettingsP>       game_settings;
	map<String,StyleSheetSettingsP> stylesheet_settings;
  public:
	StyleSheetSettings              default_stylesheet_settings;	///< The default settings for stylesheets
	
	// --------------------------------------------------- : Exports
  private:
	DelayedIndexMaps<FieldP,ValueP> export_options;
  public:
	
	/// Get the options for an export template
	IndexMap<FieldP,ValueP>& exportOptionsFor(const ExportTemplate& export_template);
	
	// --------------------------------------------------- : Printing
	
	PageLayoutType print_layout;
	
	// --------------------------------------------------- : Special game stuff
	String apprentice_location;
	
	// --------------------------------------------------- : Update checking
	#if USE_OLD_STYLE_UPDATE_CHECKER
		String updates_url;
	#endif
	String package_versions_url; ///< latest package versions
	String installer_list_url;   ///< available installers
	CheckUpdates check_updates;
	bool   check_updates_all; ///< Check updates of all packages, not just the program
	String website_url;
	
	// --------------------------------------------------- : Installation settings
	InstallType install_type;
	
	// --------------------------------------------------- : The io
	
	/// Read the settings file from the standard location
	void read();
	/// Store the settings in the standard location
	void write();
	
  private:
	/// Name of the settings file
	String settingsFile();
	/// Clear settings before reading them
	void clear();
	
	DECLARE_REFLECTION();
};

/// The global settings object
extern Settings settings;

// ----------------------------------------------------------------------------- : EOF
#endif
