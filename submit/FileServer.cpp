/*--------------------------------------------
 * FileServer.cpp
 *****************************************
 * It implements the file server for
 * problem 3
 *****************************************
 * Usage:
 * ./FS <DiskServerAddress> <DSPort> 
 * <FSport>
 *****************************************
 * Implementation ideas are decribed in
 * the project report.
 * ------------------------------------------*/
#include "cse356header.h"
#include <iostream>
#include <cmath>
#include <string>
#include <set>
#include <map>
using namespace std;

#define MAX_BUFFER_SIZE 65536

int BDSPort = -1;
int FSPort = -1;
char* serverAddr;

int FSsockfd,sockfd,client_sockfd;
socklen_t len;
struct hostent *host;
struct sockaddr_in FSserv_addr,serv_addr,client_addr;

//cylinderNum and sectorNum
int cNum,sNum;

extern const int blockSize; //Defined in cse356header.h

/*****************************************
 * Core data structures
 * Maintains the file system in memory
 * to speed up performance
 *
 * It is highly recommended that you read
 * the basic idea of implemenatations 
 * explicated in the project report
 * first before inspecting these codes
 *****************************************/
set<int> freeBlocks; //Free blocks list
set<int> dirBlocks; //Directory info blocks list
char* typeTable; //Block type info
int* fatTable; //FAT Table

int typeBlock; //# of type blocks
int fatBlock; //# of FAT blocks

class File //It is designed to match the disk block in size
{
public:
	File(int s,int sB,int dB):size(s),startBlock(sB),dirBlock(dB)
	{}
	File()
	{
		memset(name,0,sizeof name);
		size = 0;
		startBlock = dirBlock = -1;
	}
	File(const File &f)
	{
		strcpy(name,f.name);
		size=f.size;startBlock=f.startBlock;dirBlock=f.dirBlock;
	}

	char name[244];
	int size;
	int startBlock;
	int dirBlock;
};
map<string,File> files; //File lists

//Connect to the disk server, and return the sockfd
int connectDS()
{
	int sockfd;
	serv_addr.sin_family = AF_INET;
	host = gethostbyname(serverAddr);
	memcpy(&serv_addr.sin_addr.s_addr,host->h_addr,host->h_length);
	serv_addr.sin_port = htons(BDSPort);

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
	return sockfd;
}

//Read a block from the disk and put the result in res
void readDisk(char* res,int c,int s)
{
	int sockfd = connectDS();
	char line[1024];

	sprintf(line,"R %d %d",c,s);
	write(sockfd,line,strlen(line)+1);

	memset(line,0,sizeof line);
	memset(res,0,sizeof res);
	read(sockfd,line,1024);
	memcpy(res,line+1,blockSize);
	res[blockSize]='\0';

	close(sockfd);
}

//Write len bytes of text into the given block of disk
void writeDisk(char* text,int len,int c,int s)
{
	int sockfd = connectDS();
	char line[1024];

	sprintf(line,"W %d %d %d ",c,s,min(len,blockSize));
	int length = strlen(line);
	memcpy(line+length,text,min(len,blockSize));
	//len+=strlen(line);
	len+=length;
	line[len+1] = '\0';
	write(sockfd,line,len+1);

	close(sockfd);
}

//Write the in-memory typeTable to disk
void writeTypeTable()
{
	for (int i=0;i<typeBlock;i++)
		writeDisk(typeTable+i*blockSize,blockSize,i/sNum,i%sNum);
}

//Write the in-memory FAT to disk
void writeFATTable()
{
	for (int i=typeBlock;i<typeBlock+fatBlock;i++)
		writeDisk((char*)fatTable+(i-typeBlock)*blockSize,blockSize,i/sNum,i%sNum);
}

