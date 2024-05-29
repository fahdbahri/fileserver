Simple File Server
This repository contains a simple file server written in C. The server supports basic file operations and requires user authentication.

Features
Authentication: Users must authenticate with a username and password.
File Operations:
- LIST: List directory contents.
- GET: Download a file.
- PUT: Upload a file.
- DEL: Delete a file.
- QUIT: Disconnect from the server.


Getting Started
Prerequisites
POSIX-compliant system (Linux, macOS)
C compiler (e.g., gcc)


Compilation
Compile the server with:
gcc -o file_server file_server.c -lpthread


Running the Server
Run the server with:
./file_server -d <directory> -p <port> -u <password_file>


Example:
./file_server -d /path/to/directory -p 4529 -u /path/to/passwordfile


Password File Format

Entries in the password file should be in the format username:password, each on a new line:
user1:password1
user2:password2


Usage
Connect to the server using a client (e.g., telnet):
nc localhost 4529


Available commands:

USER: Authenticate with the server.
USER <username> <password>


LIST: List directory contents.
LIST



GET: Download a file.
GET <filename>


PUT: Upload a file (end with \r\n.\r\n).
PUT <filename>
<file content>
\r\n.\r\n


DEL: Delete a file.
DEL <filename>


QUIT: Disconnect from the server.
QUIT


License
This project is licensed under the MIT License. See the LICENSE file for details.
