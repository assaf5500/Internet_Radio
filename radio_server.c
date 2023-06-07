
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

#define HELLO 0
#define ASKSONG 1
#define UPSONG 2

#define WELCOME 0
#define ANNOUNCE 1
#define PERMIT_SONG 2
#define INVALID_COMM 3
#define NEW_STATION 4

#define MIN_SONG_SIZE 2000
#define MAX_SONG_SIZE 10485760 // = (2^20)*10 BYTES


/* ============= STRUCTURES ===============*/
typedef struct sockaddr_in sockaddr_in;


typedef struct song
{
	FILE * fp;

	char buffer[Buffer_size];
	char song_name[255];
	char mc_addr[16];

	int sock;
	uint32_t songSize;
	uint8_t song_name_size;

	sockaddr_in sen;
	struct song *next;	
}song;



/* ============= PROTOTYPES ===============*/

void initial_song_list(song*, char*,int);
void print_songs(void);
char *change_Madder(int station);
void *tcp(void);
void *user(void);
void reset_timer(int,int);
void *client_handler(void);
void kill_client(int, char*);
void receive_song(char* ,int);
void terminate(void);

/* =========== GLOBAL VARIABLES ===========*/

//char /*buffer[Buffer_size],*/ udp_buffer[Buffer_size], user_buffer[256];
/*
unsigned char buffer[Buffer_size];
fd_set readfds;
int tcp_sock,udp_sock;

int user_f, flag, need_to_quit;
int len_rec, tcp_rec;

FILE * fp, * song_fp;

uint16_t numStations;
char multicastGroup[16], requestedStation[16];
uint16_t portNumber;

pthread_t udp_thread, user_thread;

int i, station,;
*/
char multicastGroup[16], requestedStation[16],user_buffer[3];
char udp_buffer[Buffer_size];
char s_tcp_p[4];

int i, welcome_sock, udp_sock, tcp_port, client_sock[100] = {0};
int num_of_clients =0 ;
int udp_port, ttl = 15,  user_input = 0;
int is_welcome_on = 0;
int terminate_flag = 1 , is_welcom_on = 0 , permit =0;

uint16_t num_of_stations;

fd_set udp_fds, tcp_fds, user_fds, welcome_fds;

pthread_t user_thread, tcp_thread;
pthread_t client[100];

song *song_head, *main_song, *song_to_play;

struct timeval timeout;


void main (int argc, char *argv[])
{
	int sen_size ,len_snt ;
	


	if(argc < 5)
	{
		printf("Not enough arguments! Shuting down..\n");
		exit(1);
	}
	strcpy(s_tcp_p,argv[1]);
	tcp_port = htons(atoi(argv[1]));
	num_of_stations = argc - 4;
	strcpy(multicastGroup,argv[2]);
	udp_port = htons(atoi(argv[3]));
	if((song_head = (song*)malloc(sizeof(song))) == NULL)
	{
		printf("Faild to malloc for head \n");
		terminate();
	}
	song_head->next = NULL;
	main_song = song_head;
	
	memset(udp_buffer,0,sizeof(udp_buffer));
	
	for(i=0 ; i < num_of_stations ;i++) { initial_song_list(song_head, argv[i+4],i); }
	
	if(pthread_create(&tcp_thread,NULL,tcp,NULL) != 0)   ///// maybe udp() is a problem
	{
		perror("creating TCP thread failed!\n");
		terminate();
	}
	
	if(pthread_create(&user_thread,NULL,user,NULL) != 0)   ///// maybe udp() is a problem
	{
		perror("creating USER thread failed!\n");
		terminate();
	}
	
	
	
	song_to_play = song_head;
	while(terminate_flag)
	{
		if(fscanf(song_to_play->fp,"%1024c",udp_buffer) == EOF)
		{
			rewind(song_to_play->fp);
			continue;			
		}
		
		sen_size = sizeof(song_to_play->sen);//->
		len_snt = sendto(song_to_play->sock,udp_buffer,Buffer_size,0,(struct sockaddr *) &song_to_play->sen, sen_size);//-> ->
		if(len_snt < 0)
		{
			perror("\nError eccurred while sending\n");
			continue;
		}
		
		if(song_to_play->next == NULL)
		{
			song_to_play = song_head;
			usleep(62500);
			continue;
		}
		song_to_play = song_to_play->next;
	}
	
	
	if(pthread_join(tcp_thread,NULL)) { perror("Failed using Pthread_join for TCP thread!\n"); }
	if(pthread_join(user_thread,NULL)) { perror("Failed using Pthread_join for USER thread!\n"); }
}


