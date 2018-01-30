#include <stdio.h>
#include <cstdio>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <time.h>
#include <iostream>
#include <string>
#include <set>
#include <map>
#include <sstream>

using namespace std;

// port to listen
#define PORT "9048"

// get sockaddr, IPv4/IPv6
void *get_in_addr(struct sockaddr *socket_addr)
{
	if (socket_addr->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)socket_addr)->sin_addr);
	}
	return &(((struct sockaddr_in6*)socket_addr)->sin6_addr);
}

struct tftp_packet
{
	int16_t opcode;
	int16_t blockno;
	int16_t errcode;
	char filename[30];
};

struct client_info
{
	struct sockaddr_in client_addr;
	char filename[30];
	struct timeval time_start, time_end;
	int last_ack;
	FILE *fp;
	FILE *fp_end;
	int16_t block_num;
	int block_size = false;
	//bool retran ;
};

void packi16 (char *buf, unsigned short int i)
{
	i = htons(i);
	memcpy(buf,&i,2);
}

unsigned short int unpacki16(char *buf)
{
	unsigned short int i;
	memcpy(&i,buf,2);
	i = ntohs(i);
	return i;
}

int main(int argc, char* argv[])
{
	if(argc!=3)
	{
		cout<<"please enter server, server_ip_address, server_port \n"<<endl;
	}
	fd_set master; // master file descriptor list
	fd_set read_fds; // temp file descriptor list for select()
	/*struct SBCP_packet packet_recv, packet_send;
	//set<string> user_list;
	char user_name[16];
	char message[512];
	char reason[32];
	int client_count = 0;
	string msg="";*/

	map<int,client_info*> list_of_clients;
	struct client_info *client;
	int fdmax; // maximum file descriptor number
	int listener; // listening socket descriptor
	int newfd; // newly accept()ed socket descriptor
	struct sockaddr_in serveraddr, clientaddr; // client address
	socklen_t addrlen, templen;
	char buf[700]; // buffer for incoming data
	//char buf_send[700]; //buffer for outgoing data
	//int nbytes;
	char remoteIP[INET6_ADDRSTRLEN];
	//int yes=1; // for setsockopt() SO_REUSEADDR, below
	int i, j, rv;
	struct addrinfo hints, *ai, *p;

	FD_ZERO(&master); // clear the master and temp sets
	FD_ZERO(&read_fds);

	// get us a socket and bind it
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &ai)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}

	for(p = ai; p != NULL; p = p->ai_next)
	{
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0)
		{
			perror("socket error");
			continue;
		}

		// lose the pesky "address already in use" error message
		//setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0)
		{
			close(listener);
			perror("bind error");
			continue;
		}
		break;
	}

	// if we got here, it means we didn't get bound
	if (p == NULL)
	{
		fprintf(stderr, "selectserver: failed to bind\n");
		exit(2);
	}

	freeaddrinfo(ai); // all done with this

	/* listen
	if (listen(listener, 100) == -1)
	{
		perror("listen");
		exit(3);
	}*/

	// add the listener to the master set
	FD_SET(listener, &master);
	// keep track of the biggest file descriptor
	fdmax = listener; // so far, it's this one

	// main loop
		for(;;)
		{
			//cout<<"entering main loop"<<endl;
			read_fds = master; // copy it
			if (select(fdmax+1, &read_fds, NULL, NULL,NULL) == -1)
			{
				perror("select");
				exit(4);
			}
			//cout<<"listener:"<<listener<<endl;

			//put check for timeout
			for (auto it = list_of_clients.begin(); it != list_of_clients.end(); it++)
			{
				gettimeofday(&(it->second->time_end), NULL);
				double interval = it->second->time_end.tv_usec - it->second->time_start.tv_usec;
				if (interval >= 100000)
				{
					cout << "timeout for filename " << it->second->filename << endl;
					//it->second->retran = true;
					fseek(it->second->fp, -it->second->block_size, SEEK_CUR);
					uint16_t opcode = htons(3);
					uint16_t block_num = htons(it->second->block_num);
					char buf_send[600] = { 0 };
					memcpy(buf_send, &opcode, 2);
					memcpy(buf_send + 2, &block_num, 2);


					int size = ftell(it->second->fp_end) - ftell(it->second->fp);

					if (size >= 512)
						size = 512;
					else
						it->second->last_ack = 1;

					fread(buf_send + 4, 1, size, it->second->fp);
					sendto(i, buf_send, size + 4, 0, (struct sockaddr *)&(it->second->client_addr), addrlen);
					cout << "retransmitting block num= " << it->second->block_num << endl;

				}
			}

			// run through the existing connections looking for data to read
			for(i = 0; i <= fdmax; i++)
			{
				if (FD_ISSET(i, &read_fds)) // we have a client
				{
					if(i==listener) //new client
					{
						addrlen = sizeof(clientaddr);
						if(recvfrom(listener, buf, 512,0, (struct sockaddr *)&clientaddr ,(socklen_t*)&addrlen)==-1)
						{
							perror("recvfrom");
									return -1;
						}

						//read packet
						struct tftp_packet *received;
						received = (struct tftp_packet *)malloc(sizeof(struct tftp_packet));
						received->opcode = unpacki16(buf);

						//action according to opcode
						if(received->opcode==1) //RRQ
						{
							char temp[30];
							int index=2, temp_index=0;
							while(buf[index]!='\0')
							{
								temp[temp_index++] = buf[index++];
							}
							temp[temp_index]='\0';
							//received->filename = (char *)malloc(strlen(temp)*sizeof(char));
							strcpy(received->filename,temp);
							newfd = socket(AF_INET, SOCK_DGRAM, 0); //create new socket
							
							cout<<"Received RRQ for file "<<received->filename<<" on socket "<<newfd<<endl;
							
							if (newfd<0)
							{
								perror("accept");
								return -1;
							}
							FD_SET(newfd, &master);
							if (newfd > fdmax)
								fdmax = newfd;
							srand(time(NULL));
							short int client_port = rand() % 1001 + 3000;

							memset(&serveraddr, 0, sizeof(serveraddr));
							serveraddr.sin_family = AF_INET;
							serveraddr.sin_port = htons(client_port);
							serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
							if (bind(newfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr))<0)
							{
								perror("bind");
								return 1;
							}
							//fill the client information
							client = new client_info;
							strcpy(client->filename, received->filename);
							client->last_ack = 0;
							client->client_addr = clientaddr;
							list_of_clients[newfd] = client;
							list_of_clients[newfd]->fp = fopen(received->filename, "r");

							if (list_of_clients[newfd]->fp == NULL) //failed to open the file
							{
								uint16_t opcode = htons(5);
								uint16_t err_code = htons(1);
								char buf_send[100] = {0};
								memcpy(buf_send, &opcode, 2);
								memcpy(buf_send+2, &err_code, 2);
								memcpy(buf_send+4, "File not found", strlen("File not found"));
								addrlen = sizeof(struct sockaddr_in);
								if (sendto(newfd, buf_send, 100, 0, (struct sockaddr *)&list_of_clients[newfd]->client_addr, addrlen)!=-1)
								{
									cout << "Error Message sent: failed to open file"<<endl;
								}
								else
								{
									cout << "Error Message failed to send"<<endl;
								}
								map<int, client_info *>::iterator it = list_of_clients.find(newfd);
								list_of_clients.erase(it);
								close(newfd);
								cout<<"connection closed due to error"<<endl;
								FD_CLR(newfd, &master);
							}
							else
							{
								uint16_t opcode = htons(3);
								uint16_t block_num = htons(1);
								char buf_send[600] = { 0 };
								memcpy(buf_send, &opcode, 2);
								memcpy(buf_send + 2, &block_num, 2);
								list_of_clients[newfd]->fp_end = list_of_clients[newfd]->fp;
								fseek(list_of_clients[newfd]->fp_end, 0, SEEK_END);
								list_of_clients[newfd]->fp = fopen(received->filename, "r");
								//fseek(list_of_clients[newfd]->fp, 0, SEEK_SET);
								//rewind(list_of_clients[newfd]->fp);
								int size = ftell(list_of_clients[newfd]->fp_end) - ftell(list_of_clients[newfd]->fp);
								//cout<<"size="<<size<<endl;
								if (size >= 512)
									size = 512;
								else
									list_of_clients[newfd]->last_ack = 1;

								list_of_clients[newfd]->block_size = size;
								list_of_clients[newfd]->block_num = 1;
								gettimeofday(&(list_of_clients[newfd]->time_start), NULL);
								fread(buf_send + 4, 1, size, list_of_clients[newfd]->fp);
							/*	if ( fgets (buf_send , 50 , list_of_clients[newfd]->fp) != NULL )
								       puts (buf_send);
								else
									cout<<"size="<<size;
								cout<<endl;*/

								sendto(newfd, buf_send, size + 4, 0, (struct sockaddr *)&list_of_clients[newfd]->client_addr, addrlen);
								cout << "sent block num= " << list_of_clients[newfd]->block_num << endl;
							}

						}
						delete(received);
					}
					else //old client
					{
						addrlen = sizeof(clientaddr);
						if(recvfrom(i, buf, 512,0, (struct sockaddr *)&clientaddr ,(socklen_t*)&addrlen)==-1)
						{
							perror("recvfrom");
							return -1;
						}
						//read packet
						struct tftp_packet *received;
						received = (struct tftp_packet *)malloc(sizeof(struct tftp_packet));
						received->opcode = unpacki16(buf);
						if ((received->opcode == 4))
						{
							if (list_of_clients[i]->last_ack) //connection close as the ACK is for the last block
							{
								fclose(list_of_clients[i]->fp);
								auto it = list_of_clients.find(i);
								list_of_clients.erase(it);
								cout<<"file transfer completed"<<endl;
								break;
							}

							received->blockno = unpacki16(buf + 2);

							if (list_of_clients[i]->block_num == received->blockno) //send the next block with block_num+1
							{
								//fseek(list_of_clients[i]->fp, -list_of_clients[i]->block_size, SEEK_CUR);
								cout<<"Received ACK for file "<<list_of_clients[i]->filename<<" on socket "<<newfd<<" for block "<<received->blockno<<endl;
								uint16_t opcode = htons(3);
								uint16_t block_num = htons(received->blockno + 1);
								char buf_send[600] = { 0 };
								memcpy(buf_send, &opcode, 2);
								memcpy(buf_send + 2, &block_num, 2);
								//list_of_clients[i]->fp_end = list_of_clients[newfd]->fp;
								//fseek(list_of_clients[i]->fp_end, 0, SEEK_END);
								int size = ftell(list_of_clients[i]->fp_end) - ftell(list_of_clients[i]->fp);
								//cout<<"size="<<size<<endl;
								if (size >= 512)
									size = 512;
								else
									list_of_clients[i]->last_ack = 1;

								list_of_clients[i]->block_size = size;
								list_of_clients[i]->block_num++;
								gettimeofday(&(list_of_clients[i]->time_start), NULL);
								fread(buf_send + 4, 1, size, list_of_clients[i]->fp);
								sendto(i, buf_send, size + 4, 0, (struct sockaddr *)&list_of_clients[i]->client_addr, addrlen);
								cout << "sent block num= " <<list_of_clients[i]->block_num << endl;

							}

						}
						delete(received);
					}

				}
			}
		}


		return 0;
}
