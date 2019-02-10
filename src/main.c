// Matthew O'Connell
// ECE 4470 Computer Networks
// Lab 2

// Client-side UDP Code 
// Written by Sarvesh Kulkarni <sarvesh.kulkarni@villanova.edu>
// Adapted for use from "Beej's Guide to Network Programming" (C) 2017


#include <stdio.h>		// Std i/o libraries - obviously
#include <stdlib.h>		// Std C library for utility fns & NULL defn
#include <unistd.h>		// System calls, also declares STDOUT_FILENO
#include <errno.h>	    // Pre-defined C error codes (in Linux)
#include <string.h>		// String operations - surely, you know this!
#include <sys/types.h>  // Defns of system data types
#include <sys/socket.h> // Unix Socket libraries for TCP, UDP, etc.
#include <netinet/in.h> // INET constants
#include <arpa/inet.h>  // Conversion of IP addresses, etc.
#include <netdb.h>		// Network database operations, incl. getaddrinfo
#include <math.h>		// ceil function
#include <sys/stat.h>   // mkdir function

// Our constants ..
#define MAXBUF 10000      // 4K max buffer size for i/o over nwk
#define SRVR_PORT "5555"  // the server's port# to which we send data
						  // NOTE: ports 0 -1023 are reserved for superuser!
// Struct for all packets after the first
// Segments different components of the packet for easier processing
struct DataPacket{
	int packetNumber;
	int LPFlag;
	int payloadSize;
	char payload[100];
};

// Function declarations
int getFileSize(char buffer[]);
void getFileName(char buffer[], char fileName[]);
struct DataPacket processDataPacket(char buffer[]);
void sortPacketOrder(struct DataPacket packets[], int numPackets);
void charArrayCopy(char source[], char destination[], int size);

