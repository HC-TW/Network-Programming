# Network-Programming

## Project 1: NPShell - Design a shell with special piping mechanisms
### Functions needed to use:
  - fork
  - pipe
  - dup, dup2
  - exec*
  - wait, waitpid
 
## Project 2: RWG system - Chat-like system
### Requirements:
- Part 1: Design a Concurrent connection-oriented server. This server allows one client connect to it.
- Part 2: Design a server of the chat-like systems, called remote working systems (rwg). In this system, users can communicate with other users. You need to use the single-process concurrent paradigm to design this server.
- Pipe between different users. Broadcast message whenever a user pipe is used. 
- Broadcast message of login/logout information.
- New commands:
  - who: show information of all users. 
  - tell: send a message to another user. 
  - yell: send a message to all users.
  - name: change your name.

## Project 3: Remote batch system 
### Requirements:
- Part 1: Design a remote batch system, which consists of a simple HTTP server called http server and a CGI program console.cgi.
- Part 2: Implement one program, cgi server.exe, which is a combination of http server, panel.cgi, and console.cgi.
         Your program should run on Windows 10.
- Every function that touches network operations (e.g., DNS query, connect, accept, send, receive) MUST be implemented using the library **Boost.Asio**.
- All of the network operations should implement in **non-blocking (asynchronous)** approaches.
 
## Project 4: SOCKS 4
### Requirements:
 - SOCKS 4 Server Connect Operation
 - SOCKS 4 Server Bind Operation
 - CGI Proxy
 - Firewall
 
