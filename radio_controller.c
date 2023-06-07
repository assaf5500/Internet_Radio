
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/stat.h>


/* =============== DEFINES ================*/

#define Buffer_size 1024 ////////////////////

#define WELCOME 0
#define ANNOUNCE 1
#define PERMIT_SONG 2
#define INVALID_COMM 3
#define NEW_STATION 4

#define MIN_SONG_SIZE 2000
#define MAX_SONG_SIZE 10485760 // = (2^20)*10 BYTES

/* ============= PROTOTYPES ===============*/

void send_hello(void);
void terminate(int);
void reset_timer(int);
void wait_4_welcome(void);
void *udp(void);
void *user(void);
void config_Mgroup(struct ip_mreq);
void change_Madder(void);
void askSong(void);
void upSong(void);
size_t filesize(char*);
void loadSong(int);
char *gets(char *str);

/* =========== GLOBAL VARIABLES ===========*/

char /*buffer[Buffer_size],*/ udp_buffer[Buffer_size], user_buffer[256];
unsigned char buffer[Buffer_size];
fd_set readfds;
int tcp_sock,udp_sock;
struct timeval timeout;
int tmp_val, user_f, flag, need_to_quit;
int len_rec, len_sent, tcp_rec;

FILE * fp, * song_fp;

uint16_t numStations=0;
char multicastGroup[16], requestedStation[16];
uint16_t portNumber;

pthread_t udp_thread, user_thread;

int i, station, user_input = 0;


void main (int argc, char *argv[])
{
	int buffer_count; //add udp socket
	
	struct sockaddr_in rec; // change rec to client

	user_f = 0;
	flag = 0;
	need_to_quit = 0;
	
	if(argc != 3) 								/* INPUT validity check */
	{
		perror("wrog amount of arguments!\n");
		exit(1);
	}
	
	memset(&rec,0,sizeof(rec));						/* reset data structures */
	memset(buffer,0,sizeof(buffer));
	
	tcp_sock = socket(AF_INET,SOCK_STREAM,0);				/* OPEN new socket & success check */
	if(tcp_sock < 0)
	{
		perror("unseccessfully tried to open socket\n");
		exit(errno);
	}
	
										/* define socket according to arguments */
	rec.sin_addr.s_addr = inet_addr(argv[1]);
	rec.sin_port = htons(atoi(argv[2]));	
	rec.sin_family = AF_INET;
	
	
	if(connect(tcp_sock,(struct sockaddr*)&rec,sizeof(rec))<0)		/* connecting to server's socket & success check */
	{
		perror("Failed to Connect!\n");
		close(tcp_sock);
		exit(EXIT_FAILURE);;
	}
	buffer_count = 0;
	
	
	send_hello();
	puts("HELLO msg sent\n");
	wait_4_welcome();
	puts("TCP connection ESTABLISHED (done welcome)\n");
	/* ====================== send hello ========================= */
	
	printf("Currently there are %d running stations.\n",numStations);
	
	
	if(pthread_create(&udp_thread,NULL,udp,NULL) != 0)   ///// maybe udp() is a problem
	{
		perror("creating UDP thread failed!\n");
		terminate(0);
	}

	if(pthread_create(&user_thread,NULL,user,NULL) != 0)   ///// maybe user() is a problem
	{
		perror("creating USER thread failed!\n");
		terminate(1);
	}
	
	
			
	if(pthread_join(udp_thread,NULL)) { perror("Failed using Pthread_join for UDP thread!\n"); }
	if(pthread_join(user_thread,NULL)) { perror("Failed using Pthread_join for USER thread!\n"); }
	terminate(2);

}

void send_hello(void)
{
	uint8_t commandType = 0;
	uint16_t reserved = 0;
	
	buffer[0] = htons(commandType);
	buffer[1] = htons(reserved);
	
	len_sent = send(tcp_sock,buffer,3,0);
	if(len_sent < 0)
	{
		perror("Error eccurred while sending\n");
		terminate(0);
	}


}

void terminate(int r)
{
	printf("\nterminating\n");
	close(tcp_sock);
	if (r > 0) { pthread_exit(&udp_thread); }
	if(r > 1) { close(udp_sock); }/////////////add udp socket
	if(user_f) { pthread_exit(&user_thread); }
	printf("\nterminated\n");
	
	exit(1);
}

