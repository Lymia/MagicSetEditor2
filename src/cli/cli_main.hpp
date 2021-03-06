//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2017 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

#ifndef HEADER_CLI_MAIN
#define HEADER_CLI_MAIN

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <data/set.hpp>
#include <data/export_template.hpp>
#include <script/profiler.hpp>

// ----------------------------------------------------------------------------- : Command line interface

class CLISetInterface : public SetView {
  public:
  /// The set is optional
  CLISetInterface(const SetP& set, bool quiet = false);
  ~CLISetInterface();
  protected:
  virtual void onAction(const Action&, bool) {}
  virtual void onChangeSet();
  virtual void onBeforeChangeSet();
  private:
  bool quiet;    ///< Supress prompts and other non-vital stuff
  bool running;  ///< Still running?
  
  void run();
  void showWelcome();
  void showUsage();
  void handleCommand(const String& command);
  #if USE_SCRIPT_PROFILING
    void showProfilingStats(const FunctionProfile& parent, int level = 0);
  #endif
  void print_pending_errors();
  
  /// our own context, when no set is loaded
  Context& getContext();
  Context* our_context;
  size_t scope;
  
  // export info, so we can write files
  ExportInfo ei;
  void setExportInfoCwd();
};

// ----------------------------------------------------------------------------- : EOF
#endif