int main(int argc, char *argv[]) {
    int sockfd;			 // Socket file descriptor; much like a file descriptor
    struct addrinfo hints, *servinfo, *p; // Address structure and ptrs to them
    int rv, nbytes, nread;
	char buf[MAXBUF];    // Size of our network app i/o buffer

    if (argc != 3) {
        fprintf(stderr,"ERROR! Correct Usage is: ./program_name server userid\n"
		        "Where,\n    server = server_name or ip_address, and\n"
		        "    userid = your LDAP (VU) userid\n");
        exit(1);
    }

	// First, we need to fill out some fields of the 'hints' struct
    memset(&hints, 0, sizeof hints); // fill zeroes in the hints struc
    hints.ai_family = AF_UNSPEC;     // AF_UNSPEC means IPv4 or IPv6; don't care
    hints.ai_socktype = SOCK_DGRAM;  // SOCK_DGRAM means UDP

	// Then, we call getaddrinfo() to fill out other fields of the struct 'hints
	// automagically for us; servinfo will now point to the addrinfo structure
	// of course, if getaddrinfo() fails to execute correctly, it will report an
	// error in the return value (rv). rv=0 implies no error. If we do get an
	// error, then the function gai_strerror() will print it out for us
    if ((rv = getaddrinfo(argv[1], SRVR_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // We start by pointing p to wherever servinfo is pointing; this could be
	// the very start of a linked list of addrinfo structs. So, try every one of
	// them, and open a socket with the very first one that allows us to
	// Note that if a socket() call fails (i.e. if it returns -1), we continue
	// to try opening a socket by advancing to the next node of the list
	// by means of the stmt: p = p->ai_next (ai_next is the next ptr, defined in
	// struct addrinfo).
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("CLIENT: socket");
            continue;
        }

        break;
    }

	// OK, keep calm - if p==NULL, then it means that we cycled through whole
	// linked list, but did not manage to open a socket! We have failed, and
	// with a deep hearted sigh, accept defeat; with our tail between our legs,
	// we terminate our program here with error code 2 (from main).
    if (p == NULL) {
        fprintf(stderr, "CLIENT: failed to create socket\n");
        return 2;
    }

	// If p!=NULL, then things are looking up; the OS has opened a socket for
	// us and given us a socket descriptor. We are cleared to send! Hurray!
	// The sendto() function will report how many bytes (nbytes) it sent; but a
	// negative value (-1) means failure. Sighhh. 
    if ((nbytes = sendto(sockfd, argv[2], strlen(argv[2]), 0,
             p->ai_addr, p->ai_addrlen)) == -1) {
        perror("CLIENT: sendto");
        exit(1);
    }

    printf("CLIENT: sent '%s' (%d bytes) to %s\n", argv[2], nbytes, argv[1]);


	// Recv packet from server. YOu should modify this part of the program so that
	// you can receive more than 1 packet from the server. In fact, you should
	// call recvfrom() repeatedly till all parts of the file have been received.
	nread = recvfrom(sockfd,buf,MAXBUF,0,NULL,NULL);
	if (nread<0) {
		perror("CLIENT: Problem in recvfrom");
		exit(1);
	}
	// we recvd our very first packet from sender: <filesize,filename>
	printf("Received %d bytes\n\n",nread);
	// Output recvd data to stdout; 
	 if (write(STDOUT_FILENO,buf,nread) < 0) {
	  perror("CLIENT: Problem writing to stdout");
		exit(1);
	}
	// instantiate fileName char array, then populate with getFileName function
	char fileName[92];
	getFileName(buf,fileName);
	FILE *file;
	// construct relative file path for data_files directory
	char filePath[] = {"data_files/"};
	// if data_files directory does not exits, create it with universal user access
	mkdir(filePath, 0777);
	// append full file path and create file
	strcat(filePath,fileName);
	file = fopen(filePath, "w");

	// calls user-defined function to determine size of transmitted file
	int fileSize = getFileSize(buf);
	// 100 data bits max per packet, so this tells us how many packets will need to be sent
	// rounded up to include packets <100 bits in data size
	int numPackets = (fileSize / 100)+1;
	// array of data packet structs used to compartmentalize the different components of each data packet
	struct DataPacket packets[numPackets];

	// loop to call recvfrom to collect the entire file from the server
	for(int i = 0; i < numPackets; i++){
		nread = recvfrom(sockfd,buf,MAXBUF,0,NULL,NULL);
			if (nread<0) {
				perror("CLIENT: Problem in recvfrom");
				exit(1);
			}
		if (write(STDOUT_FILENO,buf,nread) < 0) {
			  perror("CLIENT: Problem writing to stdout");
				exit(1);
			}
		// processes data packets into DataPacket structs
		packets[i] = processDataPacket(buf);
	}
	// sorts packets into correct order based on Packet # field
	sortPacketOrder(packets, numPackets);
	// iterates over each packet, printing buffer contents to file
	for(int j = 0; j < numPackets; j++){
		// creates temporary char array to store the payload
		// this isolates the payload from other fields and erroneous unicode characters
		char info[100];
		charArrayCopy(packets[j].payload,info,100);
		// print to file
		fprintf(file, "%s", info);
	}



	// AFTER all packets have been received ....
	// free up the linked-list memory that was allocated for us so graciously
	// getaddrinfo() above; and close the socket as well - otherwise, bad things
	// could hapcpen
    freeaddrinfo(servinfo);
    close(sockfd);
    fclose(file);
	printf("\n\n"); // So that the new terminal prompt starts two lines below
	
    return 0;
}
// Function to determine the size of the file being transmitted over this particular FTP transfer.
int getFileSize(char buffer[]){
	int fileSizeArr[4];
	// converting to decimal numbers
	fileSizeArr[0] = buffer[0]-48;
	fileSizeArr[1] = buffer[1]-48;
	fileSizeArr[2] = buffer[2]-48;
	fileSizeArr[3] = buffer[3]-48;
	// 'concatenating' individual decimal numbers to one full filesize
	int fileSize = 1000*fileSizeArr[0] + 100*fileSizeArr[1] + 10*fileSizeArr[2] + fileSizeArr[3];
	return fileSize;
}
// Function to parse the name of the file being transmitted over the FTP transfer
void getFileName(char buffer[], char fileName[]){
	int counter = 0;
	// loops so long as the next character in the buffer does not exceed the max file name size nor is a null character
	for(int i = 8; buffer[i] != '\0' && i < 92; i++ ){
		fileName[counter] = buffer[i];
		counter++;
	}
}
// This function splits
struct DataPacket processDataPacket(char buffer[]){
	struct DataPacket ret;
	ret.packetNumber = (10*(buffer[0]-48)) + (buffer[1]-48); // takes first two bits of packet
	ret.LPFlag = buffer[2]-48; // third bit of packet
	ret.payloadSize = (100*(buffer[3]-48)) + (10*(buffer[4]-48)) + (buffer[5]-48); // bits 4,5, & 6 of packet display payload size
	// fill in payload data
	for(int i = 0; i < ret.payloadSize; i++){
		ret.payload[i] = buffer[i+6];
	}
	// if the data payload is less than 100 bytes (usually the final packet), this replaces erroneous
	// data with whitespace
	if(ret.payloadSize < 100){
		for(int j = ret.payloadSize; j < 100; j++){
			ret.payload[j] = ' ';
		}
	}
	return ret;
}
// Function to sort packets into correct order
void sortPacketOrder(struct DataPacket packets[], int numPackets){
	struct DataPacket temp[numPackets];
	for(int i = 0; i < numPackets; i++){
		temp[packets[i].packetNumber] = packets[i];
	}
	for(int j = 0; j < numPackets; j++){
		packets[j] = temp[j];
	}
}
// Function to copy one char array to another given that they are the same size
void charArrayCopy(char source[], char destination[], int size){
	for(int i = 0; i < size; i++){
		destination[i] = source[i];
	}
}
