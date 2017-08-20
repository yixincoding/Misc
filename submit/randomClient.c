/*--------------------------------------------
 * randomClient.c
 *****************************************
 * It implements the random data client
 * for problem 1 and 2
 *****************************************
 * Usage:
 * ./randomBDC <DiskServerAddress> <Port> 
 * <RequestNumber> <RandomSeed>
 *****************************************
 * ------------------------------------------*/
#include "cse356header.h"

extern const int blockSize; //Defined in cse356header.h

char* serverAddr;
int serverPort = -1;
int num;
int cNum,sNum;
int seed;

main(int argc,char* argv[])
{
	if (argc<5)
	{
		printf("randomBDC <DiskServerAddress> <Port> <RequestNumber> <RandomSeed>\n");
		exit(1);
	}
	serverAddr = argv[1];
	serverPort = atoi(argv[2]);
	if (serverPort<=0)
	{
		printf("Bad parameter: Illegal server port number.\n");
		exit(1);
	}
	num = atoi(argv[3]);
	if (num<=0)
	{
		printf("Bad parameter: Illegal request number.\n");
		exit(1);
	}
	seed = atoi(argv[4]);
	if (seed<=0)
	{
		printf("Bad parameter: Illegal random seed.\n");
		exit(1);
	}

	srand(seed);

	int sockfd;
	int len;
	struct sockaddr_in serv_addr;
	struct hostent *host;


	serv_addr.sin_family = AF_INET;
	host = gethostbyname(serverAddr);
	memcpy(&serv_addr.sin_addr.s_addr,host->h_addr,host->h_length);
	serv_addr.sin_port = htons(serverPort);

	char res[1024];
	char line[1024];
	char tmp[1024];

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
	//Query the disk info
	write(sockfd,"I",2);
	len = read(sockfd,res,1024);

	if (sscanf(res,"%d %d",&cNum,&sNum)<2)
	{
		printf("Bad cylinder & section numbers!\n");
		exit(1);
	}
	close(sockfd);

	for (int i=0;i<=num;i++)
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

		//Generate random R or W commands
		memset(line,0,sizeof line);
		line[0] = rand()%2 ? 'R' : 'W';
		line[1] = ' ';
		sprintf(tmp,"%d %d",rand()%cNum,rand()%sNum);
		strcat(line,tmp);
		if (line[0]=='W')
		{
			memcpy(tmp," 256 ",sizeof(" 256 "));
			for (int j=5;j<5+blockSize;j++)
				tmp[j] = rand()%95 + 32;
			tmp[5+blockSize]='\0';
			strcat(line,tmp);
		}

		write(sockfd,line,strlen(line)+1);

		len = read(sockfd,res,1024);

		printf("%c",line[0]);
	
		close(sockfd);
	}

	printf("\n");

}
