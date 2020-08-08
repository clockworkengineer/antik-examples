#include "HOST.hpp"
/*
 * File:   ArchiveMailBox.cpp
 * 
 * Author: Robert Tizzard
 * 
 * Created on October 24, 2016, 2:33 PM
 *
 * Copyright 2016.
 *
 */

//
// Program: ArchiveMailBox
//
// Description: This is a command line program to log on to an IMAP server and download e-mails
// from a configured mailbox, command separated mailbox list or all mailboxes for an account.
// A file (.eml) is created for each e-mail in a folder with the same name as the mailbox; with
// the files name being a combination of the mails UID/index prefix and the subject name. All 
// parameters and their meaning are obtained by running the program with the parameter --help.
//
// ArchiveMailBox Example Application
// Program Options:
//  --help                   Print help messages
//   -c [ --config ] arg      Config File Name
//   -s [ --server ] arg      IMAP Server URL and port
//   -u [ --user ] arg        Account username
//   -p [ --password ] arg    User password
//   -m [ --mailbox ] arg     Mailbox name
//   -d [ --destination ] arg Destination for attachments
//   -u [ --updates ]         Search since last file archived.
//   -a [ --all ]             Download files for all mailboxes.
//
// Note: MIME encoded words in the email subject line are decoded to the best ASCII fit
// available.
// 
// Dependencies: C11++, Classes (CFileMIME, CFile, CPath, CMailIMAP, CMailIMAPParse,
//               CMailIMAPBodyStruct), Linux, Boost C++ Libraries.
//

// =============
// INCLUDE FILES
// =============

//
// C++ STL
//

#include <iostream> 
#include <fstream>
#include <sstream>

//
// Antik Classes
//

#include "CIMAP.hpp"
#include "CIMAPParse.hpp"
#include "CMIME.hpp"
#include "CPath.hpp"
#include "CFile.hpp"

using namespace Antik::IMAP;
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
    std::string userName;          // Email account user name
    std::string userPassword;      // Email account user name password
    std::string serverURL;         // SMTP server URL
    std::string mailBoxName;       // Mailbox name
    std::string destinationFolder; // Destination folder for e-mail archive
    std::string configFileName;    // Configuration file name
    bool bOnlyUpdates {false };    // = true search date since last .eml archived
    bool bAllMailBoxes {false };   // = true archive all mailboxes

};

//
// Maximum subject line to take in file name
//

constexpr int kMaxSubjectLine = 80;

//
// .eml file extention
//

constexpr const  char *kEMLFileExt { ".eml" };

// ===============
// LOCAL FUNCTIONS
// ===============

//
// Exit with error message/status
//

