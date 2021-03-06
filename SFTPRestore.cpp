//
// Program: SFTPRestore
//
// Description: Simple SFTP restore program that takes a remote directory and restores it
// to a local directory.
//
// Dependencies: C11++, Classes (CSFTP, CSSHSession, CPath, CFile), Boost C++ Libraries.
//
// SFTPRestore
// Program Options:
//   --help                 Print help messages
//   -c [ --config ] arg    Config File Name
//   -s [ --server ] arg    SSH Server
//   -p [ --port ] arg      SSH Server port
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
#include <fstream>

//
// Antik Classes
//

#include "SSHSessionUtil.hpp"
#include "SFTPUtil.hpp"
#include "CPath.hpp"
#include "CFile.hpp"

using namespace Antik;
using namespace Antik::SSH;
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
    std::string userName;        // SSH account user name
    std::string userPassword;    // SSH account user name password
    std::string serverName;      // SSH server
    std::string serverPort;      // SSH server port
    std::string remoteDirectory; // SSH remote directory to restore
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
            ("server,s", po::value<std::string>(&argData.serverName)->required(), "SSH Server name")
            ("port,o", po::value<std::string>(&argData.serverPort)->required(), "SSH Server port")
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
            std::cout << "SFTPRestore" << std::endl << commandLine << std::endl;
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
        std::cerr << "SFTPRestore Error: " << e.what() << std::endl << std::endl;
        std::cerr << commandLine << std::endl;
        exit(EXIT_FAILURE);
    }

}

//
// Perform restore of backed up  files.
//

static void performRestore(CSSHSession &sshSession, ParamArgData argData) {

    CSFTP sftpServer{ sshSession};

    try {

        FileMapper fileMapper{ argData.localDirectory, argData.remoteDirectory};
        FileList remoteFileList;
        FileList restoredFiles;

        // Open SFTP session

        sftpServer.open();

        // Get remote directory file list

        listRemoteRecursive(sftpServer, argData.remoteDirectory, remoteFileList);

        // Restore files from  SFTP Server

        if (!remoteFileList.empty()) {
            restoredFiles = getFiles(sftpServer, fileMapper, remoteFileList);
        }

        // Signal success or failure

        if (!restoredFiles.empty()) {
            for (auto file : restoredFiles) {
                std::cout << "Successfully restored [" << file << "]" << std::endl;
            }
        } else {
            std::cout << "Restore failed." << std::endl;
        }

        // Close SFTP session 

        sftpServer.close();

    //
    // Catch any errors
    //    

    } catch (...) {
        sftpServer.close();
        throw;
    }
    
}

// ============================
// ===== MAIN ENTRY POint =====
// ============================

int main(int argc, char** argv) {

    CSSHSession sshSession;
    ServerVerificationContext verificationContext { &sshSession }; 
       
    try {

        ParamArgData argData;
  
        // Read in command line parameters and process

        procCmdLine(argc, argv, argData);

        std::cout << "SERVER [" << argData.serverName << "]" << std::endl;
        std::cout << "SERVER PORT [" << argData.serverPort << "]" << std::endl;
        std::cout << "USER [" << argData.userName << "]" << std::endl;
        std::cout << "REMOTE DIRECTORY [" << argData.remoteDirectory << "]" << std::endl;
        std::cout << "LOCAL DIRECTORY [" << argData.localDirectory << "]\n" << std::endl;
    
          // Set server and port
        
        sshSession.setServer(argData.serverName);
        sshSession.setPort(std::stoi(argData.serverPort));

        // Set account user name and password

        sshSession.setUser(argData.userName);
        sshSession.setUserPassword(argData.userPassword);

         // Connect to server

        sshSession.connect();

        // Verify the server's identity

        if (!verifyKnownServer(sshSession, verificationContext)) {
            throw std::runtime_error("Unable to verify server.");
        } else {
            std::cout << "Server verified..." << std::endl;
        }

        // Authenticate ourselves

        if (!userAuthorize(sshSession)) {
            throw std::runtime_error("Server unable to authorize client");
        } else {
            std::cout << "Client authorized..." << std::endl;
        }

        // Perform file restore
        
        performRestore(sshSession, argData);
        
        // Disconnect session
 
        sshSession.disconnect();

    //
    // Catch any errors
    //    

    } catch (const std::exception &e) {
        exitWithError(e.what());
    }

    exit(EXIT_SUCCESS);

}