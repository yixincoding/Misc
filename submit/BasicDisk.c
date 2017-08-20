/*--------------------------------------------
 * BasicDisk.c
 *****************************************
 * It implements the basic disk server
 * for problem 1
 *****************************************
 * Usage:
 * BDS <DiskFileName> <#cylinder/track> 
 * <#sector> <track-to-track delay> <port>
 *****************************************
 * ------------------------------------------*/
#include "cse356header.h"

extern const int blockSize;
char* diskFile;
int cylinderNum = -1;
int sectorNum = -1;
int seekTime = -1;
int DISK_SERVER_PORT = -1;

//Keep track of the current cylinder # to calculate seek time
int currentTrack = 0;

char* fileMap;

//Process the command line, put the return result in res
//and return the length of result
int process_comm(char* line,char* res)
{
	memset(res,0,sizeof 0);
	int c,s,l;

	switch(line[0])
	{
		case 'i':
		case 'I':
			{
				sprintf(res,"%d %d",cylinderNum,sectorNum);

				return strlen(res)+1;
			}

		case 'r':
		case 'R':
			{
				if (sscanf(line+1,"%d %d",&c,&s)<2)
					return 0;

				if (c<0 || c>=cylinderNum || s<0 || s>=sectorNum)
				{
					res[0]='0';
					return 1;
				}

				if (c!=currentTrack)
				{
					//Simulate seek time
					usleep(seekTime*abs(c-currentTrack));
					currentTrack = c;
				}

				res[0]='1';
				//Read data
				memcpy(res+1,fileMap+blockSize*(c*sectorNum+s),blockSize);

				return 1+blockSize;
			}

		case 'w':
		case 'W':
			{
				if (sscanf(line+1,"%d %d %d",&c,&s,&l)<3)
					return 0;

				if (c<0 || c>=cylinderNum || s<0 || s>=sectorNum || l<0 || l>blockSize)
				{
					res[0]='0';
					return 1;
				}

				char* tmp = line;
				for (int i=0;i<4;i++)
					tmp = strchr(tmp+1,' ');
				tmp++;
				for (int i=l;i<blockSize;i++)
					tmp[i]='\0';

				//Simulate seek time
				if (c!=currentTrack)
				{
					usleep(seekTime*abs(c-currentTrack));
					currentTrack = c;
				}

				//Write data
				memcpy(fileMap+blockSize*(c*sectorNum+s),tmp,blockSize);

				res[0]='1';
				return 1;
			}
		default:
			puts("Unknown Command");
	}
	return 0;
}

main(int argc,char* argv[])
{
	if (argc<6)
	{
		printf("Usage: BDS <DiskFileName> <#cylinder/track> <#sector> <track-to-track delay> <port>\n");
		exit(-1);
	}
	diskFile = argv[1];
	cylinderNum = atoi(argv[2]);
	if (cylinderNum<=0)
	{
		printf("Error: Bad parameter! Illegal number of cylinders.\n");
		exit(-1);
	}
	sectorNum = atoi(argv[3]);
	if (sectorNum<=0)
	{
		printf("Error: Bad parameter! Illegal number of sectors.\n");
		exit(-1);
	}
	seekTime = atoi(argv[4]);
	if (seekTime<=0)
	{
		printf("Error: Bad parameter! Illegal seek time between ajacent tracks.\n");
		exit(-1);
	}
	DISK_SERVER_PORT = atoi(argv[5]);
	if (DISK_SERVER_PORT<=0)
	{
		printf("Error: Bad parameter! Illegal port number.\n");
		exit(-1);
	}

	int diskFileSize = blockSize*cylinderNum*sectorNum;

	//Open file and map to memory
	int fd = open(diskFile, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
	if (fd == -1) 
	{
		perror("Error opening file for writing");
		exit(1);
	}
	if (lseek(fd, diskFileSize-1, SEEK_SET) == -1) 
	{
		close(fd);
		perror("Error calling lseek() to 'stretch' the file");
		exit(1);
	}
	if (write(fd, "", 1) != 1) 
	{
		close(fd);
		perror("Error writing last byte of the file");
		exit(1);
	}

	fileMap = (char*) mmap(0, diskFileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (fileMap == MAP_FAILED) {
		close(fd);
		perror("Error mmapping the file");
		exit(1);
	}


	//Set up the server
	int sockfd,client_sockfd;
	socklen_t len;
	struct sockaddr_in serv_addr,client_addr;

	if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
	{
		perror(NULL);
		exit(2);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(DISK_SERVER_PORT);

	if (bind(sockfd,(sockaddr*)&serv_addr,sizeof(serv_addr))<0)
	{
		perror(NULL);
		exit(2);
	}

	listen(sockfd,5);

	puts("Server running.");
	char buffer[1024];
	char res[1024];
	while (1)
	{
		len = sizeof(client_addr);
		client_sockfd = accept(sockfd,(sockaddr*)&client_addr,&len);
		if (client_sockfd == -1)
		{
			perror(NULL);
			continue;
		}

		memset(buffer,0,sizeof buffer);

		//Read command from client
		read(client_sockfd,buffer,1024);

		//Echo command received
		printf("Command received: %s\n",buffer);

		//terminate command will termiate both server and client
		if (strcmp(buffer,"terminate")==0)
			break;

		//Process the command
		int length = process_comm(buffer,res);

		//Return the result the client
		write(client_sockfd,res,length);

		close(client_sockfd);
	}

	if (munmap(fileMap, diskFileSize) == -1) {
		perror("Error un-mmapping the file");
	}
	close(fd);
}
