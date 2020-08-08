#include "HOST.hpp"
/*
 * File:   FileFolderWatcher.cpp
 * 
 * Author: Robert Tizzard
 * 
 * Created on June 17, 2018
 *
 * Copyright 2018.
 *
 */

//
// Program: FileFolderWatcher
//
// Description: Program that uses CApprise class to log files events on passed in folders and
// file. Any folders created in the watched directory are automatically added to the watch list.
//
// Dependencies: C11++, Classes (CApprise, CFile, CPath),
//               Linux, Boost C++ Libraries.
//

// =============
// INCLUDE FILES
// =============

//
// C++ STL
//

#include <iostream>
#include <thread>

//
// Antik Classes
//

#include "CApprise.hpp"
#include "CPath.hpp"
#include "CFile.hpp"

using namespace Antik::File;

//
// Boost program options library
//

#include <boost/program_options.hpp> 
#include <boost/algorithm/string.hpp>

namespace po = boost::program_options;

// ======================
// LOCAL TYES/DEFINITIONS
// ======================

//
// Command line parameter data
//

struct ParamArgData {
    std::string configFileName;
    std::string watchList;
};

// ===============
// LOCAL FUNCTIONS
// ===============

//
// Error closedown code.
//

static void exitWithError(CApprise &fileWatcher, const std::string &errMsg) {

    fileWatcher.stopWatching();
    
    std::cerr << errMsg << std::endl;
    exit(EXIT_FAILURE);

}

//
// Add options common to both command line and config file
//

static void addCommonOptions(po::options_description& commonOptions, ParamArgData& argData) {

    commonOptions.add_options()
            ("watchlist,w", po::value<std::string>(&argData.watchList)->required(), "Folder/file watch list");

}

//
// Read in and process command line arguments using boost.
//

static void procCmdLine(int argc, char** argv, ParamArgData &argData) {

    // Define and parse the program options

    po::options_description commandLine("Program Options");
    commandLine.add_options()
            ("help", "Print help messages")
            ("config,c", po::value<std::string>(&argData.configFileName), "Config File Name");

    addCommonOptions(commandLine, argData);

    po::options_description configFile("Config Files Options");

    addCommonOptions(configFile, argData);

    po::variables_map vm;

    try {

        // Process arguments

        po::store(po::parse_command_line(argc, argv, commandLine), vm);

        // Display options and exit with success

        if (vm.count("help")) {
            std::cout << "FileFolderWatcher Example Application" << std::endl << commandLine << std::endl;
            exit(EXIT_SUCCESS);
        }

        if (vm.count("config")) {
            if (CFile::exists(vm["config"].as<std::string>())) {
                std::ifstream configFileStream{vm["config"].as<std::string>()};
                if (configFileStream) {
                    po::store(po::parse_config_file(configFileStream, configFile), vm);
                }
            } else {
                throw po::error("Specified config file does not exist.");
            }
        }

        po::notify(vm);

    } catch (po::error& e) {
        std::cerr << "FileFolderWatcher Error: " << e.what() << std::endl << std::endl;
        std::cerr << commandLine << std::endl;
        exit(EXIT_FAILURE);
    }

}

//
// Convert event id to string
//

static const std::string getEventName(CApprise::EventId id) {
    
    switch (id) {
        case CApprise::Event_none:
            return ("None");
            break;
        case CApprise::Event_add:
            return ("Add File");
            break;
        case CApprise::Event_change:
            return ("Change File");
            break;
        case CApprise::Event_unlink:
            return ("Delete File");
            break;
        case CApprise::Event_addir:
            return ("Add Directory");
            break;
        case CApprise::Event_unlinkdir:
            return ("Remove Directory");
            break;
        case CApprise::Event_error:
            return ("Error");
            break;
    }
    
    return("Unknown event");

}

// ============================
// ===== MAIN ENTRY POINT =====
// ============================

int main(int argc, char** argv) {

    CApprise fileWatcher;
            
    try {

        ParamArgData argData;
        std::vector<std::string> filesToWatch;

        // Read in command line parameters and process

        procCmdLine(argc, argv, argData);
        
        // Create list of files/folders to watch
        
        boost::split(filesToWatch,argData.watchList,boost::is_any_of(","));
        
        // Add watches
        
        for (auto file : filesToWatch) {
            boost::trim(file);
            fileWatcher.addWatch(file);
            std::cout << "Watching [" << file << "]" << std::endl;
        }
  
        // Start watching
       
        fileWatcher.startWatching();
        
        // Get events and output
        
        while(fileWatcher.stillWatching()) {
            CApprise::Event fileEvent;
            fileWatcher.getNextEvent(fileEvent);
            std::cout << getEventName(fileEvent.id) << " [" << fileEvent.message << "]" << std::endl;
        }
        
        // Stop watching for events
        
        fileWatcher.stopWatching();
        
    //
    // Catch any errors
    //    
        
    } catch (const std::exception & e) {
        exitWithError(fileWatcher, e.what());
    }

    exit(EXIT_SUCCESS);

}


