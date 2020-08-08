#include "HOST.hpp"
/*
 * File:   SMTPSendMail.cpp
 * 
 * Author: Robert Tizzard
 * 
 * Created on March 9th, 2017, 2:33 PM
 *
 * Copyright 2017.
 *
 */

//
// Program: SMTPSendMail
//
// Description:  A command line program to log on to an SMTP server and
// send an email to given recipients. The mails details such as contents,
// subject and any attachments are configured via command line arguments.
// All parameters and their meaning are obtained by running the program 
// with the  parameter --help.
//
// SMTPSendMail Example Application
// Program Options:
//   --help                   Print help messages
//   -c [ --config ] arg      Config File Name
//   -s [ --server ] arg      SMTP Server URL and port
//   -u [ --user ] arg        Account username
//   -p [ --password ] arg    User password
//   -r [ --recipients ] arg  Recipients list
//   -s [ --subject ] arg     Email subject
//   -c [ --contents ] arg    File containing email contents
//   -a [ --attachments ] arg File Attachments List
//
// Dependencies: C11++, Classes (CMailSMTP, CFileMIME, CFile, CPath),
//               Linux, Boost C++ Libraries.
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

#include "CSMTP.hpp"
#include "CMIME.hpp"
#include "CPath.hpp"
#include "CFile.hpp"

using namespace Antik::SMTP;
using namespace Antik::File;

//
// Boost program options library
//

#include "boost/program_options.hpp" 

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
    std::string configFileName;    // Configuration file name
    std::string recipients;        // List of recipeints
    std::string subject;           // Email subject
    std::string mailContentsFile;  // File containing email contents
    std::string attachmentList;    // List of attachments
};

// ===============
// LOCAL FUNCTIONS
// ===============

//
// Exit with error message/status
//

static void exitWithError(std::string errMsg) {

    // Closedown SMTP transport and display error and exit.

    CSMTP::closedown();
    std::cerr << errMsg << std::endl;
    exit(EXIT_FAILURE);

}

//
// Add options common to both command line and config file
//

static void addCommonOptions(po::options_description& commonOptions, ParamArgData& argData) {

    commonOptions.add_options()
            ("server,s", po::value<std::string>(&argData.serverURL)->required(), "SMTP Server URL and port")
            ("user,u", po::value<std::string>(&argData.userName)->required(), "Account username")
            ("password,p", po::value<std::string>(&argData.userPassword)->required(), "User password")
            ("recipients,r", po::value<std::string>(&argData.recipients)->required(), "Recipients list")
            ("subject,b", po::value<std::string>(&argData.subject)->required(), "Email subject")
            ("contents,o", po::value<std::string>(&argData.mailContentsFile)->required(), "File containing email contents")
            ("attachments,a", po::value<std::string>(&argData.attachmentList)->required(), "File Attachments List");

}

//
// Read in and process command line arguments using boost.
//

static void procCmdLine(int argc, char** argv, ParamArgData &argData) {

    // Default values


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
            std::cout << "SMTPSendMail Example Application" << std::endl << commandLine << std::endl;
            exit(0);
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
        std::cerr << "SMTPSendMail Error: " << e.what() << std::endl << std::endl;
        std::cerr << commandLine << std::endl;
        exit(1);
    }

}

// ============================
// ===== MAIN ENTRY POINT =====
// ============================

int main(int argc, char** argv) {

    try {

        CSMTP mail;
        std::vector<std::string> mailMessage;

        // Read in command lien parameters and process

        ParamArgData argData;
        procCmdLine(argc, argv, argData);

        // Initialise SMTP transport
        
        CSMTP::init(true);

        // Set server and account details
        
        mail.setServer(argData.serverURL);
        mail.setUserAndPassword(argData.userName, argData.userPassword);

        // Set Sender and recipients
        
        mail.setFromAddress("<" + argData.userName + ">");
        mail.setToAddress(argData.recipients);

        // Set mail subject
        
        mail.setMailSubject(argData.subject);

        // Set mail contents
        
        if (!argData.configFileName.empty()) {
            if (CFile::exists(argData.mailContentsFile)) {
                std::ifstream mailContentsStream(argData.mailContentsFile);
                if (mailContentsStream.is_open()) {
                    for (std::string line; std::getline(mailContentsStream, line, '\n');) {
                        mailMessage.push_back(line);
                    }
                    mail.setMailMessage(mailMessage);
                }
            }
        }

        // Add any attachments. Note all base64 encoded.
        
        if (!argData.attachmentList.empty()) {
            std::istringstream attachListStream(argData.attachmentList);
            for (std::string attachment; std::getline(attachListStream, attachment, ',');) {
                attachment = attachment.substr(attachment.find_first_not_of(' '));
                attachment = attachment.substr(0, attachment.find_last_not_of(' ') + 1);
                if (CFile::exists(attachment)){
                    std::cout << "Attaching file [" << attachment << "]" << std::endl;
                    mail.addFileAttachment(attachment, Antik::File::CMIME::getFileMIMEType(attachment), "base64");
                } else {
                    std::cout << "File does not exist [" << attachment << "]" << std::endl;
                }

            }
        }
      
        // Send mail
        
        mail.postMail();
        
    //
    // Catch any errors
    //    

    } catch (const std::exception & e) {
        exitWithError(e.what());
    }

    // Closedown SMTP transport
    
    CSMTP::closedown();

    exit(EXIT_SUCCESS);

}


