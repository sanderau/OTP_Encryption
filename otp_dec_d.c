#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


void error(const char *msg)
//if an error occurs print error message and exit
{
	perror(msg);
	exit(1);
}

void doubleString(char **str, int *l)
//double the size of the passed string while maintaining its original values
{
	char *tmp = (char *) malloc(sizeof(char)*(*l)); // allocate a temp string the size of str
	strcpy(tmp, *str); //copy contents of str into a temp string

	free( *str ); // free string

	*l = *l * 2;//double the size of the size of the message
	*str = (char *) malloc(sizeof(char)*(*l));//double the actual memory allocated for the string
	
//	memset(*str, '\0', sizeof(char)*(*l)); // set all the values to null term

	strcpy(*str, tmp); //copy the contents back into str

	free ( tmp ); // free temp.
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


void nullTerm(char *message)
//replace the \n with \0
{
	char *pos;
	if((pos=strchr(message, '\n')) != NULL)
	{
		*pos = '\0';
	}
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

void doubleStringTriple(char ***str, int *l)
//double the size of the passed string while maintaining its original values
//need a special function for getallLarge because I am passing it twice so I need special pointer work
{
	char *tmp = (char *) malloc(sizeof(char)*(*l)); // allocate a temp string the size of str
	strcpy(tmp, **str); //copy contents of str into a temp string

	free( **str ); // free string

	*l = *l * 2;//double the size of the size of the message
	**str = (char *) malloc(sizeof(char)*(*l));//double the actual memory allocated for the string
	
//	memset(*str, '\0', sizeof(char)*(*l)); // set all the values to null term

	strcpy(**str, tmp); //copy the contents back into str

	free ( tmp ); // free temp.
}

int getallLarge(int socketFD, char *buff, char **message, int *sizeOfMessage)
//this will get the entire message from the client. When we do not know the size of the message I pass the amount allocated to the string so 
//everytime the total size becomes greater than the amount allocated for the string I can double the size so I dont do anything bad with memory
{
	int r, total = 0;
	memset(*message, '\0', sizeof(*message));

	while (strstr(*message, "@@") == NULL) // keep executing until we find null terminator for transmission
	{
		memset(buff, '\0', sizeof(buff)); //clear the buffer
		r = recv(socketFD, buff, sizeof(buff)-1, 0); //receive a chunk of the data
		total += r;

		if(total > *sizeOfMessage)
		{
//			printf("going to double string: %d\n", *sizeOfMessage);
			doubleStringTriple(&message, sizeOfMessage);
//			printf("Done doing that: %d\n", *sizeOfMessage);
		}

		strcat(*message, buff); // add newly recieved chunk
		if(r == -1) {printf("r == -1\n"); break;}
		if(r == 0) {printf("r == 0\n"); break;}
	}

	//int term = strstr(*message, "@@") - *message;
	//*message[term] = '\0';

	return total-2; //minus 2 to account for code
}

int checkInput(char *str, int strLength)
{
	int i;

	for(i = 0; i < strLength; i++)
	{
		char temp = str[i];
		char num = (int)temp;

		if(num > 90)
		{
			return 1;
		}

		if(num < 65 && num != 32 && num != 0)
		{
			return 1;
		}
	}

	return 0; 
}

char getChar(int c)
{
	if(c == 0)
	{
		return (char)32;
	}

	else
	{
		return (char)(c + 64);
	}
}

int cypherVal(char c)
//converts char into a num from 0 to 26
{
	if((int)c == 32)
	{
		return 0;
	}

	else
	{
		return ((int)c) - 64;
	}

}


void OTPdecrypt(char *str, char *key, char *enc, int l)
{
//	printf("OTP_ENC_D: Here is the string I have in the decryption loop: %s\n", str);
	int i = 0;

	for(i = 0; i < l; i++)
	{
		int sTemp = cypherVal(str[i]);
		int kTemp = cypherVal(key[i]);

		int cypher = (sTemp - kTemp) % 27;
		if(cypher < 0)
		{
			cypher += 27;
		}

//		printf("%c", getChar(cypher));
		enc[i] = getChar(cypher);
	}

	enc[i++] = '@';
	enc[i++] = '@';
	enc[i++] = '\0';

}


int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		fprintf(stderr, "%s [PORT]\n", argv[0]);
		exit(1);
	}

	int listenSocketFD, port, establishedConnectionFD;
	struct sockaddr_in SA, CA; //create a socket address for the server
	socklen_t sizeOfClientInfo;

	//set up the socket
	listenSocketFD = socket(AF_INET, SOCK_STREAM, 0);
	if(listenSocketFD < 0)
	{
		error("OTP_DEC_D: ERROR: cannot open socket");
	}

	//set up socket address
	memset((char *)&SA, '\0', sizeof(SA)); // clear out the struct
	SA.sin_family = AF_INET; //create network capable socket
	port = atoi(argv[1]);
	SA.sin_port = htons(port); // store the port number
	SA.sin_addr.s_addr = INADDR_ANY; //Any address is allowed to connect

	//Enable the socket to start listening
	if(bind(listenSocketFD, (struct sockaddr *)&SA, sizeof(SA)) < 0)
	{
		error("OTP_DEC_D: ERROR: could not bind!");
	}
	listen(listenSocketFD, 5);//accept up to five clients
	
	pid_t pids[5];
	int f = fork();
	pids[0] = f;

	if(f != 0)
	{
		printf("Child pid #1: %d\n", f);
		pids[1] = f;
		f = fork();
		if(f != 0)
		{
			printf("#2: %d\n", f);
			pids[2] = f;
			f = fork();
			if(f != 0)
			{
				printf("#3: %d\n", f);
				pids[3] = f;
				f = fork();
				if(f != 0)
				{
					printf("#4: %d\n", f);
					pids[4] = f;
					f = fork();
					if(f != 0)
					{
						printf("#5: %d\n", f);
					}
				}
			}
		}
	}

	if(f != 0)
	//if parent
	{
		while(1); // let children do all the work
	}

	else
	//if the child, take connections and do work
	{
		
		while(1)
		{
			//accept a connection
			sizeOfClientInfo = sizeof(CA);
			establishedConnectionFD = accept(listenSocketFD, (struct sockaddr *)&CA, &sizeOfClientInfo);
			if(establishedConnectionFD < 0)
			{
				error("OTP_DEC_D: ERROR: could not accept");
			}

			/*********************************************MAKE SURE THE RIGHT PROCESS IS CONNECTED******************************************/
			char readBuffer[10];
			char *program = (char *) malloc(sizeof(char)*20);
			int correct; // bool value used to let the server know whether or not to procceed with encryption

			getall(establishedConnectionFD, readBuffer, program);

		//	printf("%s\n", program);

			if(strcmp(program, "otp_dec") == 0)
			//if this is the right program, let the
			{
				correct = 1;
		//		printf("OTP_DEC_D: Hey that is the right function\n");
				strcpy(program, "correct@@\0");
				sendall(establishedConnectionFD, program, 10);
			}

			else 
			{
				correct = 0;
		//		printf("OTP_DEC_D: That is the incorrect function\n");
				strcpy(program, "incorrect@@\0");
				sendall(establishedConnectionFD, program, 12);
				close(establishedConnectionFD);
			}


			if(correct)
			{

				/********************************************GET THE MESSAGE*********************************************************************/
				int sizeOfMessage = 512, messageLength = 0;
				int *ptrmLength = &sizeOfMessage;
				char *completeMessage = (char *) malloc(sizeof(char)*sizeOfMessage);

				messageLength = getallLarge(establishedConnectionFD, readBuffer, &completeMessage, ptrmLength); // get the entire message

				int term = strstr(completeMessage, "@@") - completeMessage;
				completeMessage[term] = '\0';

		//		printf("OTP_DEC_D: I recieved this from the client \"%s\"\nSIZE: %d\n", completeMessage, messageLength);
		//		printf("OTP_DEC_D: Size of message now: %d\n", sizeOfMessage);
			
				send(establishedConnectionFD, "I got your msg", 17, 0);// allows the client to send the key now

				/*************************************************************GET THE KEY******************************************************/
				int sizeOfKey = 512, keyLength = 0;
				char *key = (char *) malloc(sizeof(char)*sizeOfKey);

				memset(key, '\0', sizeof(key));

				int *ptrKeyLength = &sizeOfKey;
				keyLength = getallLarge(establishedConnectionFD, readBuffer, &key, ptrKeyLength); // get the entire key

				term = strstr(key, "@@") - key;
				key[term] = '\0';

				int badInput = checkInput(completeMessage, messageLength);

		//		printf("OTP_DEC_D: I recieved this from the client \"%s\"\nSIZE: %d\n", key ,keyLength);
		//		printf("OTP_END_D: Size of key now: %d\n", sizeOfKey);
				

				if(keyLength < messageLength || (badInput))
				//if the key is too short, send final message to let them know it is too short, then close connection
				{
					if(badInput)
					{
						fprintf(stderr, "OTP_DEC_D: ERROR: The input was bad");
						sendall(establishedConnectionFD, "no@@\0", 5);
						close(establishedConnectionFD);
					}
			
					else
					{
						fprintf(stderr, "OTP_DEC_D: ERROR: Key is too short!\n");
						sendall(establishedConnectionFD, "no@@\0", 5);
						close(establishedConnectionFD);
					}
				}

				else
				{
					sendall(establishedConnectionFD, "yes@@\n", 5);
					/************Check for bad characters here first****************************/

					/****************************************************************************/
					/*******************Encrypt or Decrypt here*********************************/
					/***************************************************************************/

					char *temp = (char *) malloc(sizeof(char) * 10);
					getall(establishedConnectionFD, readBuffer, temp);

					char *decrypt = (char *) malloc(sizeof(char) * messageLength);
					memset(decrypt, '\0', sizeof(decrypt));

					OTPdecrypt(completeMessage, key, decrypt, messageLength-1);

		//			printf("OTP_DEC_D: Here is the decoded message: %s\n", decrypt);

					sendall(establishedConnectionFD, decrypt, messageLength+3);

					//send encypted message back to the client
					send(establishedConnectionFD, "I got your msg", 17, 0);
				}
			}

		}
	}
	
	close(listenSocketFD);
	
	
	return 0;
}