void reset_timer(int s)
{
	FD_ZERO(&readfds);
	FD_SET(user_input, &readfds);
	FD_SET(tcp_sock, &readfds);
	
	if(s == 2) // 2 sec
	{
		timeout.tv_sec = 2;
		timeout.tv_usec = 0; 
	}
	if(s == 3) // 0.3 sec
	{
		timeout.tv_sec = 0;
		timeout.tv_usec = 300000; 
	} 		
}

void wait_4_welcome(void)
{
	int in_b;
	reset_timer(3);
	tmp_val = select(tcp_sock+1,&readfds,NULL,NULL,&timeout);
	if( tmp_val < 1 )
	{
		if(tmp_val < 0)
		{
			printf("error eccured during using \"select()\" function\n");
			terminate(0);
		}
		printf("Wait for Welcome timed out!\n");
		terminate(0);
	}
		
	if(FD_ISSET(tcp_sock,&readfds) == 0)
	{
		printf("Welcome message timed out!\n");
		terminate(0);
	}
	
	in_b = recv(tcp_sock,buffer,Buffer_size,0);
	
	//for(i=0 ; i<9 ; i++ ) {printf("buffer[%d] = %d\n",i,(int)buffer[i]);}
	
	if(in_b < 0)
	{
		perror("Error eccurred while receiving\n");
		terminate(0);
	}

	if(buffer[0] == INVALID_COMM)
	{
		int s_inv = buffer[1];
		char arr[s_inv];
		for(i=0;i<s_inv;i++) { arr[i] = buffer[i+2]; }
		printf("Due to %s client will now terminate!\n",arr);
		terminate(0);
	}
	
	else if(buffer[0] != WELCOME)
	{
		printf("Naughty server! Bye\n");
		terminate(0);
	}

	if(in_b != 9)
	{
		printf("Wrong amount of arguments has arrived\nshutting down!\n");
		terminate(0);
	}
	
	numStations = (int)(buffer[1]);
	numStations <<= 8;
	numStations += (int)(buffer[2]);
	

	
	sprintf(multicastGroup,"%d.%d.%d.%d",(int)buffer[6],(int)buffer[5],(int)buffer[4],(int)buffer[3]);
		
	strcpy(requestedStation,multicastGroup);
	
	portNumber = (int)(buffer[7]);
	portNumber <<= 8;
	portNumber += (int)(buffer[8]);

	printf("\n m_group : %s  |  p_num: %d  | requestedStation: %s\n ",multicastGroup, portNumber,requestedStation);

}


void *udp(void)
{
	int buffer_count = 0;
	
	struct sockaddr_in udp_rec;
	struct ip_mreq mreq;
	socklen_t size;
	

	
	memset(&udp_rec,0,sizeof(udp_rec));						/* reset data structures */
	memset(udp_buffer,0,sizeof(udp_buffer));
	
	udp_sock = socket(AF_INET,SOCK_DGRAM,0);				/* OPEN new socket & success check */
	if(udp_sock < 0)
	{
		perror("unseccessfully tried to open socket\n");
		terminate(1);
	}	
	
	udp_rec.sin_addr.s_addr = htonl(INADDR_ANY);//inet_addr(argv[1]);
	udp_rec.sin_port = htons(portNumber);	
	udp_rec.sin_family = AF_INET;	

	if(bind(udp_sock, (struct sockaddr*)&udp_rec, sizeof(udp_rec)) < 0)		/* assisgn address to socket */
	{
		perror("BIND Failed!\n");
		terminate(2);
	}

/////////////////////////////////////////////////NEED TO WRITE THE FUNCTION THAT CONFIGURE MULTICAST ADDRESS///////////

	mreq.imr_multiaddr.s_addr = inet_addr(requestedStation);
	mreq.imr_interface.s_addr = htonl(INADDR_ANY);

	if(setsockopt(udp_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq,sizeof(mreq)) < 0)
	{
		perror("attempt to use \"setsockopt()\" has failed!\n");
		terminate(2);		
	}
	size = sizeof(udp_rec);
			
	fp = popen("play -t mp3 -> /dev/null 2>&1", "w"); //open a pipe. output is sent to dev/null (hell).
	
	
	
	while(1)			/* receiving data loop */
	{
		if(flag || need_to_quit)
		{
			flag = 0;
			config_Mgroup(mreq);
			//printf("flag = %d , need_to_quit = %d\n",flag, need_to_quit);
			if(need_to_quit) { break; }
		} 
	
		len_rec = recvfrom(udp_sock,udp_buffer,Buffer_size,0,(struct sockaddr *) &udp_rec, &size);
				
		if(len_rec < 0)
		{
			perror("The peer has performed an orderly shutdown\n");   
			terminate(2);
		}
		
		else
		{
			//printf("%s",udp_buffer);
			fwrite(udp_buffer,sizeof(char),Buffer_size,fp);
			memset((void *)udp_buffer,0,Buffer_size);
			
		}
	}

}


