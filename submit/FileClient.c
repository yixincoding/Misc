/*--------------------------------------------
 * FileClient.c
 *****************************************
 * It implements the command-line 
 * driven client for problem 3
 *****************************************
 * Usage:
 * ./FC <FileServerAddress> <port>
 *****************************************
 * ------------------------------------------*/
#include "cse356header.h"

char* serverAddr;
int serverPort = -1;

main(int argc,char* argv[])
{
	if (argc<3)
	{
		printf("FC <FileServerAddress> <FSPort>\n");
		exit(1);
	}
	serverAddr = argv[1];
	serverPort = atoi(argv[2]);
	if (serverPort<=0)
	{
		printf("Bad parameter: Illegal server port number.\n");
		exit(1);
	}

	int sockfd;
	int len;
	struct sockaddr_in serv_addr;
	struct hostent *host;


	serv_addr.sin_family = AF_INET;
	host = gethostbyname(serverAddr);
	memcpy(&serv_addr.sin_addr.s_addr,host->h_addr,host->h_length);
	serv_addr.sin_port = htons(serverPort);

	char line[1024];
	char res[1024];

	while (1)
	{
		if ((sockfd=socket(AF_INET,SOCK_STREAM,0))<0)
		{
			perror(NULL);
			exit(2);
		}
		if (connect(sockfd,(struct sockaddr*) &serv_addr,sizeof(serv_addr))<0)
		{
			perror(NULL);
			exit(3);
		}

		printf("Client >");
		gets(line);

		//Exit command terminates only client
		if (strcmp(line,"exit")==0)
			exit(0);

		write(sockfd,line,strlen(line)+1);

		//Terminate command terminates both server and client
		if (strcmp(line,"terminate")==0)
			exit(0);

		len = read(sockfd,res,1024);
		if (res[len-1]!='\0')
			res[len++] = '\0';

		if (len>1) 
			printf("%s\n",res);

	}

	close(sockfd);
}
