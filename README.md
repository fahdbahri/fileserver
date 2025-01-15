# File Server
This repository contains a simple file server written in C. The server supports basic file operations and requires user authentication.

## Features
**Authentication**: Users must authenticate with a username and password.
**File Operations**:
- LIST: List directory contents.
- GET: Download a file.
- PUT: Upload a file.
- DEL: Delete a file.
- QUIT: Disconnect from the server.


## Getting Started
### Prerequisites
- POSIX-compliant system (Linux, macOS)
- C compiler (e.g., gcc)


### Compilation
Compile the server with:
```console
gcc -o file_server file_server.c -lpthread
```


### Running the Server
Run the server with:
```console
./file_server -d <directory> -p <port> -u <password_file>
```

Example:
```console
./file_server -d /path/to/directory -p 4529 -u /path/to/passwordfile
```

### Password File Format

Entries in the password file should be in the format username:password, each on a new line:
```console
user1:password1
user2:password2
```

### Usage
Connect to the server using a client (e.g., nc):
```console
nc localhost 4529
```

Available commands:

* __USER__: Authenticate with the server.
```console
USER <username> <password>
```

* __LIST__: List directory contents.
```console
LIST
```


* __GET__: Download a file.
```console
GET <filename>
```

* __PUT__: Upload a file (end with \r\n.\r\n).
```console
PUT <filename>
<file content>
\r\n.\r\n
```

* __DEL__: Delete a file.
```console
DEL <filename>
```

* __QUIT__: Disconnect from the server.
```console
QUIT
```

### License
This project is licensed under the MIT License. See the LICENSE file for details.