//Process the command line, put the return result in res
//and return the length of result
int process_comm(const char* line,char* res)
{
	int c,s,l;
	int i,j;
	char tmp[1024];
	memset(res,0,sizeof res);
	memset(tmp,0,sizeof tmp);

	switch(line[0])
	{
		case 'f':
		case 'F':
			{
				//Write block type info
				for (i=0;i<typeBlock;i++)
				{
					memset(tmp,0,sizeof tmp);
					for (j=0;j<blockSize;j++)
					{
						if (i*blockSize+j<typeBlock)
							tmp[j]='t'; //type block
						else if (i*blockSize+j<typeBlock+fatBlock)
							tmp[j]='f'; //FAT block
						else if (i*blockSize+j<cNum*sNum)
							tmp[j]='v'; //free block
						else
							tmp[j]='i'; //invalid block
					}

					//Write in-momory type info
					memcpy(typeTable+i*blockSize,tmp,blockSize);

					//Write on-disk type info
					writeDisk(tmp,blockSize,i/sNum,i%sNum);
				}

				//Write FAT
				//Fill in-memory FAT with -1 entries
				memset(fatTable,255,sizeof(int)*(cNum*sNum+blockSize));

				//Fill on-disk FAT with -1 entries
				memset(tmp,255,sizeof tmp);//Set all entries to be -1
				for (i=typeBlock;i<typeBlock+fatBlock;i++)
				{
					writeDisk(tmp,blockSize,i/sNum,i%sNum);
				}

				//Maintain in-memory info
				dirBlocks.clear();
				files.clear();

				freeBlocks.clear();
				for (i=typeBlock+fatBlock;i<cNum*sNum;i++)
					freeBlocks.insert(i);

				return 0;
			}
		case 'c':
		case 'C':
			{
				//Space check
				if (freeBlocks.size()<2)
				{
					res[0]='2';
					printf("No enough space!\n");
					return 2;
				}
				string filename(line+2);
				if (filename.length() > 200)
				{
					res[0]='2';
					printf("Illegal file name!\n");
					return 2;
				}
				
				if (files.count(string(filename)))
				{
					res[0]='1';
					printf("File already exist!\n");
					return 2;
				}

				//find a block for directory info
				int dir = *freeBlocks.begin();
				freeBlocks.erase(dir);
				typeTable[dir]='d';
				
				//Create the directory info
				File f;
				strcpy(f.name,filename.c_str());
				f.size = 0;
				f.startBlock = *freeBlocks.begin();
				freeBlocks.erase(f.startBlock);
				typeTable[f.startBlock]='n';
				f.dirBlock = dir;
				//Write directory info into directory block
				writeDisk((char*)&f,sizeof(f),dir/sNum,dir%sNum);

				//Update in-memory dir info
				files[filename] = f;

				//Sync block type info between memory and disk
				writeTypeTable();

				res[0]='0';
				return 2;
			}
		case 'd':
		case 'D':
			{
				string filename(line+2);
				//File not exist
				if (!files.count(filename))
				{
					res[0]='1';
					printf("File not found!\n");
					return 2;
				}

				File f = files[filename];
				//Delete file data
				for (i=f.startBlock;i!=-1;i=fatTable[i])
				{
					freeBlocks.insert(i);
					typeTable[i]='v';
				}

				files.erase(filename);
				//Delete directory info
				dirBlocks.erase(f.dirBlock);
				freeBlocks.insert(f.dirBlock);
				typeTable[f.dirBlock]='v';

				//Update FAT
				int tmp=f.startBlock;
				for (i=tmp;i!=-1;i=tmp)
				{
					tmp=fatTable[i];
					fatTable[i]=-1;
				}

				//Sync between memory and disk
				writeTypeTable();
				writeFATTable();

				res[0]='0';
				return 2;
			}
		case 'l':
		case 'L':
			{
				//List all the files
				for (map<string,File>::iterator i=files.begin();i!=files.end();i++)
				{
					strcat(res,i->first.c_str());
					if (line[2]=='1')
						sprintf(res,"%s %d",res,i->second.size);
					if ((++i)-- != files.end())
						strcat(res,"\n");
				}
				return strlen(res)+1;
			}
		case 'r':
		case 'R':
			{
				string filename(line+2);
				if (!files.count(filename))
				{
					res[0]='1';
					printf("File not found!\n");
					return 2;
				}

				File f = files[filename];
				
				//Read data
				int count = 0;
				for (i=f.startBlock;i!=-1;i=fatTable[i])
				{
					readDisk(tmp,i/sNum,i%sNum);
					memcpy(res+count,tmp,min(blockSize,f.size-count));
					count+=blockSize;
				}
				res[f.size]='\0';

				memset(tmp,0,sizeof tmp);
				memcpy(tmp,res,f.size);
				sprintf(res,"0 %d %s",f.size,tmp);

				return strlen(res)+1;
			}
		case 'w':
		case 'W':
			{
				int len;
				if (sscanf(line,"%*s %s %d",tmp,&len)<2)
				{
					res[0]='2';
					printf("Illegal command!\n");
					return 2;
				}
				string filename(tmp);

				char* data=(char*)line;
				for (i=0;i<3;i++)
					data = strchr(data+1,' ');
				data++;

				if (!files.count(filename))
				{
					res[0]='1';
					printf("File not found!\n");
					return 2;
				}
				File f = files[filename];

				//Space checking
				if (len/blockSize - f.size/blockSize > (int)freeBlocks.size())
				{
					res[0]='2';
					printf("No enough space!\n");
					return 2;
				}
				
				//Remove current data
				for (i=f.startBlock;i!=-1;i=fatTable[i])
				{
					freeBlocks.insert(i);
					typeTable[i]='v';
				}
				//Clear FAT
				int temp=f.startBlock;
				for (i=temp;i!=-1;i=temp)
				{
					temp=fatTable[i];
					fatTable[i]=-1;
				}

				//Write new data
				f.size = len;

				int prev_blk = -1;
				int blk;
				//Write file data
				for (i=0;i<len || !len;i+=blockSize)
				{
					//Get a free block
					blk = *freeBlocks.begin();
					freeBlocks.erase(blk);
					typeTable[blk]='n';

					if (i==0)
						f.startBlock = blk;

					if (prev_blk >= 0)
						fatTable[prev_blk] = blk;//Update FAT
					prev_blk = blk;

					writeDisk(data+i,min(blockSize,len-i),blk/sNum,blk%sNum);

					//Still allocate one block for an empty file
					if (!len) break;
				}

				//Update directory info
				files[filename] = f;
				writeDisk((char*)&f,blockSize,f.dirBlock/sNum,f.dirBlock%sNum);

				//Sync between disk and memory
				writeTypeTable();
				writeFATTable();

				res[0] = '0';
				return 2;
			}
		case 'a':
		case 'A':
			{
				int len;
				if (sscanf(line,"%*s %s %d",tmp,&len)<2)
				{
					res[0]='2';
					printf("Illegal command!\n");
					return 2;
				}
				string filename(tmp);

				char* data=(char*)line;
				for (i=0;i<3;i++)
					data = strchr(data+1,' ');
				data++;

				if (!files.count(filename))
				{
					res[0]='1';
					printf("File not found!\n");
					return 2;
				}
				File f = files[filename];

				if (len-(blockSize-f.size%blockSize) > (int)freeBlocks.size()*blockSize)
				{
					res[0]='2';
					printf("No enough space!\n");
					return 2;
				}

				int endBlock;
				for (endBlock=f.startBlock;fatTable[endBlock]!=-1;endBlock=fatTable[endBlock]) ;

				//Check whether the last data block of file can hold the new data
				if (len <= blockSize - f.size%blockSize)
				{ //If so
					//Read the last data block
					readDisk(tmp,endBlock/sNum,endBlock%sNum);
					memcpy(tmp+f.size%blockSize,data,len);
					writeDisk(tmp,f.size%blockSize+len,endBlock/sNum,endBlock%sNum);

					//Update directory info
					f.size+=len;
					files[filename] = f;
					writeDisk((char*)&f,blockSize,f.dirBlock/sNum,f.dirBlock%sNum);
					
					res[0]='0';
					return 2;
				}

				//Otherwise, new blocks need to be allocated
				
				//first fill the last block
				readDisk(tmp,endBlock/sNum,endBlock%sNum);
				memcpy(tmp+f.size%blockSize,data,blockSize-f.size%blockSize);
				writeDisk(tmp,blockSize,endBlock/sNum,endBlock%sNum);

				//Add new blocks
				int prev_blk = endBlock;
				int blk;
				for (i=blockSize-f.size%blockSize;i<len;i+=blockSize)
				{
					blk = *freeBlocks.begin();
					freeBlocks.erase(blk);
					typeTable[blk]='n';

					fatTable[prev_blk] = blk;
					prev_blk = blk;

					writeDisk(data+i,min(blockSize,len-i),blk/sNum,blk%sNum);
				}

				//Update dir info
				f.size+=len;
				files[filename] = f;
				writeDisk((char*)&f,blockSize,f.dirBlock/sNum,f.dirBlock%sNum);

				//Sync between disk and memory
				writeTypeTable();
				writeFATTable();

				res[0]='0';
				return 2;
			}
		default:
			puts("Unknown Command");
	}
	return 0;
}

