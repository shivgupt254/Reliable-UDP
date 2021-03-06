#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <math.h>
#include <sys/time.h>


int fd;
int fileLength;
sockaddr_in saddr,caddr;
const unsigned int packetsize = 1222;
struct hostent *server;
char data[1222];
char request[1222];
char ackresp[1222];
bool receiving;
unsigned int ack,seq=0;
int rwnd,firstseq;
unsigned int sfseq;

using namespace std;

struct RUDPHeader
{
	char SEQ[10];
	char ACK[10];
	char F;		//fin fla
	char A;		//ack flag

};

struct RUDPpacket
{
	RUDPHeader H;
	char data[packetsize-22];
}req,res,*packetbuffer;


unsigned int atoi (char a[]);
void itoa(unsigned int a,char b[])
{
        char t[10];
        int i=0,k;
        while(a != 0)
        {
		t[i++] = a%10 + 48;
                a /= 10;
        }
        if(i<10)
            for(k=0; k < 10-i; k++)
                b[k] = '0';

        while(i>0)
        {
       		 b[k++] = t[i-1];
                 i--;
        }
}

void shiftWindow(int k)
{
    int i;

    for(i=0;i<k;i++)
        puts(packetbuffer[i].data);
    for(i=0;i<rwnd-k;i++)
    {
        memcpy(packetbuffer[i].data,res.data,sizeof(packetbuffer[i+k].data));
        memcpy(packetbuffer[i].H.SEQ,res.H.SEQ,sizeof(packetbuffer[i+k].H.SEQ));
        packetbuffer[i].H.A=packetbuffer[i+k].H.A;
    }

    for(i;i<rwnd;i++)
    {
        packetbuffer[i].H.A = '0';
        bzero(packetbuffer[i].data,sizeof(packetbuffer[i].data));
        bzero(packetbuffer[i].H.SEQ,sizeof(packetbuffer[i].H.SEQ));
    }
    sfseq = sfseq +((packetsize+1)*k);
}
void parseResponse()
{
    int i,k=0;
    char temp[10];
    for(i=0; i < 10; i++)
    {
        res.H.SEQ[i] = data[k];
        temp[i] = data[k++];
    }
    temp[i]='\0';

    for(i=0; i < 10; i++)
    {
        res.H.ACK[i] = data[k++];
    }

    res.H.F = data[k++];
    res.H.A = data[k++];
    i=0;
    bzero(res.data,packetsize-22);
    while(k < strlen(data))
    {
       res.data[i++] = data[k++];
    }

//    if ( res.H.A == '1')
//        ack = atoi(res.H.ACK);
    seq = atoi(temp);

    if (res.H.F == '1')
        receiving = false;
}

void createRequest(char file_name[])
{
    int i,k=0,j=0;
   // itoa(0,req.H.ACK);

    for (i=0;i<10;i++)
        req.H.SEQ[i] = '0';

    for (i=0;i<10;i++)
        req.H.ACK[i] = '0';
    req.H.F = '0';
    req.H.A = '0';

    for(i=0;i<strlen(file_name);i++)
        req.data[i]=file_name[i];

    bzero(request,packetsize);
    for(i=0;i<10;i++)
        request[j++] = req.H.SEQ[i];
    for(i=0;i<10;i++)
        request[j++] = req.H.ACK[i];
    request[j++] = req.H.F;
    request[j++] = req.H.A;
    for(i = 0; i<strlen(req.data); i++)
        request[j++] = req.data[i];
}

void createAck()
{
    int i,k=0,j=0;
    itoa(seq+packetsize+1,req.H.ACK);

    for (i=0;i<10;i++)
        req.H.SEQ[i] = '0';

    if (res.H.F == '1')
        req.H.F = '1';
    else
        req.H.F = '0';
    req.H.A = '1';

    for(i=0;i<packetsize-22;i++)
        req.data[i]='0';

    bzero(ackresp,packetsize);
    for(i=0;i<10;i++)
        ackresp[j++] = req.H.SEQ[i];
    for(i=0;i<10;i++)
        ackresp[j++] = req.H.ACK[i];
    ackresp[j++] = req.H.F;
    ackresp[j++] = req.H.A;
    for(i = 0; i<strlen(req.data); i++)
        ackresp[j++] = req.data[i];
}

void createDupAck(char buffer[],unsigned int seq)
{
    int i,k=0,j=0;
    RUDPpacket dupAck;
    itoa(seq,dupAck.H.ACK);

    for (i=0;i<10;i++)
        dupAck.H.SEQ[i] = '0';

if(res.H.F=='1')
{
 dupAck.H.F = '1';
 receiving=false;
 }
 else
    dupAck.H.F = '0';
    dupAck.H.A = '1';
dupAck.data[0]='\0';
    for(i=0;i<packetsize-22;i++)
        dupAck.data[i]='0';

    bzero(buffer,packetsize);
    for(i=0;i<10;i++)
        buffer[j++] = dupAck.H.SEQ[i];
    for(i=0;i<10;i++)
        buffer[j++] = dupAck.H.ACK[i];
    buffer[j++] = dupAck.H.F;
    buffer[j++] = dupAck.H.A;
    for(i = 0; i<strlen(dupAck.data)+1; i++)
        buffer[j++] = dupAck.data[i];

}

