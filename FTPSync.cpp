#include "HOST.hpp"
/*
 * File:   FTPSync.cpp
 * 
 * Author: Robert Tizzard
 * 
 * Created on October 24, 2017, 2:33 PM
 *
 * Copyright 2017.
 *
 */

//
// Program: FTPSync
//
// Description: Simple FTP program that takes a local directory and keeps it 
// sycnhronized with a remote server directory. Note: Im sure the sync can be done
// in one pass (come back and look at later).
//
// Dependencies: C11++, Classes (CFTP, CFile, CPath, CSocket), Boost C++ Libraries.
//
// FTPSync
// Program Options:
//   --help                 Print help messages
//   -c [ --config ] arg    Config File Name
//   -s [ --server ] arg    FTP Server
//   -p [ --port ] arg      FTP Server port
//   -u [ --user ] arg      Account username
//   -p [ --password ] arg  User password
//   -r [ --remote ] arg    Remote server directory
//   -l [ --local ] arg     Local directory
//

// =============
// INCLUDE FILES
// =============

//
// C++ STL
//

#include <iostream>
#include <unordered_map>

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
// Boost program library
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
    std::string remoteDirectory; // FTP remote directory for sync
    std::string localDirectory;  // Local directory for sync with server
    std::string configFileName;  // Configuration file name
};

// ===============
// LOCAL FUNCTIONS
// ===============

//
// Exit with error message/status
//

void exitWithError(std::string errMsg) {

    // Display error and exit.

    std::cout.flush();
    std::cerr << errMsg << std::endl;
    exit(EXIT_FAILURE);

}

//
// Add options common to both command line and config file
//

void addCommonOptions(po::options_description& commonOptions, ParamArgData& argData) {

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

void procCmdLine(int argc, char** argv, ParamArgData &argData) {

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
            std::cout << "FTPSync" << std::endl << commandLine << std::endl;
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
        
        if (argData.localDirectory.back() != '/')argData.localDirectory.push_back('/');

    } catch (po::error& e) {
        std::cerr << "FTPSync Error: " << e.what() << std::endl << std::endl;
        std::cerr << commandLine << std::endl;
        exit(EXIT_FAILURE);
    }

}

//
// Convert local file path to remote server path
//

static inline std::string localFileToRemote(ParamArgData &argData, const std::string &localFilePath) {
    return(argData.remoteDirectory+localFilePath.substr(argData.localDirectory.rfind('/')));
}

//
// Convert remote server file path to local path
//

static inline std::string remoteFileToLocal(ParamArgData &argData, const std::string &remoteFilePath) {
    return(argData.localDirectory+remoteFilePath.substr(argData.remoteDirectory.size()+1));
}

// ============================
// ===== MAIN ENTRY POint =====
// ============================