void initial_song_list(song *head, char *s_name, int num)

{
	song *new ;
	if((new = (song*)malloc(sizeof(song))) == NULL)
	{
		printf("Faild to malloc for song num %d\n",num);
		terminate();
	}
	char *temp = change_Madder(num); //char temp[16] = change_Madder(num);
	song *curr = head;
	
	memset(&new->sen,0,sizeof(new->sen));						/* reset data structures */
	
	new->next = NULL; // ->
	new->sock = socket(AF_INET,SOCK_DGRAM,0); // ->				/* OPEN new socket & success check */
	if(new->sock < 0) // ->
	{
		perror("unseccessfully tried to open socket\n");
		exit(errno);
	}
	
									/* define socket according to arguments */
	new->sen.sin_addr.s_addr = inet_addr(temp); // ->
	new->sen.sin_port = udp_port;	// ->
	new->sen.sin_family = AF_INET;// ->
//	new->song_name = s_name;// ->
	strcpy(new->song_name , s_name);

	
	if(bind(new->sock, (struct sockaddr*)&new->sen, sizeof(new->sen)) < 0)	/* assisgn address to socket */ // -> -> ->
	{
		printf("BIND client num %d has Failed!\n",num); //was perror
		terminate();
	}
	
	if(setsockopt(new->sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl,sizeof(ttl)) < 0) /* add options to socket acording to 3rd argument */
	{
		printf("attempt to use \"setsockopt()\" has for client num %d has failed!\n",num); // was perror
		terminate();		
	}
	
	new->fp = fopen(s_name,"r");						/* open file */
	if(new->fp < 0)
	{
		printf("Faild opent song file num %d\n",num);
		terminate();
	}
	
	
	while (curr->next != NULL) { curr = curr->next; }
	
	curr->next = new;
	
}

char* change_Madder(int station)
{
	char temp[3];
	int octt1,octt2,octt3,octt4/* = user_select*/,mod,val; //station was in int line here
	
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
	
	return requestedStation;
}

void *tcp(void)
{
	is_welcome_on = 1; ///when welcome socket on
	
	int size1;
	
	struct sockaddr_in tcp_sen;

	welcome_sock = socket(AF_INET,SOCK_STREAM,0);
	if(welcome_sock < 0)
	{
		printf("FAILED to open welcome socket!\n");
		terminate();
	}
	is_welcom_on = 1;
	
	tcp_sen.sin_addr.s_addr = htonl(INADDR_ANY);
	tcp_sen.sin_port = tcp_port;	
	tcp_sen.sin_family = AF_INET;
	
	size1 = sizeof(tcp_sen);// was tcp.sen
	if(bind(welcome_sock, (struct sockaddr*)&tcp_sen, size1) < 0)		/* assisgn address to socket */ // was tcp.sen
	{
		perror("BIND Failed!\n");
		terminate();
	}
	
	if(listen(welcome_sock, SOMAXCONN) < 0)					/* wait for connection  */
	{									/* by creating welcome socket */
		perror("Failed to listen!\n");
		terminate();		
	}
	FD_ZERO(&tcp_fds);
	while(1)
	{
		//reset_timer(int s)
		FD_ZERO(&welcome_fds);
		FD_SET(welcome_sock, &welcome_fds);
		
		if(select(welcome_sock+1,&welcome_fds, NULL,NULL,NULL) < 0)
		{
			printf("error eccured during using \"select()\" function\n");
			terminate();
		}
		if(FD_ISSET(welcome_sock,&welcome_fds))
		{   					// check if new welcomeSocket
			client_sock[num_of_clients] = accept(welcome_sock, (struct sockaddr*) &tcp_sen, &size1);
			if(client_sock[num_of_clients] < 0)
			{
				printf("FAILED to accept connection number %d.\n",num_of_clients);
				terminate();
			}
			FD_SET(client_sock[num_of_clients],&tcp_fds);
			if(pthread_create(&client[num_of_clients],NULL,client_handler,NULL) != 0)   ///// maybe udp() is a problem
			{
				perror("creating USER thread failed!\n");
				terminate();
			}
			usleep(5);
			num_of_clients++;
		}
	
	
	}

}

