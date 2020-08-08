#include "HOST.hpp"
/*
 * File:   WaitForMailBoxEvent.cpp
 * 
 * Author: Robert Tizzard
 * 
 * Created on October 24, 2016, 2:33 PM
 *
 * Copyright 2016.
 *
 */

//
// Program: WaitForMailBoxEvent
//
// Description: Log on to a IMAP server and wait for a status change in a specified
// mailbox. By default it will use IDLE but polling every time period is also supported.
// This is not directly useful but may be applied to other situations where the functionality
// is needed.All parameters and their meaning are obtained by running the program with the 
// parameter --help.
//
// WaitForMailBoxEvent Example Application
// Program Options:
//   --help                Print help messages
//   -c [ --config ] arg   Config File Name
//   -s [ --server ] arg   IMAP Server URL and port
//   -u [ --user ] arg     Account username
//   -p [ --password ] arg User password
//   -m [ --mailbox ] arg  Mailbox name
//   -l [ --poll ]         Check status using NOOP
//   -w [ --wait ]         Wait for new mail
//   
//
// Dependencies: C11++, Classes (CMailIMAP, CMailIMAPParse, CFile, CPath),
//               Linux, Boost C++ Libraries.
//

// =============
// INCLUDE FILES
// =============

//
// C++ STL
//

#include <iostream>
#include <chrono>
#include <thread>

//
// Antik Classes
//

#include "CIMAP.hpp"
#include "CIMAPParse.hpp"
#include "CPath.hpp"
#include "CFile.hpp"

using namespace Antik::IMAP;
using namespace Antik::File;

//
// Boost program options library
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
    std::string userName;           // Email account user name
    std::string userPassword;       // Email account user name password
    std::string serverURL;          // SMTP server URL
    std::string mailBoxName;        // Mailbox name
    std::string configFileName;     // Configuration file name
    bool bPolls { false };          // ==true then use NOOP
    bool bWaitForNewMail { false }; // ==true wait for a new message to arrive
};

//
// Polling period between NOOP in seconds;
//

constexpr int kPollPeriod = 15;

// ===============
// LOCAL FUNCTIONS
// ===============


//
// Exit with error message/status
//

static void exitWithError(std::string errMsg) {

    // Display error and exit.

    std::cerr << errMsg << std::endl;
    exit(EXIT_FAILURE);

}

//
// Add options common to both command line and config file
//

static void addCommonOptions(po::options_description& commonOptions, ParamArgData& argData) {

    commonOptions.add_options()
            ("server,s", po::value<std::string>(&argData.serverURL)->required(), "IMAP Server URL and port")
            ("user,u", po::value<std::string>(&argData.userName)->required(), "Account username")
            ("password,p", po::value<std::string>(&argData.userPassword)->required(), "User password")
            ("mailbox,m", po::value<std::string>(&argData.mailBoxName)->required(), "Mailbox name")
            ("wait,w", "Wait for new mail")
            ("poll,l", "Check status using NOOP");

}

//
// Read in and process command line arguments using boost.
//

