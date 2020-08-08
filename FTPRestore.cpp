#include "HOST.hpp"
/*
 * File:   FTPRestore.cpp
 * 
 * Author: Robert Tizzard
 * 
 * Created on October 24, 2017, 2:33 PM
 *
 * Copyright 2017.
 *
 */

//
// Program: FTPRestore
//
// Description: Simple FTP restore program that takes a remote directory and restores it
// to a local directory.
//
// Dependencies: C11++, Classes (CFTP, CFile, CPath, CSocket), Boost C++ Libraries.
//
// FTPRestore
// Program Options:
//   --help                 Print help messages
//   -c [ --config ] arg    Config File Name
//   -s [ --server ] arg    FTP Server
//   -p [ --port ] arg      FTP Server port
//   -u [ --user ] arg      Account username
//   -p [ --password ] arg  User password
//   -r [ --remote ] arg    Remote server directory to restore
//   -l [ --local ] arg     Local directory to use as base for restore
//

// =============
// INCLUDE FILES
// =============

//
// C++ STL
//

#include <iostream>

//
// Antik Classes
//

#include "FTPUtil.hpp"
#include "CPath.hpp"
#include "CFile.hpp"

using namespace Antik;
using namespace Antik::FTP;
using namespace Antik::File;

//
// Boost program options library
//

#include <boost/program_options.hpp>  

namespace po = boost::program_options;

// ======================
// LOCAL TYES/DEFINITIONS
// ======================

// Command line parameter data

struct ParamArgData {
    std::string userName;        // FTP account user name
    std::string userPassword;    // FTP account user name password
    std::string serverName;      // FTP server
    std::string serverPort;      // FTP server port
    std::string remoteDirectory; // FTP remote directory to restore
    std::string localDirectory;  // Local directory to use as base for restore
    std::string configFileName;  // Configuration file name
};

// ===============
// LOCAL FUNCTIONS
// ===============

//
// Exit with error message/status
//

static void exitWithError(std::string errMsg) {

    // Display error and exit.

    std::cout.flush();
    std::cerr << errMsg << std::endl;
    exit(EXIT_FAILURE);

}

//
// Add options common to both command line and config file
//

static void addCommonOptions(po::options_description& commonOptions, ParamArgData& argData) {

    commonOptions.add_options()
            ("server,s", po::value<std::string>(&argData.serverName)->required(), "FTP Server name")
            ("port,o", po::value<std::string>(&argData.serverPort)->required(), "FTP Server port")
            ("user,u", po::value<std::string>(&argData.userName)->required(), "Account username")
            ("password,p", po::value<std::string>(&argData.userPassword)->required(), "User password")
            ("remote,r", po::value<std::string>(&argData.remoteDirectory)->required(), "Remote directory to restore")
            ("local,l", po::value<std::string>(&argData.localDirectory)->required(), "Local directory as base for restore");

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
            std::cout << "FTPRestore" << std::endl << commandLine << std::endl;
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
        std::cerr << "FTPRestore Error: " << e.what() << std::endl << std::endl;
        std::cerr << commandLine << std::endl;
        exit(EXIT_FAILURE);
    }

}

// ============================
// ===== MAIN ENTRY POint =====
// ============================

int main(int argc, char** argv) {

    try {

        ParamArgData argData;
        CFTP ftpServer;
        FileList  remoteFileList;
        FileList restoredFiles;

        // Read in command line parameters and process

        procCmdLine(argc, argv, argData);

        std::cout << "SERVER [" << argData.serverName << "]" << std::endl;
        std::cout << "SERVER PORT [" << argData.serverPort << "]" << std::endl;
        std::cout << "USER [" << argData.userName << "]" << std::endl;
        std::cout << "REMOTE DIRECTORY [" << argData.remoteDirectory << "]" << std::endl;
        std::cout << "LOCAL DIRECTORY [" << argData.localDirectory << "]\n" << std::endl;
    
        // Set server and port
        
        ftpServer.setServerAndPort(argData.serverName, argData.serverPort);

        // Set mail account user name and password

        ftpServer.setUserAndPassword(argData.userName, argData.userPassword);

        // Enable SSL

        ftpServer.setSslEnabled(true);

        // Connect

        if (ftpServer.connect() != 230) {
            throw CFTP::Exception("Unable to connect status returned = " + ftpServer.getCommandResponse());
        }

        // Get remote directory file list
        
        listRemoteRecursive(ftpServer, argData.remoteDirectory, remoteFileList);
        
        // Restore files from  FTP Server

        if (!remoteFileList.empty()) {
            restoredFiles = getFiles(ftpServer, argData.localDirectory, remoteFileList);
        }

        // Signal success or failure
        
        if (!restoredFiles.empty()) {
            for (auto file : restoredFiles) {
                std::cout << "Successfully restored [" << file << "]" << std::endl;
            }
        } else {
            std::cout << "Restore failed."<< std::endl;
        }

        // Disconnect 
        
        ftpServer.disconnect();

    //
    // Catch any errors
    //    

    } catch (const std::exception &e) {
        exitWithError(e.what());
    }

    exit(EXIT_SUCCESS);

}