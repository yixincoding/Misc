/*--------------------------------------------
 * IntelliDisk.c
 *****************************************
 * It implements the intelligent disk server
 * for problem 2
 *****************************************
 * Usage:
 * IDS <DiskFileName> <#cylinder/track> 
 * <#sector> <track-to-track delay> <port> 
 * <RequestQueueLength> <SchedulingAlgorithm>
 *****************************************
 * More discussions are available in the
 * project report
 * ------------------------------------------*/
#include "cse356header.h"

extern const int blockSize; //Defined in cse356header.h

char* diskFile;
int cylinderNum = -1;
int sectorNum = -1;
int seekTime = -1;
int DISK_SERVER_PORT = -1;

int num;
int algorithm;

//Keep track of the current cylinder # to calculate seek time
int currentTrack = 0;

char* fileMap;

int sockfd,client_sockfd;

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

				if (c!=currentTrack)
				{
					//Simulate seek time
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

//First come first serve
void FCFS(int num,char buffer[][1024])
{
	int i;
	char res[1024];

	for (i=0;i<num;i++)
	{
		int len = process_comm(buffer[i],res);
		
		if (len && res[len-1]!='\0')
			res[len++]='\0';

//		write(client_sockfd,res,len);
	}
}

//Shortest seek time first
void SSTF(int num,char buffer[][1024])
{
	int i,j;
	char res[1024];
	int cylinder[1024];
	bool v[1024];
	
	memset(v,0,sizeof v);
	for (i=0;i<num;i++)
		sscanf(buffer[i],"%*c %d",cylinder+i);

	for (i=0;i<num;i++)
	{
		int min = cylinderNum+1;
		int mid = -1;

		for (j=0;j<num;j++)
			if (!v[j] && abs(cylinder[j]-currentTrack)<min)
			{
				min = abs(cylinder[j]-currentTrack);
				mid = j;
			}

		if (mid>=0)
		{
			v[mid]=true;
			int len = process_comm(buffer[mid],res);

			if (len && res[len-1]!='\0')
				res[len++]='\0';
//			write(client_sockfd,res,len);
		}
	}
}

struct track_t {
	int track;
	int id;
};
int comp(const void* x,const void* y)
{
	return ((track_t*)x)->track - ((track_t*)y)->track;
}
//Circular-Look
void CLOOK(int num,char buffer[][1024])
{
	int i;
	char res[1024];
	struct track_t cylinder[1024];
	bool v[1024];
	int start;
	
	memset(v,0,sizeof v);
	for (i=0;i<num;i++)
	{
		cylinder[i].id = i;
		sscanf(buffer[i],"%*c %d",&(cylinder[i].track));
	}
	//Sort the requests
	qsort(cylinder,num,sizeof(track_t),comp);

	//Find the first request whose track # is no less than currentTrack
	for (start=0;start<num;start++)
		if (cylinder[start].track >= currentTrack)
			break;

	for (i=start;i<num;i++)
	{
		int len = process_comm(buffer[cylinder[i].id],res);

		if (len && res[len-1]!='\0')
			res[len++]='\0';
//		write(client_sockfd,res,len);
	}

	for (i=0;i<start;i++)
	{
		int len = process_comm(buffer[cylinder[i].id],res);

		if (len && res[len-1]!='\0')
			res[len++]='\0';
//		write(client_sockfd,res,len);
	}
}

main(int argc,char* argv[])
{
	if (argc<8)
	{
		printf("Usage: IDS <DiskFileName> <#cylinder/track> <#sector> <track-to-track delay> <port> <RequestQueueLength> <SchedulingAlgorithm>\n");
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
	num = atoi(argv[6]);
	if (num<=0)
	{
		printf("Error: Bad parameter! Illegal length of request queue.\n");
		exit(-1);
	}
	algorithm = atoi(argv[7]);
	if (algorithm<0 || algorithm>2)
	{
		printf("Error: Bad parameter! Illegal scheduling algorithm( 0:FCFS, 1:SSTF, 2:C-LOOK).\n");
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

	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigaction( SIGPIPE, &sa, 0 );

	//Set up the server
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

	int count = 0;
	char buffer[1024][1024];
	memset(buffer,0,sizeof buffer);
	//function pointer array for different scheduling algorithms
	void (*schedule[3])(int num,char buffer[][1024]);	
	schedule[0] = FCFS;
	schedule[1] = SSTF;
	schedule[2] = CLOOK;
	
	puts("Server running.");

	while (1)
	{
		len = sizeof(client_addr);
		client_sockfd = accept(sockfd,(sockaddr*)&client_addr,&len);
		if (client_sockfd == -1)
		{
			perror(NULL);
			continue;
		}

		//Read command from client
		read(client_sockfd,buffer[count++],1024);

		//Echo command received
		printf("Command received: %s\n",buffer[count-1]);

		//terminate command will termiate both server and client
		if (strcmp(buffer[count-1],"terminate")==0)
		{
			count--;
			break;
		}

		//"I" command is executed instantly
		if (strcmp(buffer[count-1],"I")==0 || strcmp(buffer[count-1],"i")==0)
		{
			count--;
			char res[1024];
			int length = process_comm(buffer[count],res);
			write(client_sockfd,res,length);
		}

		//If the queue is full, process the requests
		if (count==num)
		{
			//Algorithm selection
			schedule[algorithm](count,buffer);

			//Clear count
			count=0;
			memset(buffer,0,sizeof buffer);
		}

		close(client_sockfd);
	}

	//Process remaining requests
	if (count>0)
	{
		schedule[algorithm](count,buffer);
	}

	if (munmap(fileMap, diskFileSize) == -1) {
		perror("Error un-mmapping the file");
	}
	close(fd);
}
