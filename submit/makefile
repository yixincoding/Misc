all: BDS BDC randomBDC IDS FS FC

BDS: BasicDisk.c
	g++ BasicDisk.c -o BDS 

BDC: cmdClient.c
	g++ cmdClient.c -o BDC 

randomBDC: randomClient.c
	g++ randomClient.c -o randomBDC 

IDS: IntelliDisk.c
	g++ IntelliDisk.c -o IDS

FS: FileServer.cpp
	g++ FileServer.cpp -o FS

FC: FileClient.c
	g++ FileClient.c -o FC
