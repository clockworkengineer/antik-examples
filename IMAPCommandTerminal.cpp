#include "HOST.hpp"
/*
 * File:   IMAPCommandTerminal.cpp
 * 
 * Author: Robert Tizzard
 * 
 * Created on October 24, 2016, 2:33 PM
 *
 * Copyright 2016.
 *
 */

//
// Program: IMAPCommandTerminal
//
// Description: A Simple IMAP command console/terminal that logs on to a given IMAP server
// and executes commands typed in. The raw command responses are echoed back as default but 
// parsed responses are displayed if specified in program options.
// 
// Dependencies: C11++, Classes (CFile, CPath, CMailIMAP, CMailIMAPParse, CMailIMAPBodyStruct),
//               Linux, Boost C++ Libraries.
//
// IMAPCommandTerminal
// Program Options:
//   --help                Print help messages
//   -c [ --config ] arg   Config File Name
//   -s [ --server ] arg   IMAP Server URL and port
//   -u [ --user ] arg     Account username
//   -p [ --password ] arg User password
//   --parsed              Response parsed
//   --bodystruct          Parsed output includes bodystructs

// =============
// INCLUDE FILES
// =============

//
// C++ STL
//

#include <iostream>
#include <deque>

//
// Antik Classes
//

#include "CIMAP.hpp"
#include "CIMAPParse.hpp"
#include "CIMAPBodyStruct.hpp"
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

// Command line parameter data

struct ParamArgData {
    std::string userName;        // Email account user name
    std::string userPassword;    // Email account user name password
    std::string serverURL;       // SMTP server URL
    std::string configFileName;  // Configuration file name
    bool bParsed { false };      // true output parsed
    bool bBodystruct { false };  // Parsed output includes BODYSTRUCTS
};

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
            ("parsed", "Response parsed")
            ("bodystruct", "Parsed output includes bodystructs");

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
            std::cout << "IMAPCommandTerminal" << std::endl << commandLine << std::endl;
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

        // Display parsed output

        if (vm.count("parsed")) {
            argData.bParsed = true;
        }

        // Display parsed BODYSTRUCT output
        
        if (vm.count("bodystruct")) {
            argData.bBodystruct = true;
        }

        po::notify(vm);

    } catch (po::error& e) {
        std::cerr << "IMAPCommandTerminal Error: " << e.what() << std::endl << std::endl;
        std::cerr << commandLine << std::endl;
        exit(EXIT_FAILURE);
    }

}

//
// Walk data
//

struct WalkData {
    std::uint32_t count;
};

//
// Body structure tree walk function  too display body details.
//

static void walkFn(std::unique_ptr<CIMAPBodyStruct::BodyNode>& bodyNode, CIMAPBodyStruct::BodyPart& bodyPart, std::shared_ptr<void>& walkData) {

    auto wlkData = static_cast<WalkData *> (walkData.get());
    (void)wlkData;

    std::cout << std::string(120, '#') << std::endl;

    std::cout << "PART NO = [" << bodyPart.partNo << "]" << std::endl;
    std::cout << "TYPE= [" << bodyPart.parsedPart->type << "]" << std::endl;
    std::cout << "SUBTYPE= [" << bodyPart.parsedPart->subtype << "]" << std::endl;
    std::cout << "PARAMETER LIST = [" << bodyPart.parsedPart->parameterList << "]" << std::endl;
    std::cout << "ID = [" << bodyPart.parsedPart->id << "]" << std::endl;
    std::cout << "DESCRIPTION = [" << bodyPart.parsedPart->description << "]" << std::endl;
    std::cout << "ENCODING = [" << bodyPart.parsedPart->encoding << "]" << std::endl;
    std::cout << "SIZE = [" << bodyPart.parsedPart->size << "]" << std::endl;

    if (!bodyPart.parsedPart->textLines.empty()) {
        std::cout << "TEXTLINES = [" << bodyPart.parsedPart->textLines << "]" << std::endl;
    }

    if (!bodyPart.parsedPart->md5.empty()) {
        std::cout << "MD5 = [" << bodyPart.parsedPart->md5 << "]" << std::endl;
    }

    if (!bodyPart.parsedPart->disposition.empty()) {
        std::cout << "DISPOSITION = [" << bodyPart.parsedPart->disposition << "]" << std::endl;
    }

    if (!bodyPart.parsedPart->language.empty()) {
        std::cout << "LANGUAGE = [" << bodyPart.parsedPart->language << "]" << std::endl;
    }

    if (!bodyPart.parsedPart->location.empty()) {
        std::cout << "LOCATION = [" << bodyPart.parsedPart->location << "]" << std::endl;
    }

    std::cout << "EXTENDED = [" << bodyPart.parsedPart->extended << "]" << std::endl;

    std::cout << "MULTI-EXTENDED = [" << bodyNode->extended << "]" << std::endl;

}