int main(int argc, char** argv) {

    try {

        ParamArgData argData;
        CFTP ftpServer;
        std::vector<std::string> localFiles;
        std::vector<std::string> remoteFiles;

        // Read in command line parameters and process

        procCmdLine(argc, argv, argData);

        std::cout << "SERVER [" << argData.serverName << "]" << std::endl;
        std::cout << "SERVER PORT [" << argData.serverPort << "]" << std::endl;
        std::cout << "USER [" << argData.userName << "]" << std::endl;
        std::cout << "REMOTE DIRECTORY [" << argData.remoteDirectory << "]" << std::endl;
        std::cout << "LOCAL DIRECTORY [" << argData.localDirectory << "]\n" << std::endl;

        // Set server and port

        ftpServer.setServerAndPort(argData.serverName, argData.serverPort);

        // Set FTP account user name and password

        ftpServer.setUserAndPassword(argData.userName, argData.userPassword);

        // Enable SSL

        ftpServer.setSslEnabled(true);

        // Connect

        if (ftpServer.connect() != 230) {
            throw CFTP::Exception("Unable to connect status returned = " + ftpServer.getCommandResponse());
        }

        // Create remote directory

        if (!ftpServer.fileExists(argData.remoteDirectory)) {
            makeRemotePath(ftpServer, argData.remoteDirectory);
            if (!ftpServer.fileExists(argData.remoteDirectory)) {
                throw CFTP::Exception("Remote FTP server directory " + argData.remoteDirectory + " could not created.");
            }
        }
        
        ftpServer.changeWorkingDirectory(argData.remoteDirectory);
        ftpServer.getCurrentWoringDirectory(argData.remoteDirectory);
        
        // Get local and remote file lists
      
        listRemoteRecursive(ftpServer, argData.remoteDirectory, remoteFiles);
        listLocalRecursive(argData.localDirectory, localFiles);

        if (remoteFiles.empty()) {
            std::cout << "*** Remote server directory empty ***" << std::endl;
        }
        
        if (localFiles.empty()) {
            std::cout << "*** Local directory empty ***" << std::endl;
        }

        // PASS 1) Copy new files to server

        std::cout << "*** Transferring any new files to server ***" << std::endl; 
      
        std::vector<std::string> newFiles;
        std::vector<std::string> newFilesTransfered;
        
        for (auto file : localFiles) {
            if (find(remoteFiles.begin(), remoteFiles.end(), localFileToRemote(argData, file)) == remoteFiles.end()) {
                newFiles.push_back(file);
            }
        }
 
        if (!newFiles.empty()) {
            newFilesTransfered = putFiles(ftpServer, argData.localDirectory, newFiles);
            std::cout << "Number of new files transfered [" << newFilesTransfered.size() << "]" << std::endl;
            std::copy(newFilesTransfered.begin(), newFilesTransfered.end(), std::back_inserter(remoteFiles));
        }

        // PASS 2) Remove any deleted local files from server

        std::cout << "*** Removing any deleted local files from server ***" << std::endl; 
               
        for (auto file : remoteFiles) {
            if (find(localFiles.begin(), localFiles.end(), remoteFileToLocal(argData, file))==localFiles.end()) {
                if (ftpServer.deleteFile(file) == 250) {
                    std::cout << "File [" << file << " ] removed from server." << std::endl;
                } else if (ftpServer.removeDirectory(file) == 250) {
                    std::cout << "Directory [" << file << " ] removed from server." << std::endl;
                } else {
                    std::cerr << "File [" << file << " ] could not be removed from server." << std::endl;
                }
            }
        }

        // PASS 3) Copy any updated local files to remote server. Note: PASS 2 may
        // have deleted some remote files but if the get modified date/time fails
        // it is ignored and not added to remoteFileModifiedTimes.
        
        std::cout << "*** Copying updated local files to server ***" << std::endl; 
        
        // Fill out remote file name, date/time modified map.
        
        std::unordered_map<std::string, CFTP::DateTime> remoteFileModifiedTimes;
        
        for (auto file : remoteFiles) {
            CFTP::DateTime modifiedDateTime;
            if (ftpServer.getModifiedDateTime(file, modifiedDateTime)==213) {
                remoteFileModifiedTimes[file] = modifiedDateTime;
            }
        }

        // Copy updated files to server
        
        for (auto file : localFiles) {
            if (CFile::isFile(file)) {
                auto localModifiedTime = CFile::lastWriteTime(file);
                if (remoteFileModifiedTimes[localFileToRemote(argData, file)] < 
                        static_cast<CFTP::DateTime>(std::localtime(&localModifiedTime))) {
                    std::cout << "Server file " << localFileToRemote(argData, file) << " out of date." << std::endl;
                    if (ftpServer.putFile(localFileToRemote(argData, file), file) == 226) {
                        std::cout << "File [" << file << " ] copied to server." << std::endl;
                    } else {
                        std::cerr << "File [" << file << " ] not copied to server." << std::endl;
                    }
                }
            }
        }
        
        // Disconnect 

        ftpServer.disconnect();

        std::cout << "*** Files synchronized with server ***" << std::endl; 
              
    //
    // Catch any errors
    //    

    } catch (std::exception &e) {
        exitWithError(e.what());
    }

    exit(EXIT_SUCCESS);

}