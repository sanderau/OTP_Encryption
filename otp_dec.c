#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

void error(char *msg)
//handy way to print an error
{
	perror(msg);
	exit(1);
}

void doubleString(char **str, int *l)
//double the size of the passed string while maintaining its original values
{
	char *tmp = (char *) malloc(sizeof(char)*(*l));
	strcpy(tmp, *str);

	free( *str );

	*l = *l * 2;
	*str = (char *) malloc(sizeof(char)*(*l));
	memset(*str, '\0', sizeof(char)*(*l)); // set all the values to null term
	strcpy(*str, tmp);

	free ( tmp );
}


int lengthOfFile(char *file)
//returns the length of the file
{
	FILE *fp = fopen(file, "rb");
	fseek(fp, 0, SEEK_END);
	int length = ftell(fp);
	fclose(fp);

	length +=2; // this is to account for the code i send at every message to let the server know it is done transmitting

	return length;
}

void nullTerm(char *message)
//replace the \n with \0
{
	char *pos;
	if((pos=strchr(message, '\n')) != NULL)
	{
		*pos = '\0';
	}
}

void getMessage(char *message, char *file)
//gets the message from the file
{
	FILE *fp = fopen(file, "r");
	char current;

	while((current = getc(fp)) != '\n' || '\0')
	{
		*message++ = current;
	}

	*message++ = '@';
	*message++ = '@';
	*message++ = '\0';
}


int sendall(int socketFD, char *message, int length)
//this makes sure that the message is sent in its entirety
{
	int total = 0;
	int bytesLeft = length;
	int n;

	while(total < length)
	//ensure the entire message is sent
	{
		n = send(socketFD, message+total, bytesLeft, 0);
		if(n < 0)
		{
			error("OTP_ENC: ERROR: There was a problem sending the message!");
			exit(1);
		}
		total += n;
		bytesLeft -= n;
	}

	return n==-1?-1:0; // return -1 on fail, 0 on success
}


int getall(int socketFD, char *buff, char *message)
//this will get the entire message from the client.
{
	int r, total = 0;
	memset(message, '\0', sizeof(message));

	while (strstr(message, "@@") == NULL) // keep executing until we find null terminator for transmission
	{
		memset(buff, '\0', sizeof(buff)); //clear the buffer
		r = recv(socketFD, buff, sizeof(buff)-1, 0); //receive a chunk of the data
		total += r;
		strcat(message, buff); // add newly recieved chunk
		if(r == -1) {printf("r == -1\n"); break;}
		if(r == 0) {printf("r == 0\n"); break;}
	}

	int term = strstr(message, "@@") - message;
	message[term] = '\0';

	return total-2; //minus 2 to account for code
}



int main(int argc, char *argv[])
{
	if(argc < 4)
	//if program isnt executed with correct number of params, throw error
	{
		fprintf(stderr, "ERROR: %s [MESSAGE FILE] [KEY FILE] [PORT]\n", argv[0]);
		exit(1);
	}

	//variables used to make encryption client
	int socketFD, portNumber, charsWritten, charsRead; // socket for client; port number of server we wish to connect to; number of chars sent to server succesfully; chars read from server
	struct sockaddr_in serverAddress; //server address struct so the socket knows which server to connect to
	struct hostent *serverHostInfo; //used to connect to the host ie localhost in our programs scope


	//creation of socket
	socketFD = socket(AF_INET, SOCK_STREAM, 0);//create a socket that will communicate with the server.
	if(socketFD < 0)
	//if the socket cannot be made throw error
	{
		error("OTP_DEC: ERROR opening socket");
	}
	

	//creation of address stuct to server
	memset((char*)&serverAddress, '\0', sizeof(serverAddress)); //clear out server address struct so no stray characters will effect program
	portNumber = atoi(argv[3]); // get the port number of the server
	serverAddress.sin_family = AF_INET; //make socket general purpose
	serverAddress.sin_port = htons(portNumber); // convert portNumber to LSB 
	serverHostInfo = gethostbyname("localhost"); // get host info
	if(serverHostInfo == NULL)
	//if host not found print message
	{
		fprintf(stderr, "OTP_DEC: No such host!\n");
		exit(1);
	}
	memcpy((char*)&serverAddress.sin_addr.s_addr, (char*)serverHostInfo->h_addr, serverHostInfo->h_length);//add host address to address struct.

	//connect to the server
	if(connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
	{
		fprintf(stderr, "OTP_DEC: ERROR: Cannot connect to server.\n");
		exit(1);
	}
	
	//make sure this is the correct server
	char *program = (char *) malloc(sizeof(char)*20);
	char readBuffer[10];
	strcpy(program, "otp_dec@@\0");
	
	sendall(socketFD, program, 10);
	
//	free(program);
//	program = NULL;

	getall(socketFD, readBuffer, program);

	if(strcmp(program, "correct") == 0)
	//if it is the right seerver proceed with encryption and let server know to continue
	{
//		printf("OTP_DEC: Connected to correct server\n");
	}

	else
	//if it is not the right server, let the server know then exit
	{
		fprintf(stderr, "OTP_DEC: ERROR: Connected to incorrect server!\nExiting...\n");
		exit(1);
	}

	//get the message to send
	char *message;
	int messageLength = lengthOfFile(argv[1]); // get length of message
	message = (char *) malloc(messageLength*sizeof(char)); // allocate a string of that size
	getMessage(message, argv[1]);

	//get the keyfile contents
	char *key;
	int keyLength = lengthOfFile(argv[2]);
	key = (char *) malloc(keyLength*sizeof(char));
	getMessage(key, argv[2]);

	//send the file to be encrypted to server
	sendall(socketFD, message, messageLength);//function ensures that the entire message is sent.

	memset(message, '\0', sizeof(char)*messageLength);

	charsRead = recv(socketFD, message, messageLength, 0);//wait for the server to finish receiving the first message.

	//send the key to the server
	sendall(socketFD, key, keyLength);

	//see if the key is too short or if the input is bad
	memset(message, '\0', sizeof(char) * messageLength);
	getall(socketFD, readBuffer, message);

	sendall(socketFD, "bs@@\0", 5); 

	if(strcmp(message, "no") == 0)
	{
		fprintf(stderr, "OTP_ENC: ERROR: The input was bad\n");
	}
	
	else
	{
		//get encrypted message back from server
		memset(message, '\0', messageLength);
		getall(socketFD, readBuffer, message);

		printf("%s\n", message);

	}
	close(socketFD);//close the client
	

	return 0;
}