//
// Display parsed IMAP command response
//

static void processIMAPResponse(CIMAP& imap, CIMAPParse::COMMANDRESPONSE& parsedResponse) {

    std::cout << std::string(120, '*') << std::endl;

    if (parsedResponse->byeSent) {
        std::cout << "BYE RECIEVED {" << parsedResponse->errorMessage << "}" << std::endl;
        return;

    } else if (parsedResponse->status != CIMAPParse::RespCode::OK) {
        std::cout << "COMMAND = {" << CIMAPParse::commandCodeString(parsedResponse->command) << "}" << std::endl;
        std::cout << "ERROR = {" << parsedResponse->errorMessage << "}" << std::endl;
        std::cout << std::string(120, '!') << std::endl;
        return;
    }

    switch (parsedResponse->command) {

        case CIMAPParse::Commands::SEARCH:
        {
            std::cout << "COMMAND {" << CIMAPParse::commandCodeString(parsedResponse->command) << "}" << std::endl;
            std::cout << "INDEXES = ";
            for (auto index : parsedResponse->indexes) {
                std::cout << index << " ";
            }
            std::cout << std::endl;
            break;
        }

        case CIMAPParse::Commands::STATUS:
        case CIMAPParse::Commands::SELECT:
        case CIMAPParse::Commands::EXAMINE:
        {
            std::cout << "COMMAND {" << CIMAPParse::commandCodeString(parsedResponse->command) << "}" << std::endl;
             for (auto resp : parsedResponse->responseMap) {
                std::cout << resp.first << " = " << resp.second << std::endl;
            }
            break;
        }

        case CIMAPParse::Commands::LIST:
        case CIMAPParse::Commands::LSUB:
        {
            std::cout << "COMMAND {" << CIMAPParse::commandCodeString(parsedResponse->command) << "}" << std::endl;
            for (auto mailboxEntry : parsedResponse->mailBoxList) {
                std::cout << "NAME = " << mailboxEntry.mailBoxName << std::endl;
                std::cout << "ATTRIB = " << mailboxEntry.attributes << std::endl;
                std::cout << "DEL = " << mailboxEntry.hierDel << std::endl;
            }
            break;
        }

        case CIMAPParse::Commands::EXPUNGE:
        {
            std::cout << "COMMAND {" << CIMAPParse::commandCodeString(parsedResponse->command) << "}" << std::endl;
            std::cout << "EXISTS = " << parsedResponse->responseMap[kEXISTS] << std::endl;
            std::cout << "EXPUNGED = "<< parsedResponse->responseMap[kEXPUNGE] << std::endl;
            break;
        }

        case CIMAPParse::Commands::STORE:
        {
            std::cout << "COMMAND {" << CIMAPParse::commandCodeString(parsedResponse->command) << "}" << std::endl;
            for (auto storeEntry : parsedResponse->storeList) {
                std::cout << "INDEX = " << storeEntry.index << std::endl;
                std::cout << "FLAGS = " << storeEntry.flagsList << std::endl;
            }
            break;
        }

        case CIMAPParse::Commands::CAPABILITY:
        {
            std::cout << "COMMAND {" << CIMAPParse::commandCodeString(parsedResponse->command) << "}" << std::endl;
            std::cout << "CAPABILITIES = " << parsedResponse->responseMap[kCAPABILITY] << std::endl;
            break;
        }

        case CIMAPParse::Commands::FETCH:
        {
            std::cout << "COMMAND {" << CIMAPParse::commandCodeString(parsedResponse->command) << "}" << std::endl;

            for (auto fetchEntry : parsedResponse->fetchList) {
                std::cout << "INDEX = " << fetchEntry.index << std::endl;
                for (auto resp : fetchEntry.responseMap) {
                    if (resp.first.compare(kBODYSTRUCTURE) == 0) {
                        std::unique_ptr<CIMAPBodyStruct::BodyNode> treeBase{ new CIMAPBodyStruct::BodyNode()};
                        std::shared_ptr<void> walkData{ new WalkData()};
                        CIMAPBodyStruct::consructBodyStructTree(treeBase, resp.second);
                        CIMAPBodyStruct::walkBodyStructTree(treeBase, walkFn, walkData);
                    } else {
                        std::cout << resp.first << " = " << resp.second << std::endl;
                    }
                }

            }
            break;
        }

        case CIMAPParse::Commands::NOOP:
        case CIMAPParse::Commands::IDLE:
        {
            std::cout << "COMMAND {" << CIMAPParse::commandCodeString(parsedResponse->command) << "}" << std::endl;
            if (!parsedResponse->responseMap.empty()) {
                for (auto resp : parsedResponse->responseMap) {
                    std::cout << resp.first << " = " << resp.second << std::endl;
                }
            } else {
                std::cout << "All quiet!!!" << std::endl;
            }
            break;
        }

        default:
        {
            std::cout << "COMMAND {" << CIMAPParse::commandCodeString(parsedResponse->command) << "}" << std::endl;
            break;
        }
    }

    std::cout << std::string(120, '+') << std::endl;

}