bool loadFileSystem()
{
	int i,j;
	char res[1024];

	//Clear in-memory type info and FAT
	memset(fatTable,255,sizeof(int)*(cNum*sNum+blockSize));
	memset(typeTable,0,sizeof(char)*(cNum*sNum+blockSize));
	
	//Read block type info
	for (i=0;i<typeBlock;i++)
	{
		readDisk(res,i/sNum,i%sNum);

		for (j=0;j<blockSize;j++)
		{
			if (i*blockSize+j >= cNum*sNum)
				break;

			typeTable[i*blockSize+j] = res[j]; //Keep type info in memory
			switch (res[j])
			{
				case 't': //Block type
				case 'f': //File Allocation Table
				case 'n': //Normal file data
				case 'i': //Invalid
					break;
				case 'd': //Directory information
					{
						dirBlocks.insert(i*blockSize+j);
						break;
					}
				case 'v': //Free space
					{
						freeBlocks.insert(i*blockSize+j);
						break;
					}
				default:
					{
						return false;
					}
			}
		}
	}
		
	//Read Directory information
	for (set<int>::iterator i=dirBlocks.begin();i!=dirBlocks.end();i++)
	{
		readDisk(res,*i / sNum,*i % sNum);

		File f;
		memcpy(&f,res,sizeof f);
		files[string(f.name)]=f;
	}

	return true;
}

