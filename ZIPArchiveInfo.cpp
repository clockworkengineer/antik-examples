#include "HOST.hpp"
/*
 * File:   ZIPArchiveInfo.cpp
 * 
 * Author: Robert Tizzard
 * 
 * Created on April 4, 2017, 2:33 PM
 *
 * Copyright 2016.
 *
 */

//
// Program: ZIPArchiveInfo
//
// Description: This is a command line program to scan a ZIP archive and output 
// information about it. All parameters and their meaning are obtained by running 
// the program with the parameter --help.
//
// ZIPArchiveInfo Example Application
// Command Line Options:
//   --help                      Display help message
//   -c [ --config ] arg         Config File Name
//   -z [ --zip ] arg            ZIP Archive Name
// 
// Dependencies: C11++, Classes (CFileZIPIO,CFile,CPath), Linux, Boost C++ Libraries.
//

// =============
// INCLUDE FILES
// =============

//
// C++ STL
//

#include <iostream>
#include <iomanip>

//
// Antik Classes
//

#include "CZIPIO.hpp"
#include "CPath.hpp"
#include "CFile.hpp"

using namespace Antik::ZIP;
using namespace Antik::File;

//
// BOOST program options library
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
    std::string configFileName; // Configuration file name
    std::string zipFileName;    // ZIP Archive File Name
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
            ("zip,z", po::value<std::string>(&argData.zipFileName)->required(), "ZIP Archive Name");

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
            std::cout << "ZIPArchiveInfo Example Application" << std::endl << commandLine << std::endl;
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

        // Check ZIP archive exists

        if (vm.count("zip") && !CFile::exists(vm["zip"].as<std::string>())) {
            throw po::error("Specified ZIP archive file does not exist.");
        }

        po::notify(vm);

    } catch (po::error& e) {
        std::cerr << "ZIPArchiveInfo Error: " << e.what() << std::endl << std::endl;
        std::cerr << commandLine << std::endl;
        exit(1);
    }

}

//
// Output byte array (in hex).
//

static void dumpBytes(std::vector<std::uint8_t>& bytes) {

    std::uint32_t byteCount = 1;
    std::cout << std::hex;
    for (int byte : bytes) {
        std::cout << "0x" << byte << " ";
        if (!((byteCount++)&0xF))std::cout << "\n";
    }
    std::cout << std::dec << std::endl;
}

//
// Output End Of Central Directory record information.
//

static void dumpEOCentralDirectoryRecord(CZIPIO::EOCentralDirectoryRecord& endOfCentralDirectory) {

    std::cout << "End Of Central Directory Record" << "\n";
    std::cout << "-------------------------------\n" << "\n";
    std::cout << "Start Disk Number                         : " << endOfCentralDirectory.startDiskNumber << "\n";
    std::cout << "Total Disk Number                         : " << endOfCentralDirectory.diskNumber << "\n";
    std::cout << "Number Of Central Directory Entries       : " << endOfCentralDirectory.numberOfCentralDirRecords << "\n";
    std::cout << "Total Number Of Central Directory Entries : " << endOfCentralDirectory.totalCentralDirRecords << "\n";
    std::cout << "Central Directory Offset                  : " << endOfCentralDirectory.offsetCentralDirRecords << "\n";
    std::cout << "Comment length                            : " << endOfCentralDirectory.commentLength << "\n";

    if (endOfCentralDirectory.commentLength) {
        std::cout << "Comment                                   : " << endOfCentralDirectory.comment << "\n";
    }

    std::cout << std::endl;

}

//
// Output Central Directory File Header record information.
//