// ============================
// ===== MAIN ENTRY POINT =====
// ============================

int main(int argc, char** argv) {

    try {

        ParamArgData argData;
        CIMAP imap;
        std::deque<std::string> startupCommands;

        // Read in command line parameters and process

        procCmdLine(argc, argv, argData);

        std::cout << "SERVER [" << argData.serverURL << "]" << std::endl;
        std::cout << "USER [" << argData.userName << "]" << std::endl;

        // Set mail account user name and password

        imap.setServer(argData.serverURL);
        imap.setUserAndPassword(argData.userName, argData.userPassword);

        std::string commandLine;

        // Connect

        imap.connect();

        // Loop executing commands until exit

        do {

            // Process any startup commands first

            if (!startupCommands.empty()) {
                commandLine = startupCommands.front();
                startupCommands.pop_front();
            }

            // If command line empty them prompt and read in new command

            if (commandLine.empty()) {
                std::cout << "COMMAND>";
                std::getline(std::cin, commandLine);
            }

            // exit

            if (commandLine.compare("exit") == 0) break;

            // Run command

            if (!commandLine.empty()) {

                std::string commandResponse(imap.sendCommand(commandLine));

                if (argData.bParsed) {
                    CIMAPParse::COMMANDRESPONSE parsedRespnse(CIMAPParse::parseResponse(commandResponse));
                    processIMAPResponse(imap, parsedRespnse);
                } else {
                    std::cout << commandResponse << std::endl;
                }

                commandLine.clear();

            }

        } while (true);

    //
    // Catch any errors
    //    
 
    } catch (const std::exception & e) {
        exitWithError(e.what());
    }

    exit(EXIT_SUCCESS);

}