void* client_handler(void)
{
	int client_num = num_of_clients;
	int k, tmp_val, in_b, len_sent, temp_size;
	
	char client_buffer[Buffer_size];
	
	reset_timer(3,client_num);
	
	memset(client_buffer,0,sizeof(client_buffer));
	
	tmp_val = select(client_sock[client_num],&tcp_fds,NULL,NULL,&timeout);
	if( tmp_val < 1 )
	{
		if(tmp_val < 0)
		{
			printf("error eccured during using \"select()\" function\n");
			terminate();
		}
		printf("Wait for Hello timed out! TCP_session will close now.\n");
		num_of_clients--;
		close(client_sock[client_num]);
		pthread_exit(&client[client_num]);
		//return?
	}
		
	if(FD_ISSET(client_sock[client_num],&tcp_fds) == 0)
	{
		printf("Welcome message timed out!\n");
		terminate();
	}
	
	in_b = recv(client_sock[client_num],client_buffer,Buffer_size,0);
	//for(i=0 ; i<9 ; i++ ) {printf("buffer[%d] = %d\n",i,(int)buffer[i]);}
	if(in_b < 0)
	{
		perror("Error eccurred while receiving\n");
		terminate();
	}
	
	if(client_buffer[0] != HELLO || client_buffer[0] != HELLO || client_buffer[0] != HELLO ||  sizeof(client_buffer) > 3)
	{
		client_buffer[0] = htons(INVALID_COMM);
		char *text = "invalid hello has arrived!";
		client_buffer[1] = htons(strlen(text));		
		strncpy(client_buffer+2,text,strlen(text));
		/* send invalid connamd*/
		
		len_sent = send(client_sock[client_num],client_buffer,Buffer_size,0);
		if(len_sent < 0)
		{
			printf("Error eccurred while sending INVALID COMMAND for client %d.\n",client_num); // was perror
			terminate();
		}
		
		close(client_sock[client_num]);
		pthread_exit(&client[client_num]);
	}
	
	client_buffer[0] = htons(WELCOME);
	client_buffer[1] = htons(num_of_stations);
	
	char octt[4][3]/*,octt2[3],octt3[3],octt4[3]*/;
	char to_add[2], mc_ad[16];
	song *looking = song_head;
	
	strncpy(octt[0],&multicastGroup[0],3);//&
	strncpy(octt[1],&multicastGroup[4],3);//&	
	strncpy(octt[2],&multicastGroup[8],3);//&
	strncpy(octt[3],&multicastGroup[12],3);//&
	
	for(k=2; k < 6 ; k++) { strcpy(&client_buffer[k],octt[k-2]); } //&
	
	strcpy(client_buffer+7,s_tcp_p);
		
	len_sent = send(client_sock[client_num],client_buffer,Buffer_size,0);
	if(len_sent < 0)
	{
		printf("Error eccurred while sending WELCOME for client %d.\n",client_num);//was perror
		terminate();
	}
	while(1)
	{
		FD_CLR(client_sock[client_num],&tcp_fds);
		FD_SET(client_sock[client_num],&tcp_fds);
		
		if(select(client_sock[client_num]+1,&tcp_fds, NULL,NULL,NULL) < 0)
		{
			printf("error eccured during using \"select()\" function\n");
			terminate();
		}
			
	    	if(FD_ISSET(client_sock[client_num],&tcp_fds))
	    	{
	    		in_b = recv(client_sock[client_num],client_buffer,Buffer_size,0);
			//for(i=0 ; i<9 ; i++ ) {printf("buffer[%d] = %d\n",i,(int)buffer[i]);}
			if(in_b < 0)
			{
				perror("Error eccurred while receiving\n");
				terminate();
			}
			
			switch((int)client_buffer[0])   ////////////////   PROBLEM? ////////////////
			{
				case HELLO:
					kill_client(client_num,"Client sent unwarranted HELLO.. client will go to sleep\n");				
					break;
			
				case ASKSONG:
					strncpy(to_add,client_buffer+1,2);
					
					if(in_b != 3 || atoi(to_add) > num_of_stations)
					{ 	kill_client(client_num,"client is trying to play games.. hell be put down now.\n"); }
					
					strcpy(mc_ad, change_Madder(atoi(to_add)));
					while(strcmp(looking->mc_addr,mc_ad) != 0) { looking = looking->next; } // ->
					
					client_buffer[0] = htons(ANNOUNCE);
					client_buffer[1] = htons(looking->song_name_size); ///// ->
					strcpy(client_buffer+2,looking->song_name);
					
					len_sent = send(client_sock[client_num],client_buffer,Buffer_size,0);
					if(len_sent < 0)
					{
						printf("Error eccurred while sending INVALID COMMAND for client %d.\n",client_num); //was perror
						terminate();
					}
					
					break;
				
				case UPSONG:
					receive_song(client_buffer,client_num);
					break;
				default:
					kill_client(client_num,"Client sent an unknown Command-Type message.");
					break;
			
			}
	    } 
	}	
}