void read_write()
{
    struct timeval s,e;
    char ch;
    int n,i,k;
    int sack;
    unsigned int t;
    //int cnt=0;
    bzero(data,packetsize);
    socklen_t len = sizeof(struct sockaddr_in);
    gettimeofday(&s,NULL);
    sendto(fd,request,packetsize,0,(const struct sockaddr *)&saddr, len);
    receiving = true;
    recvfrom(fd,data,packetsize,0,(struct sockaddr *)&caddr,&len);
    parseResponse();
    memcpy(packetbuffer[0].data,res.data,sizeof(res.data));
    memcpy(packetbuffer[0].H.SEQ,res.H.SEQ,sizeof(res.H.SEQ));
    packetbuffer[0].H.A='1';                    //using this bit of header to check if buffer is empty or not

    firstseq=sfseq=seq;
    createAck();
    sendto(fd,ackresp,packetsize,0,(const struct sockaddr *)&saddr, len);

    while(receiving)
    {
        n = rand()%7;
        bzero(data,packetsize);
        recvfrom(fd,data,packetsize,0,(struct sockaddr *)&saddr,&len);
        //if(n+1==5)
        //    continue;
        parseResponse();
        t = sfseq+(rwnd*1223);
        if(seq<sfseq)
        {
            cout << "\nDropping duplicate packet!!\n";
            createDupAck(ackresp,sfseq);
            sendto(fd,ackresp,strlen(ackresp),0,(const struct sockaddr *)&saddr, len);
            continue;
        }
        else if(seq>t)
        {
            cout << "\nWindow is full,dropping packet!!\n";
            createDupAck(ackresp,sfseq);
            sendto(fd,ackresp,packetsize,0,(const struct sockaddr *)&saddr, len);
            continue;
        }

        sack = ((seq-sfseq)/1223);
        memcpy(packetbuffer[sack].data,res.data,sizeof(res.data));
        memcpy(packetbuffer[sack].H.SEQ,res.H.SEQ,sizeof(res.H.SEQ));
        packetbuffer[sack].H.A='1';         //using this bit of header to check if buffer is empty or not

        for(i =0; i<rwnd;i++)
            if(packetbuffer[i].H.A != '1')
                break;

        if(i>0)
            shiftWindow(i);

        for(i=0;i<rwnd;i++)
            if(packetbuffer[i].H.A == '1')
                break;
        if(i < rwnd)
        {
            for(k=0;k<i;k++)
            {

                seq = sfseq+((packetsize+1)*k);
                createDupAck(ackresp,seq);
                cout << "\nDuplicate ack being send : ";
                puts(ackresp);
                sendto(fd,ackresp,packetsize,0,(const struct sockaddr *)&saddr, len);
                break;
            }
            continue;
        }
		createAck();
	//	if(res.H.F == '1')
          	  sendto(fd,ackresp,packetsize,0,(const struct sockaddr *)&saddr, len);

	//	else if (n!=0)
	//	{
         //   sendto(fd,ackresp,packetsize,0,(const struct sockaddr *)&saddr, len);
        //}
    }
    for(i =0; i<rwnd;i++)
            if(packetbuffer[i].H.A != '1')
                break;

        if(i>=1)
            shiftWindow(i);
    gettimeofday(&e,NULL);

    cout <<"\nRetrieval Time : " << (e.tv_sec*1000000 + e.tv_usec)-(s.tv_sec*1000000 + s.tv_usec);
}

unsigned int atoi(char a[])
{

    int len = strlen(a);
    int i=0;
    unsigned int result=0;
    while(i < len )
    {
	result = result + ( pow(10,len-i-1)*((int)(a[i]-48)));
	i++;
    }
    return result;
}

void startClient(char a[],char b[])
{
    fd = socket(AF_INET,SOCK_DGRAM,0);

    if(fd < 0)
    {
        cout << "Invalid Socket";
    }

    server = gethostbyname(a);

    saddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,(char *)&saddr.sin_addr.s_addr,server->h_length);



    unsigned int k = atoi(b);
    saddr.sin_port = htons(k);

}
int main(int argc, char *argv[])
{
 if (argc < 4)
    {
        cout << "\nInsufficient Arguments\n";
                return 0;
    }
    int i;
    for (i =0; i < argc; i++)
    {
        if(argv[i] == NULL)
        {
                cout << "\nInsufficient Arguments\n";
                return 0;
        }
    }

    int t = atoi(argv[3]);
    rwnd = t;

    packetbuffer =(RUDPpacket*) malloc(sizeof(RUDPpacket)*t);
    for(i=0;i<t;i++)
        packetbuffer[i].H.A='0';
    startClient(argv[1],argv[2]);
    createRequest(argv[4]);
    read_write();

    return 0;
}
