#include "HOST.hpp"
/*
 * File:   ArchiveFolder.cpp
 * 
 * Author: Robert Tizzard
 * 
 * Created on March 4, 2017, 2:33 PM
 *
 * Copyright 2016.
 *
 */

//
// Program: ArchiveFolder
//
// Description: This is a command line program that writes the contents of a source
// folder to a ZIP archive; traversing it recursively and adding any sub-folder contents.
// All parameters and their meaning are obtained by running the program with the 
// parameter --help.
//
// ArchiveFolder Example Application
// Command Line Options:
//   --help                Display help message
//   -c [ --config ] arg   Config File Name
//   -s [ --Source ] arg   Source Folder To ZIP
//   -z [ --zip ] arg      ZIP File Name
// 
// Dependencies: C11++, Classes (CFileZIP), Linux, Boost C++ Libraries.
//

// =============
// INCLUDE FILES
// =============

//
// C++ STL
//

#include <cstdlib>
#include <iostream>
#include <thread>
#include <iomanip>

//
// Antik Classes
//

#include "CZIP.hpp"
#include "CPath.hpp"
#include "CFile.hpp"

using namespace Antik::ZIP;
using namespace Antik::File;

//
// BOOST program options library.
//

#include <boost/program_options.hpp>

namespace po = boost::program_options;

// ======================
// LOCAL TYES/DEFINITIONS
// ======================

//
// Command line parameter data
//

struct ParamArgData {
    std::string configFileName;      // Configuration file name
    std::string zipFileName;         // ZIP Archive File Name
    std::string sourceFolderName;    // Source folder
};

// ===============
// LOCAL FUNCTIONS
// ===============

//
// Exit with error message/status
//

static void exitWithError(std::string errMsg) {

    std::cerr << errMsg << std::endl;
    exit(EXIT_FAILURE);

}

//
// Add options common to both command line and config file
//

static void addCommonOptions(po::options_description& commonOptions, ParamArgData &argData) {

    commonOptions.add_options()
            ("source,s", po::value<std::string>(&argData.sourceFolderName)->required(), "Source Folder To ZIP")
            ("zip,z", po::value<std::string>(&argData.zipFileName)->required(), "ZIP File Name");

}

//
// Read in and process command line arguments using boost. 
//

static void procCmdLine(int argc, char** argv, ParamArgData &argData) {

    // Define and parse the program options

    po::options_description commandLine("Command Line Options");
    commandLine.add_options()
            ("help", "Display help message")
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
            std::cout << "ArchiveFolder Example Application" << std::endl << commandLine << std::endl;
            exit(0);
        }

        if (vm.count("config")) {
            if (CFile::exists(CPath(vm["config"].as<std::string>()))) {
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
        std::cerr << "ArchiveFolder Error: " << e.what() << std::endl << std::endl;
        std::cerr << commandLine << std::endl;
        exit(1);
    }

}

// ============================
// ===== MAIN ENTRY POINT =====
// ============================

int main(int argc, char** argv) {

    ParamArgData argData;

    try {

        // Read in command line parameters and process

        procCmdLine(argc, argv, argData);

        if (!argData.zipFileName.empty()) {

            CZIP zipFile(argData.zipFileName);

            // Create Archive
            
            zipFile.create();
            
            // Iterate recursively through folder hierarchy creating file list
            
            Antik::FileList fileNameList = CFile::directoryContentsList(argData.sourceFolderName);
            
            zipFile.open();
            
            // Add files to archive
            
            std::cout << "There are " << fileNameList.size() << " files: " << std::endl;
            for (auto& fileName : fileNameList) {
                std::cout << "Add " << fileName << '\n';
                if (CFile::isFile(fileName))  zipFile.add(fileName,fileName.substr(1));
            }
            
            // Save archive
            
            std::cout << "Creating Archive " << argData.zipFileName  << "." << std::endl;           
            zipFile.close();
      
        }


    //
    // Catch any errors
    //

    } catch (const std::exception & e) {
        exitWithError(e.what());
    }

    //
    // Normal closedown
    //

    exit(EXIT_SUCCESS);

}