void receive_song(char* buff,int c_num)
{
	uint8_t commandType = 2;
	uint16_t amount;
	int n, len_sent,select_res,len_rec;
	uint8_t s_nsize=0;
	uint32_t s_size=0,byte_count=0;
	song *new = (song*)malloc(sizeof(song));
	song *temp = main_song; // was *temp

	if(permit)
	{
		//song *new = (song*)malloc(sizeof(song)); /// change
		//song *temp = main_song; // was *temp
		
		s_size = (int)(buff[1]);
		s_size <<= 8;
		s_size += (int)(buff[2]);
		s_size <<= 8;
		s_size += (int)(buff[3]);
		s_size <<= 8;
		s_size += (int)(buff[4]);
		
		s_nsize = (int)(buff[5]);
				
		strncpy(new->song_name,buff+6,s_nsize);
		
		new->songSize = s_size;
		new->song_name_size = s_nsize;
		
	}
	
	buff[0] = htons(commandType);
	buff[1] = htons(permit);
	
	len_sent = send(client_sock[c_num],buff,Buffer_size/*2*/,0);
	if(len_sent < 0)
	{
		perror("Error occurred while sending\n");
		terminate();
	}
	
	if(permit == 0) { return; }	//not aloud to upload
	
	permit = 0;	/* "MUTEX up" */
	
	while(temp->next != NULL) { temp = temp->next; }
	
	new->fp = fopen(new->song_name,"w");
	reset_timer(30,c_num);
	while(byte_count < s_size)					/* receiving & printing data loop */
	{
		FD_CLR(client_sock[c_num],&tcp_fds);
		FD_SET(client_sock[c_num],&tcp_fds);
		
		select_res =select(client_sock[c_num]+1,&tcp_fds, NULL,NULL,&timeout); // was readfds
		if(select_res < 0)
		{
			printf("error occurred during using \"select()\" function\n");
			terminate();
		}
		if(select_res == 0)
		{
			printf("select timed out!!\nclient have gap while uploading song.\n");
			kill_client(c_num,"GAP of receiving data detected (timed out).\n");
			
		}
	    if(FD_ISSET(client_sock[c_num],&tcp_fds))
	   	{
		
			len_rec = recv(client_sock[c_num],buff,Buffer_size,0);
			if(len_rec < 0)
			{
				perror("Error occurred while receiving\n"); 
				terminate(); 
			}
			byte_count += len_rec;
		
			if(fprintf(new->fp,"%1024c",buff) == EOF) { break; }/////////////problem!!!!!!..!=EOF!!!!!!
			
			reset_timer(30,c_num);
		}
	}
	//rewind(new->fp);
	fclose(new->fp);
	new->fp = fopen(new->song_name,"r");
	if(new->fp < 0)
	{
		printf("Faild opent song file num %d\n",num_of_stations);
		terminate();
	}
	num_of_stations++;
	//new->mc_addr = change_Madder(num_of_stations);
	strcpy(new->mc_addr , change_Madder(num_of_stations));
	amount = num_of_stations;
	new->next = NULL;

	new->sock = socket(AF_INET,SOCK_DGRAM,0);// ->				/* OPEN new socket & success check */
	if(new->sock < 0)//->
	{
		perror("unseccessfully tried to open socket\n");
		exit(errno);
	}
									/* define socket according to arguments */
	new->sen.sin_addr.s_addr = inet_addr(new->mc_addr);
	new->sen.sin_port = udp_port;	
	new->sen.sin_family = AF_INET;
	//new->song_name = s_name;

	if(bind(new->sock, (struct sockaddr*)&new->sen, sizeof(new->sen)) < 0)		/* assisgn address to socket */
	{
		printf("BIND client num %d has Failed!\n",c_num);
		terminate();
	}
	
	if(setsockopt(new->sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl,sizeof(ttl)) < 0) /* add options to socket acording to 3rd argument */
	{
		printf("attempt to use \"setsockopt()\" has for client num %d has failed!\n",c_num);
		terminate();		
	}
		
	temp->next = new;
	
	permit = 1;	/* "MUTEX down" */
	
	buff[0] = htons(NEW_STATION);
	buff[2] = htons(amount & 0xFF);
	amount >>= 8;
	buff[1] = htons(amount & 0xFF);
	
	for(n=0 ; n<num_of_clients ; n++)
	{	
		len_sent = send(client_sock[n],buff,Buffer_size,0);
		if(len_sent < 0)
		{
			printf("Error eccurred while sending INVALID COMMAND for client %d.\n",c_num);
			terminate();
		}
	}
}

