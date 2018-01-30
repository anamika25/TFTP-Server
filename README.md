First use the makefile to compile the server.cpp then use the following steps to 
run the server. For running server we need IP and PORT number.

For Server:
./server <Loopback Address> <PORT NO>

example:

./server 127.0.0.1 3310

Client:
For client part, we use the existing TFTP client for MAC. To test the code with Pumpkin, which is a well known TFTP client, we need to provide the server ip address (which is the loopback address in our case) and the port number(specified while running the server). We also need to provide the filename that we want to download.

Server Implementation Details:

getaddrinfo(): To get the structure addrinfo.
socket(): To get the file descriptor.
bind(): To associate that socket with a port.

For every new request, we get a new file descriptor and bind it to that address.
Every time we have a new File descriptor, add that into select() read_fds set.
select() API checks if there is something to read. We store client information for each client in client_info structure which will have client address, filename it wants to download,file pointers to get the block, latest received block number, block size and some parameters to check timeout.

There is a map which has file descriptor as the key and client information(structure discussed above) as value. We store all existing client connection in this map. It is useful and easy to access when we want to deal with multiple clients simultaneously once we bind that client with a particular file descriptor which is again used as the key for the map.

After establishing the connection, when client wants to download a file, it sends RRQ request to Server with filename. Server checks the received packet and if its opcode is RRQ then it reads filename. If Server does not have that file, it sends Error Packet with Error Message. If Server has that file, it divides the file into blocks of 512 and sends each block one by one but after every block sent, it waits for an ACK from client with block number. If block number is same as last sent block from Server, it sends the next one and if it does not receive the ACK, it waits for timeout period and after timeout it retransmits the same block of data again until it gets ACK for sent block from that client.

Usage:
Using this TFTP package, a client can download any file present on the server, of size not exceeding 32 MB. Several clients can download files from the server simultaneously. If the requested file is not present on the server, the client receives an error message of "failed to open file". 
