//+----------------------------------------------------------------------------+
//| Description:  Magic Set Editor - Program to make Magic (tm) cards          |
//| Copyright:    (C) 2001 - 2012 Twan van Laarhoven and Sean Hunt             |
//| License:      GNU General Public License 2 or later (see file COPYING)     |
//+----------------------------------------------------------------------------+

// ----------------------------------------------------------------------------- : Includes

#include <util/prec.hpp>
#include <util/error.hpp>
#include <cli/cli_main.hpp>
#include <cli/text_io_handler.hpp>
#include <script/functions/functions.hpp>
#include <script/profiler.hpp>
#include <data/format/formats.hpp>
#include <wx/process.h>
#include <wx/wfstream.h>

String read_utf8_line(wxInputStream& input, bool eat_bom = true, bool until_eof = false);

DECLARE_TYPEOF_COLLECTION(ScriptParseError);

// ----------------------------------------------------------------------------- : Command line interface

CLISetInterface::CLISetInterface(const SetP& set, bool quiet)
	: quiet(quiet)
	, our_context(nullptr)
{
	if (!cli.haveConsole()) {
		throw Error(_("Can not run command line interface without a console;\nstart MSE with \"mse.com --cli\""));
	}
	ei.allow_writes_outside = true;
	setExportInfoCwd();
	setSet(set);
	// show welcome logo
	if (!quiet) showWelcome();
	cli.print_pending_errors();
}

CLISetInterface::~CLISetInterface() {
	delete our_context;
}

Context& CLISetInterface::getContext() {
	if (set) {
		return set->getContext();
	} else {
		if (!our_context) {
			our_context = new Context();
			init_script_functions(*our_context);
		}
		return *our_context;
	}
}

void CLISetInterface::onBeforeChangeSet() {
	if (set || our_context) {
		Context& ctx = getContext();
		ctx.closeScope(scope);
	}
}

void CLISetInterface::onChangeSet() {
	Context& ctx = getContext();
	scope = ctx.openScope();
	ei.set = set;
}

void CLISetInterface::setExportInfoCwd() {
	// write to the current directory
	ei.directory_relative = ei.directory_absolute = wxGetCwd();
	// read from the current directory
	ei.export_template = intrusive(new Package());
	ei.export_template->open(ei.directory_absolute, true);
}


// ----------------------------------------------------------------------------- : Running

void CLISetInterface::run_interactive() {
	// loop
	running = true;
	while (running) {
		// show prompt
		if (!quiet) {
			cli << GRAY << _("> ") << NORMAL;
			cli.flush();
		}
		// read line from stdin
		String command = cli.getLine();
		if (command.empty() && !cli.canGetLine()) break;
		handleCommand(command);
		cli.print_pending_errors();
		cli.flush();
		cli.flushRaw();
	}
}

bool CLISetInterface::run_script(ScriptP const& script) {
	try {
		WITH_DYNAMIC_ARG(export_info, &ei);
		Context& ctx = getContext();
		ScriptValueP result = ctx.eval(*script,false);
		// show result (?)
		cli << result->toCode() << ENDL;
		return true;
	} CATCH_ALL_ERRORS(true);
	return false;
}

bool CLISetInterface::run_script_string(String const& command, bool multiline) {
	vector<ScriptParseError> errors;
	ScriptP script = parse(command,nullptr,false,errors);
	if (errors.empty()) {
		return run_script(script);
	} else {
		FOR_EACH(error,errors) {
			if (multiline) {
				cli.show_message(MESSAGE_ERROR, String::Format(_("On line %d:\t"), error.line) + error.what());
			} else {
				cli.show_message(MESSAGE_ERROR, error.what());
			}
		}
		return false;
	}
}

bool CLISetInterface::run_script_file(String const& filename) {
	// read file
	if (!wxFileExists(filename)) {
		cli.show_message(MESSAGE_ERROR, _("File not found: ")+filename);
		return false;
	}
	wxFileInputStream is(filename);
	if (!is.Ok()) {
		cli.show_message(MESSAGE_ERROR, _("Unable to open file: ")+filename);
		return false;
	}
	wxBufferedInputStream bis(is);
	String code = read_utf8_line(bis, true, true);
	run_script_string(code);
	return true;
}

void CLISetInterface::showWelcome() {
	cli << _("                                                                     ___  \n")
	       _("  __  __           _       ___     _      ___    _ _ _              |__ \\ \n")
	       _(" |  \\/  |__ _ __ _(_)__   / __|___| |_   | __|__| (_) |_ ___ _ _       ) |\n")
	       _(" | |\\/| / _` / _` | / _|  \\__ | -_)  _|  | _|/ _` | |  _/ _ \\ '_|     / / \n")
	       _(" |_|  |_\\__,_\\__, |_\\__|  |___|___|\\__|  |___\\__,_|_|\\__\\___/_|      / /_ \n")
	       _("             |___/                                                  |____|\n\n");
	cli.flush();
}