static void dumpCentralDirectoryFileHeader(CZIPIO& zipFile, CZIPIO::CentralDirectoryFileHeader& fileHeader, std::uint32_t number) {

    std::cout << "Central Directory File Header No: " << number << "\n";
    std::cout << "--------------------------------\n" << "\n";

    std::cout << "File Name Length        : " << fileHeader.fileNameLength << "\n";
    std::cout << "File Name               : " << fileHeader.fileName << "\n";
    std::cout << "General Bit Flag        : " << fileHeader.bitFlag << "\n";
    std::cout << "Compressed Size         : " << fileHeader.compressedSize << "\n";
    std::cout << "Compression Method      : " << fileHeader.compression << "\n";
    std::cout << "CRC 32                  : " << fileHeader.crc32 << "\n";
    std::cout << "Creator Version         : " << fileHeader.creatorVersion << "\n";
    std::cout << "Start Disk Number       : " << fileHeader.diskNoStart << "\n";
    std::cout << "External File Attribute : " << fileHeader.externalFileAttrib << "\n";
    std::cout << "Extractor Version       : " << fileHeader.extractorVersion << "\n";
    std::cout << "File HeaderOffset       : " << fileHeader.fileHeaderOffset << "\n";
    std::cout << "Internal File Attribute : " << fileHeader.internalFileAttrib << "\n";
    std::cout << "Modification Date       : " << fileHeader.modificationDate << "\n";
    std::cout << "Modification Time       : " << fileHeader.modificationTime << "\n";
    std::cout << "Uncompressed Size       : " << fileHeader.uncompressedSize << "\n";
    std::cout << "File Comment Length     : " << fileHeader.fileCommentLength << "\n";
    std::cout << "Extra Field Length      : " << fileHeader.extraFieldLength << "\n";

    if (fileHeader.fileCommentLength) {
        std::cout << "Comment                 : " << fileHeader.fileComment << "\n";
    }

    if (fileHeader.extraFieldLength) {
        std::cout << "Extra Field             :\n";
        dumpBytes(fileHeader.extraField);
    }

    // For file header data > 32 bits display ZIP64 values.

    if (zipFile.fieldOverflow(fileHeader.compressedSize) ||
            zipFile.fieldOverflow(fileHeader.uncompressedSize) ||
            zipFile.fieldOverflow(fileHeader.fileHeaderOffset)) {

        CZIPIO::Zip64ExtendedInfoExtraField extra;

        extra.compressedSize = fileHeader.compressedSize;
        extra.fileHeaderOffset = fileHeader.fileHeaderOffset;
        extra.originalSize = fileHeader.uncompressedSize;
        std::cout << "\nZIP64 extension data :\n";
        std::cout << "+++++++++++++++++++++\n";
        zipFile.getZip64ExtendedInfoExtraField(extra, fileHeader.extraField);
        if (zipFile.fieldOverflow(fileHeader.compressedSize)) {
            std::cout << "Compressed Size         : " << extra.compressedSize << "\n";
        }
        if (zipFile.fieldOverflow(fileHeader.uncompressedSize)) {
            std::cout << "Uncompressed Size       : " << extra.originalSize << "\n";
        }
        if (zipFile.fieldOverflow(fileHeader.fileHeaderOffset)) {
            std::cout << "File HeaderOffset       : " << extra.fileHeaderOffset << "\n";
        }

    }

    std::cout << std::endl;

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

            CZIPIO zipFile;
            CZIPIO::EOCentralDirectoryRecord endOfCentralDirectory;

            //  Open zip file for read

            zipFile.openZIPFile(argData.zipFileName, std::ios::in);

            // Read End Of Central Directory and display info

            zipFile.getZIPRecord(endOfCentralDirectory);
            dumpEOCentralDirectoryRecord(endOfCentralDirectory);

            // Move to start of Central Directory and loop displaying entries.

            zipFile.positionInZIPFile(endOfCentralDirectory.offsetCentralDirRecords);

            for (auto entryNumber = 0; entryNumber < endOfCentralDirectory.numberOfCentralDirRecords; entryNumber++) {
                CZIPIO::CentralDirectoryFileHeader fileHeader;
                zipFile.getZIPRecord(fileHeader);
                dumpCentralDirectoryFileHeader(zipFile, fileHeader, entryNumber);
            }

            // Close archive.

            zipFile.closeZIPFile();

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