int main(int argc,char* argv[])
{
	if (argc<4)
	{
		printf("Usage: FS <DiskServerAddress> <BDSPort> <FSPort>\n");
		exit(1);
	}
	serverAddr = argv[1];
	BDSPort = atoi(argv[2]);
	if (BDSPort<=0)
	{
		printf("Bad parameter: Illegal disk server port number.\n");
		exit(1);
	}
	FSPort = atoi(argv[3]);
	if (FSPort<=0)
	{
		printf("Bad parameter: Illegal file server port number.\n");
		exit(1);
	}

	char res[MAX_BUFFER_SIZE];
	char buffer[MAX_BUFFER_SIZE];

	//Read Disk info
	sockfd = connectDS();
	write(sockfd,"I",2);
	len = read(sockfd,res,1024);

	if (sscanf(res,"%d %d",&cNum,&sNum)<2)
	{
		printf("Bad cylinder & section numbers!\n");
		exit(1);
	}
	close(sockfd);

	//Calculate # of type blocks and FAT blocks
	typeBlock = ceil((double)cNum*sNum/blockSize);
	fatBlock = ceil((double)cNum*sNum*4/blockSize);
	//Allocate space for in-memory type table and FAT
	fatTable = new int[cNum*sNum + blockSize];
	typeTable = new char[cNum*sNum + blockSize];

	//Try to load the file system info into memory
	if (!loadFileSystem())
	{
		printf("Unknown format! Will automatically format the file system.\n");
		process_comm("F",res);
		loadFileSystem();
	}
	
	//Build the server
	if ((FSsockfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
	{
		perror(NULL);
		exit(2);
	}

	FSserv_addr.sin_family = AF_INET;
	FSserv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	FSserv_addr.sin_port = htons(FSPort);

	if (bind(FSsockfd,(sockaddr*)&FSserv_addr,sizeof(FSserv_addr))<0)
	{
		perror(NULL);
		exit(2);
	}

	listen(FSsockfd,5);
	
	puts("File server running.");

	while (1)
	{
		len = sizeof(client_addr);
		client_sockfd = accept(FSsockfd,(sockaddr*)&client_addr,&len);
		if (client_sockfd == -1)
		{
			perror(NULL);
			continue;
		}

		memset(buffer,0,sizeof buffer);

		//Read command from client
		read(client_sockfd,buffer,MAX_BUFFER_SIZE);

		//Echo command received
		printf("Command received: %s\n",buffer);

		//terminate command will terminate both the server and the client
		if (strcmp(buffer,"terminate")==0)
			break;

		//Process the command
		int length = process_comm(buffer,res);

		//Return the result the client
		write(client_sockfd,res,length);

		close(client_sockfd);
	}
	
	delete [] typeTable;
	delete [] fatTable;

	close(FSsockfd);
}
