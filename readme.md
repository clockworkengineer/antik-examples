
# Example Programs #

This repository contains is a small but growing library of example programs  that use the antik classe library:

1.  **[IMAPCommandTerminal](https://github.com/clockworkengineer/antik-examples/blob/master/IMAPCommandTerminal.cpp)**  A Simple IMAP command console/terminal that logs on to a given IMAP server and executes commands typed in. The raw command responses are echoed back as default but 
parsed responses are displayed if specified in program options.

1.  **[DownloadAllAttachments](https://github.com/clockworkengineer/antik-examples/blob/master/DownloadAllAttachments.cpp)** Log on to a given IMAP server and download attachments found in any e-mail in a specific mailbox to a given local folder. The final destination folder is a base name with the mailbox name attached.
  
1.  **[ArchiveMailBox](https://github.com/clockworkengineer/antik-examples/blob/master/ArchiveMailBox.cpp)** Log on to a given IMAP server and download all e-mails for a given mailbox and create an .eml file for them  in a specified destination folder. The .eml files are created within a sub-folder with the mailbox name and with filenames consisting of the mail UID prefix and the subject line. If parameter --updates is set  then the date of the newest .eml in the destination folder is used as the basis of the IMAP search (ie. only download new e-mails). Note: MIME encoded words in the email subject line are decoded to the best ASCII fit available.

1. **[WaitForMailBoxEvent](https://github.com/clockworkengineer/antik-examples/blob/master/WaitForMailBoxEvent.cpp)** Log on to a IMAP server and wait for a status change in a specified mailbox. By default it will use IDLE but polling every time period using NOOP is also supported. This is not directly useful but may be applied to other situations where the functionality is needed.

1. **[SMTPSendMail](https://github.com/clockworkengineer/antik-examples/blob/master/SMTPSendMail.cpp)** A command line program to log on to an SMTP server and send an email to given recipients. The mails details such as contents, subject and any attachments are configured via command line arguments.

1. **[ArchiveFolder](https://github.com/clockworkengineer/antik-examples/blob/master/ArchiveFolder.cpp)** A command line program that writes the contents of a source folder to a ZIP archive; traversing it recursively and adding any sub-folder contents. It compresses each file with deflate unless its size does not decrease in which case it simply stores the file.

1. **[ExtractToFolder](https://github.com/clockworkengineer/antik-examples/blob/master/ExtractToFolder.cpp)** A command line program that extracts the contents of a ZIP archive to a specified destination folder. Note: Any destination folders are created by the program before a file is extracted as the class will not do this. 

1. **[ZIPArchiveInfo](https://github.com/clockworkengineer/antik-examples/blob/master/ZIPArchiveInfo.cpp)**  This is a command line program to scan a ZIP archive and output information about it.

1. **[FTPBackup](https://github.com/clockworkengineer/antik-examples/blob/master/FTPBackup.cpp)**  
  Simple FTP backup program that takes a local directory and backs it up to a specified FTP server using account details provided.

1. **[FTPResore](https://github.com/clockworkengineer/antik-examples/blob/master/FTPResore.cpp)** 
 Simple FTP restore program (companion program to FTPBackup) that takes a remote FTP server directory and restores it to a local directory.