void config_Mgroup(struct ip_mreq mreq)
{
	if(setsockopt(udp_sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq,sizeof(mreq)) < 0)
	{
		perror("attempt to use \"setsockopt()\" has failed!\n");
		close(udp_sock);
		exit(errno);		
	}
	if(need_to_quit){ return; }
	change_Madder();
	
	mreq.imr_multiaddr.s_addr = inet_addr(requestedStation);/////////////need to check about change station (addresswise)

	if(setsockopt(udp_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq,sizeof(mreq)) < 0)
	{
		perror("attempt to use \"setsockopt()\" has failed!\n");
		terminate(2);		
	}


}


void change_Madder(void)
{
	
	char temp[3];
	
	int octt1,octt2,octt3,octt4, station/* = user_select*/,mod,val;
	
	strncpy(temp,&multicastGroup/*requestedStation*/[0],3);
	octt1 = atoi(temp);
	strncpy(temp,&multicastGroup/*requestedStation*/[4],3);
	octt2 = atoi(temp);
	strncpy(temp,&multicastGroup/*requestedStation*/[8],3);
	octt3 = atoi(temp);
	strncpy(temp,&multicastGroup/*requestedStation*/[12],3);
	octt4 = atoi(temp);
	
	octt4 += station;
	val = octt4;
	mod = val % 255;
	octt4 = mod;
	
	val -= octt4;
	val /= 255;
	val += octt3;
	mod = val % 255;
	
	octt3 = mod;
	
	val -= octt3;
	val /= 255;
	octt2 += val;
	
	
	sprintf(requestedStation,"%d.%d.%d.%d",octt1,octt2,octt3,octt4);
}


void *user(void)
{
	user_f = 1;
	
	puts("\nAAAAAAAAAAAAAAAAAA\n");	
	while(1)
	{
	
		FD_ZERO(&readfds);
		FD_SET(user_input,&readfds);
		FD_SET(tcp_sock,&readfds);
		
		//fflush(stdout)?
			
		printf("\n@==================================================@\n\n");
		printf("Welcome to bar-ass's internet radio!\n");
		printf("please keep in mind that bar-ass always RULEZZ...\n\n");
		printf("@==================================================@\n\n");
		
		printf("ENTER a command from the following options:\n");
		printf("	1) choose station to listen (range 0 to %d)\n", numStations-1);////// does -1 is needed?????///////////////////
		printf("	2) Type 's' for uploading a new song to a newstation\n");
		printf("	3) Type 'q' if you would like to Quit\n\n");
		
		if(select(tcp_sock+1,&readfds, NULL,NULL,NULL) < 0)
		{
			printf("error eccured during using \"select()\" function\n");
			terminate(2);
		}
/////////////////////////////// lets start from here //////////////////////////////////				
	    	if(FD_ISSET(user_input,&readfds))
	    	{      
				memset((void*) user_buffer,0,31);
				read(user_input,(void*)user_buffer,30);
		
				switch(user_buffer[0])
				{
					case 'Q':
						printf("It was lovely! Bye Bye now..\n");
						terminate(2);
					case 'q':
						printf("It was lovely! Bye Bye now..\n");
						terminate(2);
					case 'S':   
						upSong();
						break;
					case 's':
						upSong();
						break;
					default:
						printf("user_buffer = %d numStations = %d!!\n",atoi(&user_buffer[0]),numStations);
						if(atoi(&user_buffer[0]) > numStations)
						{
							printf("There is no such station!\n");
							printf("Try again but choose positive number under %d.\n",numStations);
							break;
						}
						askSong();
						break;
		
				}
	   	}
		else
		{
			tcp_rec = recv(tcp_sock,buffer,Buffer_size,0);
			if(tcp_rec < 1)
			{
				if(len_rec < 0) { perror("Error eccurred while receiving\n"); }
				else {  perror("The server has performed an orderly shutdown of TCP session\n");  }   
				terminate(2);
			}
			
			else if (buffer[0] == NEW_STATION) 
			{  
				numStations++;
				if (numStations != buffer[1])
				{
					printf("Naughty server! wrong amount of stations. Bye...\n");
					terminate(2);
				}
				printf("We got an update!\n");
				printf("there are now %d stations avelable.\n",numStations);
				 
			}
			else 
			{
				printf("Naughty server! Bye...\n");
				terminate(2);
			}
			
		}
		
		
	
	
	}		
}