void kill_client(int c_num,char *err)
{
	int len;
	char client_buffer[Buffer_size];/////////////////
	
	client_buffer[0] = htons(3);
	client_buffer[1] = htons(strlen(err));		
	strncpy(client_buffer+2,err,strlen(err));
	/* send invalid connamd*/
	
	len = send(client_sock[c_num],client_buffer,Buffer_size,0);
	if(len < 0)
	{
		printf("Error eccurred while sending INVALID COMMAND for client %d.\n",c_num); //was perror instead of printf
		terminate();
	}
	
	close(client_sock[c_num]);
	pthread_exit(&client[c_num]);
}




void print_songs(void)
{
	int j = 1;
	
	song *curr_temp = main_song;
	
	printf("Nice choice!\n");
	printf("Currently, there are %d clients listening to our station.\n",num_of_clients);
	printf("\nHere is our JookBox music collection list:\n");
	printf("===========================================");
			
	while(curr_temp != NULL)
	{
		printf("# %d)\n",j);
		printf("Song name:  %s\n",curr_temp->song_name); /////check
		printf("The song is played on IP_address: %s\n",curr_temp->mc_addr); ////curr_temp->mc_addr was .
		
		curr_temp = curr_temp->next;
		j++;
	}
	
}

void *user(void)
{
	while(1)
	{
		FD_ZERO(&user_fds);
		FD_SET(user_input,&user_fds);
			
		printf("\n@==================================================@\n\n");
		printf("Welcome to bar-ass's internet radio erver!\n");
		printf("please keep in mind that bar-ass always RULEZZ...\n\n");
		printf("@==================================================@\n\n");	
		
		printf("ENTER a command from the following options:\n");
		printf("	1) Type 'p' if you want to print current info on active stations.\n");
		printf("	2) Type 'q' if you would like to Quit.\n\n");
	
		if(select(1,&user_fds, NULL,NULL,NULL) < 0)
		{
			printf("error eccured during using \"select()\" function\n");
			terminate();
		}
			
	    	if(FD_ISSET(user_input,&user_fds))//for  safty
	    	{      
			memset((void*) user_buffer,0,3);
			read(user_input,(void*)user_buffer,3);
			
			switch(user_buffer[0])
			{
			case 'Q':
			case 'q':
				printf("It was lovely! Bye Bye now..\n");
				terminate_flag = 0;
				terminate();
			case 'P':
			case 'p':   
				print_songs();
				break;
			default:		
				printf("You idiot!\n");
				printf("Try again but choose from what we said you can choose\n\n");
				break;
			
		
			}
		}
		
	}
		
}




void terminate(void)
{
	printf("\nterminating\n");
	close(udp_sock);
	if (is_welcom_on) { close(welcome_sock); }
	if(num_of_clients)
	{
		for(i=0 ; i<num_of_clients ; i++)
		{
			close(client_sock[i]);
		 	pthread_exit(&client[i]);		 
		}
	}
		
	
	/* need to add loop to deelete song linked list AND MORE  */
	
	printf("\nterminated\n");
	
	exit(1);
}


void reset_timer(int s,int c_num)
{
	FD_ZERO(&tcp_fds);
	FD_SET(client_sock[c_num], &tcp_fds);///////////////////
	
	if(s == 30) // 3 sec
	{
		timeout.tv_sec = 3;
		timeout.tv_usec = 0; 
	}
	if(s == 3) // 0.3 sec
	{
		timeout.tv_sec = 0;
		timeout.tv_usec = 300000; 
	} 		
}















