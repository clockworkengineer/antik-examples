#include "HOST.hpp"
/*
 * File:   DownloadAllAttachments.cpp
 * 
 * Author: Robert Tizzard
 * 
 * Created on October 24, 2016, 2:33 PM
 *
 * Copyright 2016.
 *
 */

//
// Program: DownloadAllAttachments
//
// Description: A command line program to log on to an IMAP server and download
// any attachments found in any e-mails in a configured mailbox. The destination 
// for any attachment being a folder with the same name as the mailbox created 
// off of the destination folder.All parameters and their meaning are obtained 
// by running the program with the parameter --help
//
// DownloadAllAttachments Example Application
// Program Options:
//   --help                   Print help messages
//   -c [ --config ] arg      Config File Name
//   -s [ --server ] arg      IMAP Server URL and port
//   -u [ --user ] arg        Account username
//   -p [ --password ] arg    User password
//   -m [ --mailbox ] arg     Mailbox name
//   -d [ --destination ] arg Destination for attachments
// 
// Dependencies: C11++, Classes (CFile, CPath, CMailIMAP, CMailIMAPParse, 
//               CMailIMAPBodyStruct), Linux, Boost C++ Libraries.
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

#include "CIMAP.hpp"
#include "CIMAPParse.hpp"
#include "CIMAPBodyStruct.hpp"
#include "CSMTP.hpp"
#include "CPath.hpp"
#include "CFile.hpp"

using namespace Antik::IMAP;
using namespace Antik::SMTP;
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
    std::string userName;            // Email account user name
    std::string userPassword;        // Email account user name password
    std::string serverURL;           // SMTP server URL
    std::string mailBoxName;         // Mailbox name
    std::string destinationFolder;   // Destination folder for attachments
    std::string configFileName;      // Configuration file name
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
            ("mailbox,m", po::value<std::string>(&argData.mailBoxName)->required(), "Mailbox name")
            ("destination,d", po::value<std::string>(&argData.destinationFolder)->required(), "Destination for attachments");

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
            std::cout << "DownloadAllAttachments Example Application" << std::endl << commandLine << std::endl;
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
        std::cerr << "DownloadAllAttachments Error: " << e.what() << std::endl << std::endl;
        std::cerr << commandLine << std::endl;
        exit(EXIT_FAILURE);
    }

}

//
// Download an attachment, decode it and write to local folder.
//

static void downloadAttachment(CIMAP& imap, const CPath &destinationFolder, CIMAPBodyStruct::Attachment &attachment) {

    std::string commandLine("FETCH " + attachment.index + " BODY[" + attachment.partNo + "]");
    std::string parsedResponseStr(imap.sendCommand(commandLine));
    CIMAPParse::COMMANDRESPONSE parsedResponse(CIMAPParse::parseResponse(parsedResponseStr));

    if ((parsedResponse->status == CIMAPParse::RespCode::BAD) ||
            (parsedResponse->status == CIMAPParse::RespCode::NO)) {
        throw CIMAP::Exception("IMAP FETCH "+parsedResponse->errorMessage);
    }

    for (auto fetchEntry : parsedResponse->fetchList) {

        for (auto resp : fetchEntry.responseMap) {

            if (resp.first.find("BODY[" + attachment.partNo + "]") == 0) {  
                
                CPath fullFilePath = { destinationFolder };
                fullFilePath.join(attachment.fileName);

                if (!CFile::exists(fullFilePath)) {
                    std::string decodedString;
                    std::istringstream responseStream(resp.second);
                    std::ofstream attachmentFileStream(fullFilePath.toString(), std::ios::binary);
                    if (attachmentFileStream.is_open()) {
                        std::cout << "Creating [" << fullFilePath.toString() << "]" << std::endl;
                        // Encoded lines have terminating '\r\n' the getline removes '\n'
                        for (std::string line; std::getline(responseStream, line, '\n');) {
                            line.pop_back(); // Remove '\r'
                            CSMTP::decodeFromBase64(line, decodedString, line.length());
                            attachmentFileStream.write(&decodedString[0], decodedString.length());
                        }
                    } else {
                        std::cout << "Failed to create file [" << fullFilePath.toString() << "]" << std::endl;
                    }
                }

            }
        }
    }
}