static void exitWithError(const std::string errMsg) {

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
            ("destination,d", po::value<std::string>(&argData.destinationFolder)->required(), "Destination for e-mail archive")
            ("updates,u", "Search since last file archived.")
            ("all,a", "Download files for all mailboxes.");

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
            std::cout << "ArchiveMailBox Example Application" << std::endl << commandLine << std::endl;
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

        // Search for new e-mails only
        
        if (vm.count("updates")) {
            argData.bOnlyUpdates = true;
        }

        // Download all mailboxes
        
        if (vm.count("all")) {
            argData.bAllMailBoxes = true;
        }

        po::notify(vm);

    } catch (po::error& e) {
        std::cerr << "ArchiveMailBox Error: " << e.what() << std::endl << std::endl;
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

    if (parsedResponse->byeSent) {
        throw CIMAP::Exception("Received BYE from server: " + parsedResponse->errorMessage);
    } else if (parsedResponse->status != CIMAPParse::RespCode::OK) {
        throw CIMAP::Exception(command + ": " + parsedResponse->errorMessage);
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

//
// Fetch a given e-mails body and subject line and create an .eml file for it.
//

static void fetchEmailAndArchive(CIMAP& imap, const std::string& mailBoxName, 
                     const CPath &destinationFolder, std::uint64_t index) {

    std::string command, commandResponse, subject, emailBody;
    CIMAPParse::COMMANDRESPONSE parsedResponse;

    command = "UID FETCH " + std::to_string(index) + " (BODY[] BODY[HEADER.FIELDS (SUBJECT)])";
    commandResponse = sendCommand(imap, mailBoxName, command);
    parsedResponse = parseCommandResponse(command, commandResponse);

    if (parsedResponse) {

        for (auto fetchEntry : parsedResponse->fetchList) {
            std::cout << "EMAIL MESSAGE NO. [" << fetchEntry.index << "]" << std::endl;
            for (auto resp : fetchEntry.responseMap) {
                if (resp.first.find("BODY[]") == 0) {
                    emailBody = resp.second;
                } else if (resp.first.find("BODY[HEADER.FIELDS (SUBJECT)]") == 0) {
                    if (resp.second.find("Subject:") != std::string::npos) { // Contains "Subject:"
                        subject = resp.second.substr(8);
                        subject = Antik::File::CMIME::convertMIMEStringToASCII(subject);
                        if (subject.length() > kMaxSubjectLine) { // Truncate for file name
                            subject = subject.substr(0, kMaxSubjectLine);
                        }
                        for (auto &ch : subject) { // Remove all but alpha numeric from subject
                            if (!std::isalnum(ch)) ch = ' ';
                        }
                    }
                }
            }
        }

        // Have email body so create .eml file for it.

        if (!emailBody.empty()) {
            CPath fullFilePath { destinationFolder };
            fullFilePath.join("(" + std::to_string(index) + ") " + subject + kEMLFileExt);
            if (!CFile::exists(fullFilePath)) {
                std::istringstream emailBodyStream(emailBody);
                std::ofstream emlFileStream(fullFilePath.toString(), std::ios::binary);
                if (emlFileStream.is_open()) {
                    std::cout << "Creating [" << fullFilePath.toString() << "]" << std::endl;
                    for (std::string line; std::getline(emailBodyStream, line, '\n');) {
                        line.push_back('\n');
                        emlFileStream.write(&line[0], line.length());
                    }
                } else {
                    std::cerr << "Failed to create file [" << fullFilePath.toString() << "]" << std::endl;
                }
            }
        }

    }

}

//
// Find the UID on the last message saved and search from that. Each saved .eml file has a "(UID)"
// prefix; get the UID from this.
//

static std::uint64_t getLowerSearchLimit(const CPath &destinationFolder) {

    if (CFile::exists(destinationFolder) && CFile::isDirectory(destinationFolder)) {

        std::uint64_t highestUID=1, currentUID=0;
        
        Antik::FileList mailMessages { CFile::directoryContentsList(destinationFolder) };

        for (auto& mailFile : mailMessages) {
            if (CFile::isFile(mailFile) && (CPath(mailFile).extension().compare(kEMLFileExt) == 0)) {
                currentUID=std::strtoull(CIMAPParse::stringBetween(mailFile,'(', ')').c_str(), nullptr, 10);
                if (currentUID > highestUID) {
                    highestUID = currentUID;
                } 
            }
        }
        
        return (highestUID);
        
    }
    
    return(0);

}

//
// Convert list of comma separated mailbox names / list all mailboxes and place into vector or mailbox name strings.
//

static void createMailBoxList(CIMAP& imap, const ParamArgData& argData, 
                       std::vector<std::string>& mailBoxList) {

    if (argData.bAllMailBoxes) {
        std::string command, commandResponse;
        CIMAPParse::COMMANDRESPONSE parsedResponse;

        command = "LIST \"\" *";
        commandResponse = sendCommand(imap, "", command);
        parsedResponse = parseCommandResponse(command, commandResponse);

        if (parsedResponse) {

            for (auto mailBoxEntry : parsedResponse->mailBoxList) {
                if (mailBoxEntry.mailBoxName.front() == ' ') mailBoxEntry.mailBoxName = mailBoxEntry.mailBoxName.substr(1);
                if (mailBoxEntry.attributes.find("\\Noselect") == std::string::npos) {
                    mailBoxList.push_back(mailBoxEntry.mailBoxName);
                }
            }

        }
    } else {
        std::istringstream mailBoxStream(argData.mailBoxName);
        for (std::string mailBox; std::getline(mailBoxStream, mailBox, ',');) {
            mailBox = mailBox.substr(mailBox.find_first_not_of(' '));
            mailBox = mailBox.substr(0, mailBox.find_last_not_of(' ') + 1);
            mailBoxList.push_back(mailBox);
        }
    }
}

// ============================
// ===== MAIN ENTRY POINT =====
// ============================

int main(int argc, char** argv) {

    try {

        ParamArgData argData;
        CIMAP imap;
        std::vector<std::string> mailBoxList;

        // Read in command line parameters and process

        procCmdLine(argc, argv, argData);

        // Set mail account user name and password

        imap.setServer(argData.serverURL);
        imap.setUserAndPassword(argData.userName, argData.userPassword);

        // Connect

        std::cout << "Connecting to server [" << argData.serverURL << "]" << std::endl;

        imap.connect();

        // Create mailbox list

        createMailBoxList(imap, argData, mailBoxList);

        for (std::string mailBox : mailBoxList) {

            CIMAPParse::COMMANDRESPONSE parsedResponse;
            CPath mailBoxPath { argData.destinationFolder };
            std::string command, commandResponse;
            std::uint64_t searchUID=0;
            
            std::cout << "MAIL BOX [" << mailBox << "]" << std::endl;

            // SELECT mailbox

            command = "SELECT " + mailBox;
            commandResponse = sendCommand(imap, mailBox, command);
            parsedResponse = parseCommandResponse(command, commandResponse);

            // Clear any quotes from mailbox name for folder name

            if (mailBox.front() == '\"') mailBox = mailBox.substr(1);
            if (mailBox.back() == '\"') mailBox.pop_back();

            // Create destination folder

            mailBoxPath.join(mailBox);
            if (!argData.destinationFolder.empty() && !CFile::exists(mailBoxPath)) {
                std::cout << "Creating destination folder = [" << mailBoxPath.toString() << "]" << std::endl;
                CFile::createDirectory(mailBoxPath);
            }

            // Get UID of newest archived message and search from that for updates

            if (argData.bOnlyUpdates) {
                searchUID = getLowerSearchLimit(mailBoxPath);
            }

            // SEARCH for email.

            if (searchUID!=0) { // Updates
                std::cout << "Searching from [" << std::to_string(searchUID) << "]" << std::endl;
                command = "UID SEARCH UID "+std::to_string(searchUID)+":*";
            } else {            // All
                command = "UID SEARCH UID 1:*";
            }

            commandResponse = sendCommand(imap, mailBox, command);
            
            // Archive any email returned from search
            
            parsedResponse = parseCommandResponse(command, commandResponse);
            if (parsedResponse) {
                for (auto index : parsedResponse->indexes) {
                    if (index != searchUID) {
                        fetchEmailAndArchive(imap, mailBox, mailBoxPath, index);
                    }
                }
            }

        }

        std::cout << "Disconnecting from server [" << argData.serverURL << "]" << std::endl;

        imap.disconnect();

    //
    // Catch any errors
    //    

    } catch (const std::exception &e) {
        exitWithError(e.what());
    }

    exit(EXIT_SUCCESS);


}