void askSong(void)
{
	uint8_t commandType = 1;
	uint16_t station_number = atoi(&user_buffer[0]);
	//station_number <<= 8;
	//station_number += atoi(&user_buffer[1]);
	
	buffer[0] = 1; ///// MAYBE PROBLEM HERE (cast int to char)
	//buffer[1] = user_buffer[0];
	//buffer[2] = user_buffer[1];
	buffer[2] = atoi(&user_buffer[0]);
	buffer[1] = 0;
	len_sent = send(tcp_sock,buffer,3,0);
	if(len_sent < 0)
	{
		perror("Error eccurred while sending\n");
		terminate(0);
	}
	
	reset_timer(3);	
	
	while(1)
	{
		tmp_val = select(tcp_sock+1,&readfds,NULL,NULL,&timeout);
		if( tmp_val < 1 )
		{
			if(tmp_val < 0)
			{
				printf("error eccured during using \"select()\" function\n");
				terminate(2);
			}
			printf("Wait for Announce timed out!\n");
			terminate(2);
		}
		
		if (FD_ISSET(tcp_sock,&readfds))//no timeout
		{
			tcp_rec = recv(tcp_sock,buffer,Buffer_size,0);
			if(tcp_rec < 1)
			{
				if(len_rec < 0) { perror("Error eccurred while receiving\n"); }
				else {  perror("The server has performed an orderly shutdown of TCP session\n");  }   
				terminate(2);
			}
			
			if(buffer[0] == NEW_STATION)
			{
				numStations++;
				if (numStations != buffer[1])
				{
					printf("Naughty server! wrong amount of stations. Bye...\n");
					terminate(2);
				}
				printf("We got an update!\n");
				printf("there are now %d stations avelable.\n",numStations);
				
				FD_CLR(tcp_sock, &readfds);  //  need to check not reseting corectlly ??????????????????????????
				
				continue;
			}
			if(buffer[0] == INVALID_COMM)
			{
				int s_inv = buffer[1];
				char arr[s_inv];
				for(i=0;i<s_inv;i++) { arr[i] = buffer[i+2]; }
				printf("Due to %s client will now terminate!\n",arr);
				terminate(2);
			}
			if(buffer[0] == ANNOUNCE)
			{
				int name_size = atoi(&buffer[1]);
				char song_name[name_size];
				
				strcpy(song_name,buffer+2);
				
				printf("\n Song Name is: %s\n",song_name);
				if(name_size == strlen(song_name))
				{
					printf("\n Announce message successfully received!\n");
					station = station_number;
					flag=1;
					return;
				}
				else
				{
					printf("corrupted Announce message! program closing...\n");
					terminate(2);
				}
			}
			else
			{
				printf("wait for announce failed!\n");
				terminate(2);
			}	
		}
		else
		{
			printf("Announce message timed out!\n");
			terminate(2);
		}
	}
}

size_t filesize(char* filename)
{
    struct stat st;
    if(stat(filename, &st) != 0) {
        return 0;
    }
    return st.st_size;   
}