//
// For a passed in BODTSTRUCTURE parse and download any base64 encoded attachments.
//

static void getBodyStructAttachments(CIMAP& imap, std::uint64_t index, const CPath &destinationFolder, const std::string& bodyStructure) {

    std::unique_ptr<CIMAPBodyStruct::BodyNode> treeBase{ new CIMAPBodyStruct::BodyNode()};
    std::shared_ptr<void> attachmentData{ new CIMAPBodyStruct::AttachmentData()};

    CIMAPBodyStruct::consructBodyStructTree(treeBase, bodyStructure);
    CIMAPBodyStruct::walkBodyStructTree(treeBase, CIMAPBodyStruct::attachmentFn, attachmentData);

    auto attachments = static_cast<CIMAPBodyStruct::AttachmentData *> (attachmentData.get());

    if (!attachments->attachmentsList.empty()) {
        for (auto attachment : attachments->attachmentsList) {
            if (CIMAPParse::stringStartsWith (attachment.encoding, CSMTP::kEncodingBase64)) {
                attachment.index = std::to_string(index);
                downloadAttachment(imap, destinationFolder, attachment);
            } else {
                std::cout << "Attachment not base64 encoded but " << attachment.encoding << "]" << std::endl;
            }
        }
    } else {
        std::cout << "No attachments present." << std::endl;
    }


}

// ============================
// ===== MAIN ENTRY POINT =====
// ============================

int main(int argc, char** argv) {

    try {

        ParamArgData argData;
        CIMAP imap;
        std::string comandResponse;
        CIMAPParse::COMMANDRESPONSE  parsedResponse;

        // Read in command line parameters and process

        procCmdLine(argc, argv, argData);

        // Set mail account user name and password
        
        imap.setServer(argData.serverURL);
        imap.setUserAndPassword(argData.userName, argData.userPassword);

       // Create destination folder

        argData.destinationFolder += argData.mailBoxName;
        if (!argData.destinationFolder.empty() && !CFile::exists(argData.destinationFolder)) {
            std::cout << "Creating destination folder = [" << argData.destinationFolder << "]" << std::endl;
            CFile::createDirectory(argData.destinationFolder);
        }
        
        // Connect
 
        std::cout << "Connecting to server [" << argData.serverURL << "]" << std::endl;
               
        imap.connect();

        // SELECT mailbox
        
        comandResponse=imap.sendCommand("SELECT "+argData.mailBoxName);
        parsedResponse = CIMAPParse::parseResponse(comandResponse);
        if (parsedResponse->status != CIMAPParse::RespCode::OK) {
            throw CIMAP::Exception("IMAP SELECT "+parsedResponse->errorMessage);
        } else if (parsedResponse->byeSent) {
            throw CIMAP::Exception("Received BYE from server: " + parsedResponse->errorMessage);
        }

        // FETCH BODYSTRUCTURE for all mail
        
        comandResponse=imap.sendCommand("FETCH 1:* BODYSTRUCTURE");
        parsedResponse = CIMAPParse::parseResponse(comandResponse);
        if (parsedResponse->status != CIMAPParse::RespCode::OK) {
            throw CIMAP::Exception("IMAP FETCH "+parsedResponse->errorMessage);
        }  else if (parsedResponse->byeSent) {
            throw CIMAP::Exception("Received BYE from server: " + parsedResponse->errorMessage);
        }

        std::cout << "COMMAND = " << CIMAPParse::commandCodeString(parsedResponse->command) << std::endl;

        //  Take decoded response and get any attachments specified in BODYSTRUCTURE.
        
        for (auto fetchEntry : parsedResponse->fetchList) {
            std::cout << "EMAIL INDEX [" << fetchEntry.index << "]" << std::endl;
            for (auto resp : fetchEntry.responseMap) {
                if (resp.first.compare(kBODYSTRUCTURE) == 0) {
                    getBodyStructAttachments(imap, fetchEntry.index, argData.destinationFolder,resp.second);
                } else {
                    std::cout << resp.first << " = " << resp.second << std::endl;
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