static void procCmdLine(int argc, char** argv, ParamArgData &argData) {

    // Define and parse the program options

    po::options_description commandLine("Program Options");
    commandLine.add_options()
            ("help", "Print help messages")
            ("config,c", po::value<std::string>(&argData.configFileName)->required(), "Config File Name");

    addCommonOptions(commandLine, argData);

    po::options_description configFile("Config Files Options");

    addCommonOptions(configFile, argData);

    po::variables_map vm;

    try {

        // Process arguments

        po::store(po::parse_command_line(argc, argv, commandLine), vm);

        // Display options and exit with success

        if (vm.count("help")) {
            std::cout << "WaitForMailBoxEvent Example Application" << std::endl << commandLine << std::endl;
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

        // Check status with polling loop

        if (vm.count("poll")) {
            argData.bPolls = true;
        }
        
        // Wait for new mail

        if (vm.count("wait")) {
            argData.bWaitForNewMail = true;
        }


    } catch (po::error& e) {
        std::cerr << "WaitForMailBoxEvent Error: " << e.what() << std::endl << std::endl;
        std::cerr << commandLine << std::endl;
        exit(EXIT_FAILURE);
    }

}

//
// Parse command response and return pointer to parsed data.
//

static CIMAPParse::COMMANDRESPONSE parseCommandResponse(const std::string& command,
        const std::string& commandResponse) {

    CIMAPParse::COMMANDRESPONSE parsedResponse;

    try {
        parsedResponse = CIMAPParse::parseResponse(commandResponse);
    } catch (CIMAPParse::Exception &e) {
        std::cerr << "RESPONSE IN ERRROR: [" << commandResponse << "]" << std::endl;
        throw (e);
    }

    if (parsedResponse) {
        if (parsedResponse->byeSent) {
            throw CIMAP::Exception("Received BYE from server: " + parsedResponse->errorMessage);
        } else if (parsedResponse->status != CIMAPParse::RespCode::OK) {
            throw CIMAP::Exception(command + ": " + parsedResponse->errorMessage);
        }
    } else {
        throw CIMAPParse::Exception("nullptr parsed response returned.");
    }

    return (parsedResponse);

}

//
// Send command to IMAP server. At present it checks for any errors and just exits.
//

static std::string sendCommand(CIMAP& imap, const std::string& mailBoxName,
        const std::string& command) {

    std::string commandResponse;

    try {
        commandResponse = imap.sendCommand(command);
    } catch (CIMAP::Exception &e) {
        std::cerr << "IMAP ERROR: Need to reconnect to server" << std::endl;
        throw (e);
    }

    return (commandResponse);

}

// ============================
// ===== MAIN ENTRY POINT =====
// ============================

int main(int argc, char** argv) {

    try {

        ParamArgData argData;
        CIMAP imap;
        std::string comandResponse, command;
        CIMAPParse::COMMANDRESPONSE parsedResponse;
        int exists = 0, newExists = 0;

        // Read in command line parameters and process

        procCmdLine(argc, argv, argData);

        // Set mail account user name and password

        imap.setServer(argData.serverURL);
        imap.setUserAndPassword(argData.userName, argData.userPassword);

        // Connect

        std::cout << "Connecting to server [" << argData.serverURL << "]" << std::endl;

        imap.connect();

        std::cout << "Connected." << std::endl;

        // SELECT mailbox

        command = "SELECT " + argData.mailBoxName;
        comandResponse = sendCommand(imap, argData.mailBoxName, command);
        parsedResponse = parseCommandResponse(command, comandResponse);

        if (parsedResponse->responseMap.find("EXISTS") != parsedResponse->responseMap.end()) {
            exists = std::strtoull(parsedResponse->responseMap["EXISTS"].c_str(), nullptr, 10);
            std::cout << "Current Messages [" << exists << "]" << std::endl;
        }

        do {

            // IDLE is prone to server disconnect so its recommended to use polling 
            // instead but IDLE is shown here just for completion.

            std::cout << "Waiting on mailbox [" << argData.mailBoxName << "]" << std::endl;

            if (!argData.bPolls) {
                command = "IDLE";
                comandResponse = sendCommand(imap, argData.mailBoxName, command);
                parsedResponse = parseCommandResponse(command, comandResponse);
            } else {
                while (true) {
                    std::cout << "Polling [" << argData.mailBoxName << "]" << std::endl;
                    command = "NOOP";
                    comandResponse = sendCommand(imap, argData.mailBoxName, command);
                    parsedResponse = parseCommandResponse(command, comandResponse);
                    if (parsedResponse->responseMap.size() >= 1) break;
                    std::this_thread::sleep_for(std::chrono::seconds(kPollPeriod));
                }
            }

            // Display any response

            for (auto& resp : parsedResponse->responseMap) {
                std::cout << resp.first << " = " << resp.second << std::endl;
            }

            if (parsedResponse->responseMap.find("EXISTS") != parsedResponse->responseMap.end()) {
                newExists = std::strtoull(parsedResponse->responseMap["EXISTS"].c_str(), nullptr, 10);
                if (newExists > exists) {
                    std::cout << "YOU HAVE NEW MAIL !!!" << std::endl;
                    break;
                }
                exists = newExists;
            }

        } while (argData.bWaitForNewMail);

        std::cout << "Disconnecting from server [" << argData.serverURL << "]" << std::endl;

        imap.disconnect();


    //
    // Catch any errors
    //    

    } catch (const std::exception & e) {
        exitWithError(e.what());
    }

    exit(EXIT_SUCCESS);

}