void upSong(void)
{
	
	int songNamesize, song_size;
	char song_name[256];
	
	
	
	printf(" Enter song name (full file name) to upload:\n");
	gets(song_name);
	
	songNamesize = strlen(song_name);
	
	if((song_fp=fopen(song_name,"r")) == NULL)
	{
		printf(" INVALID song name\n");
		return;
	}
	song_size = filesize(song_name);
	
	if(song_size >= MIN_SONG_SIZE  && song_size <= MAX_SONG_SIZE)
	{
		buffer[0] = 2; // UpSong command Type
		buffer[1] = song_size;			//MAYBE PROBLEM WITH RETORN VALUE OF FILESIZE FUNC
		buffer[5] = songNamesize;
		memcpy(buffer+6, song_name, songNamesize);
		
		len_sent = send(tcp_sock, buffer, songNamesize + 6,0);
		if(len_sent < 0)
		{
			perror("Error eccurred while sending\n");
			terminate(2);
		}
		
		reset_timer(3);
		
		while(1)
		{		
	 		tmp_val = select(tcp_sock+1,&readfds,NULL,NULL,&timeout);
	 		if( tmp_val < 1 )
	 		{
				if(tmp_val < 0)
				{
					printf("error eccured during using \"select()\" function\n");
					terminate(2);
				}
				printf("Wait for Permit timed out! Bye..\n");
				terminate(2);
			}
			if (FD_ISSET(tcp_sock,&readfds))//no timeout
			{
				tcp_rec = recv(tcp_sock,buffer,Buffer_size,0);
				if(tcp_rec < 1)
				{
					if(len_rec < 0) { perror("Error eccurred while receiving\n"); }
					else {  perror("The server has performed an orderly shutdown of TCP session\n");  }   
					terminate(2);
				}
				
				if(buffer[0] == NEW_STATION)
				{
					numStations++;
					if (numStations != buffer[1])
					{
						printf("Naughty server! wrong amount of stations. Bye...\n");
						terminate(2);
					}
					printf("We got an update!\n");
					printf("there are now %d stations avelable.\n",numStations);
					
					FD_CLR(tcp_sock, &readfds);  //  need to check not reseting corectlly ??????????????????????????
					
					continue;
				}
				if(buffer[0] == INVALID_COMM)
				{
					int s_inv = buffer[1];
					char arr[s_inv];
					for(i=0;i<s_inv;i++) { arr[i] = buffer[i+2]; }
					printf("Due to %s client will now terminate!\n",arr);
					terminate(2);
				}
				if(buffer[0] == PERMIT_SONG)
				{
					if(buffer[1] == 1)
					{
						printf("ready to upload song.\n");
						loadSong(song_size);
						printf("song uploaded successfuly!\n");
						return;
					}
					else if(buffer[1] == 0)
					{
						printf("server is busy, please try again later\n");
						return;
					}
					else
					{
						printf("corrupted permit received.\n");
						terminate(2);
					}
					
						
				}
				else
				{
					printf("FAILED to PERMIT!\n");
					terminate(2);
				}
			}
		}

	}
	else
	{
		printf("Song size is out of range.\n");
		terminate(2);
	}
}

void loadSong(int song_size)
{
	int has_sent = 0;
	
	while(/*has_sent < song_size &&*/ fscanf(song_fp,"%1024c",buffer) != EOF )
	{
		len_sent = send(tcp_sock, buffer, strlen(buffer),0);
		if(len_sent < 0)
		{
			perror("Error eccurred while sending\n");
			terminate(2);
		}
		//has_sent += len_sent;    //MAYBE WE DONT NEED?!?!?!?!?!?!?!?!?!?!?!?!?!?!??!? EOF is enough...
	
		//fflush(stdout);
		//usleep(usec);		//needed?
	}
	
	reset_timer(2);
	tmp_val = select(tcp_sock+1,&readfds,NULL,NULL,&timeout);
	if( tmp_val < 1 )
	{
		if(tmp_val < 0)
		{
			printf("error eccured during using \"select()\" function\n");
			terminate(2);
		}
		printf("Wait for NesStations message timed out! Bye..\n");
		terminate(2);
	}
	if (FD_ISSET(tcp_sock,&readfds))//no timeout
	{
		tcp_rec = recv(tcp_sock,buffer,Buffer_size,0);
		if(tcp_rec < 1)
		{
			if(len_rec < 0) { perror("Error eccurred while receiving\n"); }
			else {  perror("The server has performed an orderly shutdown of TCP session\n");  }   
			terminate(2);
		}
		
		if(buffer[0] == NEW_STATION)
		{
			numStations++;
			if (numStations != buffer[1])
			{
				printf("Naughty server! wrong amount of stations. Bye...\n");
				terminate(2);
			}
			printf("We got an update!\n");
			printf("there are now %d stations avelable.\n",numStations);
			
			//FD_CLR(tcp_sock, &readfds);  //  need to check not reseting corectlly ??????????????????????????	
		}
		else
		{
			printf("ITS NOT newStations AFTER UPLOADING SONG. Bye!\n");
			terminate(2);
		}
	}
}