void CLISetInterface::showUsage() {
	cli << _(" Commands available from the prompt:\n\n");
	cli << _("   <expression>        Execute a script expression, display the result\n");
	cli << _("   :help               Show this help page.\n");
	cli << _("   :load <setfile>     Load a different set file.\n");
	cli << _("   :quit               Exit the MSE command line interface.\n");
	cli << _("   :reset              Clear all local variable definitions.\n");
	cli << _("   :pwd                Print the current working directory.\n");
	cli << _("   :cd                 Change the working directory.\n");
	cli << _("   :! <command>        Perform a shell command.\n");
	cli << _("\n Commands can be abreviated to their first letter if there is no ambiguity.\n\n");
}

void CLISetInterface::handleCommand(const String& command) {
	try {
		if (command.empty()) {
			// empty, ignore
		} else if (command.GetChar(0) == _(':')) {
			// :something
			size_t space = min(command.find_first_of(_(' ')), command.size());
			String before = command.substr(0,space);
			String arg    = space + 1 < command.size() ? command.substr(space+1) : wxEmptyString;
			if (before == _(":q") || before == _(":quit")) {
				if (!quiet) {
					cli << _("Goodbye\n");
				}
				running = false;
			} else if (before == _(":?") || before == _(":h") || before == _(":help")) {
				showUsage();
			} else if (before == _(":l") || before == _(":load")) {
				if (arg.empty()) {
					cli.show_message(MESSAGE_ERROR,_("Give a filename to open."));
				} else {
					setSet(import_set(arg));
				}
			} else if (before == _(":r") || before == _(":reset")) {
				Context& ctx = getContext();
				ei.exported_images.clear();
				ctx.closeScope(scope);
				scope = ctx.openScope();
			} else if (before == _(":i") || before == _(":info")) {
				if (set) {
					cli << _("set:      ") << set->identification() << ENDL;
					cli << _("filename: ") << set->absoluteFilename() << ENDL;
					cli << _("relative: ") << set->relativeFilename() << ENDL;
					cli << String::Format(_("#cards:   %d"), set->cards.size()) << ENDL;
				} else {
					cli << _("No set loaded") << ENDL;
				}
			} else if (before == _(":c") || before == _(":cd")) {
				if (arg.empty()) {
					cli.show_message(MESSAGE_ERROR,_("Give a new working directory."));
				} else {
					if (!wxSetWorkingDirectory(arg)) {
						cli.show_message(MESSAGE_ERROR,_("Can't change working directory to ")+arg);
					} else {
						setExportInfoCwd();
					}
				}
			} else if (before == _(":pwd") || before == _(":p")) {
				cli << ei.directory_absolute << ENDL;
			} else if (before == _(":!")) {
				if (arg.empty()) {
					cli.show_message(MESSAGE_ERROR,_("Give a shell command to execute."));
				} else {
					#ifdef UNICODE
						#ifdef __WXMSW__
							_wsystem(arg.c_str()); // TODO: is this function available on other platforms?
						#else
							wxCharBuffer buf = arg.fn_str();
							system(buf);
						#endif
					#else
						system(arg.c_str());
					#endif
				}
			#if USE_SCRIPT_PROFILING
				} else if (before == _(":profile")) {
					if (arg == _("full")) {
						showProfilingStats(profile_root);
					} else {
						long level = 1;
						arg.ToLong(&level);
						showProfilingStats(profile_aggregated(level));
					}
			#endif
			} else {
				cli.show_message(MESSAGE_ERROR,_("Unknown command, type :help for help."));
			}
		} else if (command == _("exit") || command == _("quit")) {
			cli << _("Use :quit to quit\n");
		} else if (command == _("help")) {
			cli << _("Use :help for help\n");
		} else {
			run_script_string(command);
		}
	} catch (const Error& e) {
		cli.show_message(MESSAGE_ERROR,e.what());
	}
}

#if USE_SCRIPT_PROFILING
	DECLARE_TYPEOF_COLLECTION(FunctionProfileP);
	void CLISetInterface::showProfilingStats(const FunctionProfile& item, int level) {
		// show parent
		if (level == 0) {
			cli << GRAY << _("Time(s)   Avg (ms)  Calls   Function") << ENDL;
			cli <<         _("========  ========  ======  ===============================") << NORMAL << ENDL;
		} else {
			for (int i = 1 ; i < level ; ++i) cli << _("  ");
			cli << String::Format(_("%8.5f  %8.5f  %6d  %s"), item.total_time(), 1000 * item.avg_time(), item.calls, item.name.c_str()) << ENDL;
		}
		// show children
		vector<FunctionProfileP> children;
		item.get_children(children);
		FOR_EACH_REVERSE(c, children) {
			showProfilingStats(*c, level + 1);
		}
	}
#endif
