#include <stdio.h>		// standard input / output functions
#include <fcntl.h>		// File control definitions
#include <stdint.h>     // standard integer definitions
#include <stdlib.h>		
#include <string.h>     // string function definitions
#include <unistd.h>     // UNIX standard function definitions    
#include <errno.h>      // Error number definitions
#include <termios.h>    // POSIX terminal control definitions
#include <time.h>		// time and timer definitions
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <threads.h>
#include <string.h>

//#include <libusb.h>
 #include <sys/random.h> //for some randomness

#define MAX_SIZE 0x1000
#define EXT_FZORC 11
#define MAX_FZORC 10
#define ITERS 10
#define MAX_SIZE_LOG 4096

//#define VidSamsung 0x04E8
//#define PidGalaxyS2 0x685D
//global vars

const char* INTFC = "/dev/ttyACM0";
//const char INTFC[] = "/dev/ttyACM0";
const char* OUTLOG = "output.log";
const char* FUZZDAT = "ats.txt";
int device_fd = -1;

struct
{

     char *at;
     char *param;
     char *data;

}

fzorc[] =
{

     {"", "=", ""}, //0
     {"", "?", ""}, //1
     {"", "=?", ""}, //2
     {"", "\"", ""}, //3
     {"", ",", ""}, //4
     {"", "*#", ""}, //5
     {"", "#",""}, //6
     {"", "*",""}, //7
     {"",";",""}, //8
     {"",">",""}, //9
     {"","",""}
    
     
};



//function prototypes
void *ermall(size_t size);
void write_file(const unsigned char*);
int init_tty(size_t, size_t, size_t);
void set_blocking (int, int);
int open_dev(const char* , size_t , int);
void write_at(ssize_t, const unsigned char* );
int read_at(ssize_t, unsigned char* , size_t);
void close_dev(size_t);



//writes data to file
void write_file(const unsigned char* data)
{
	ssize_t fd = open(OUTLOG, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0666);
	size_t err;
	size_t len = (strlen(data));
	if (fd == -1)
		perror("open");
	while (len !=0 && (err = write(fd, data, len)) !=0)
	{
		if(err == -1)
		{
			if(errno==EINTR)
				continue;
			perror("write");
			break;
		}
		if(len < 0 || len > (MAX_SIZE-1))
			printf("[write]: wrong buffer length value");
			
		len -= err;
		data += err;

	}

}


int init_tty(size_t dev, size_t speed, size_t size)
{
  struct termios tty;
  struct termios tty_old;
  memset (&tty, 0, sizeof tty);
  if (tcgetattr (dev, &tty) != 0)
        {
                perror ("tty");
                return -1;
        }
  //save old tty      
  tty_old = tty;

  //set speed
  cfsetispeed (&tty, speed);
  cfsetospeed (&tty, speed);

  /* set port stuff : control mode flags */
tty.c_cflag     &=  ~PARENB;    //      unsetting specific bits of tty.c_cflag struct
tty.c_cflag     &=  ~CSTOPB;	//
tty.c_cflag     &=  ~CSIZE;		//
tty.c_cflag     |=  CS8;				//8bit chars

tty.c_cflag     &=  ~CRTSCTS;           // no flow control
tty.c_cc[VMIN]   =  1;                  // non-blocking reads
tty.c_cc[VTIME]  =  5;                  // 0.5 seconds read timeout
tty.c_cflag |= (CREAD | CLOCAL);     // turn on READ & ignore ctrl lines

/*make raw */
cfmakeraw(&tty);

/* flush device/port, then apply attributes */
tcflush( dev, TCIFLUSH );
if (tcsetattr(dev, TCSANOW, &tty) != 0) 
{
	fprintf(stderr, "error %d from tcsetattr", errno);
    return -1;

}

return 0;

}


void set_blocking (int fd, int should_block)
    {
            struct termios tty;
            memset (&tty, 0, sizeof tty);
            if (tcgetattr (fd, &tty) != 0)
            {
                    fprintf (stderr, "error %d from tggetattr", errno);
                    return;
            }

            tty.c_cc[VMIN]  = should_block ? 1 : 0;
            tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

            if (tcsetattr (fd, TCSANOW, &tty) != 0)
                    fprintf (stderr, "error %d setting term attributes", errno);
    }


int open_dev(const char* device, size_t baud, int blocking)
{  
  int i = 10;
  retry:
  device_fd = open (device, O_RDWR | O_NOCTTY | O_SYNC); // O_NONBLOCK);

  if (device_fd == -1)
  {
  	if(errno == EAGAIN)
  	{  	
  		perror(device);
  		if(i > 0)
  		{
  			i--;
  			goto retry;
  		}
  	}
    perror(device);
    return -1;

  }
 
  init_tty(device_fd, baud, 0);
  set_blocking (device_fd, blocking);
  printf("[open_dev]Device %s opened\n", device);

  return 0;
}

//send command to device
void write_at(ssize_t device, const unsigned char* data) 
{
    	write(device, data, strlen(data)); 
} 

//read response from device
int read_at(ssize_t device, unsigned char* buf, size_t bufsz) 
{		
	ssize_t nr;

	do_read:	
		nr = read(device, buf, bufsz);

		if(nr == -1)
		{
			if(errno == EINTR || errno == EAGAIN)
			{
				perror("retrying");
				goto do_read;
			}
			perror("read failed");
		}


	return nr;
}

//close device descriptor
void close_dev(size_t device)
{
	printf("\n[close_dev]Closing device %s\n", INTFC);
	close(device);
	printf("[close_dev]Closed\n");

}

//in case we use malloc, make it check the ret val
void *ermall(size_t size)
{
	void *ptr;
	ptr = malloc(size);
	if(ptr == NULL)
	  perror("[err] malloc failed\n");
	return ptr;
}
//clean up the output remove LF
void denl(char* ptr)
{
	char* nl = ptr;
	while(ptr != 0 && *ptr != '\0') 
	{
		if(*ptr == '\n')
			{
				ptr++;
				*nl = *ptr;
			}
		else
			*nl++ = *ptr++;	
	}
	*nl='\0';	
}



void writelog(FILE* fout, char* temp, size_t resp, unsigned char* buff)
{
	
    struct timeval tp;
    gettimeofday(&tp, 0);
    time_t curtime = tp.tv_sec;
    struct tm *t = localtime(&curtime);
    
    fflush(fout);
    fprintf(fout,"\n[%02d:%02d:%02d:%03ld] Sent command:%s",t->tm_hour, t->tm_min, t->tm_sec, tp.tv_usec/1000, temp); //line
    fprintf(fout, "\n[%02d:%02d:%02d:%03ld] Got %ld bytes response:%s", t->tm_hour, t->tm_min, t->tm_sec, tp.tv_usec/1000, resp, buff);
    fprintf(fout,"\n+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	
	
}

void communicate(char* temp, FILE* fout)
{    
	size_t resp = 0;
	char* buff = (char*)ermall(MAX_SIZE);
	memset(buff, 0, MAX_SIZE);	

	write_at(device_fd, temp);
	write_at(device_fd,"\r\n");
	usleep(1000*1000);
	resp = read_at(device_fd, buff, MAX_SIZE-1);
	denl(buff);

	printf("\n[com] Got %ld bytes response: %s", resp, buff);

	/* writing command and response to logfile */
	//if(fout != NULL)
	writelog(fout, temp, resp, buff);

	memset(temp, 0, MAX_SIZE);
  
	if(buff != NULL)
	{
		free(buff);
		//buff = NULL;
	}
}

//TODO: int is_dev_present ()

int main(int argc, char* argv[])
{	
    /* all the fuzz data is passed through temp  */
	static char* temp = NULL; /*holds fuzz data*/
	if(temp == NULL)
	{
		temp = (char*)malloc(MAX_SIZE);
		if(temp == NULL)
		{
			perror("malloc");
		}
	}
	//unsigned char buff[MAX_SIZE] = {0}; //holds response
	size_t resp = 0; //resp = strlen(buff)

	size_t i = 0;
	size_t loop = 0;

	FILE* fdrand; //fd for /dev/urandom
	char randm[22]; //for chars from /dev/urandom
	char randm2[22]; //for numbers gened by rand()
	ssize_t rnd = 0;

	

 	/* parameters for readline() - needed to parse input file line by line */
	char* line = NULL; //line of text in ats.txt
	size_t len = 0; //vars: len = strlen(line), resp = strlen(buff), isFreed - kinda boolean
	FILE* fin; //filestram for input data
	ssize_t err = -1; //check return value of readline() 
	

	size_t isFreed = 0;
	
   	
	if(open_dev(INTFC, B9600, 0) == -1) //open device, at baudrate 9600,  blocking mode = 0
	{   
		return -1;
	}

	//open ats.txt - file containing all possible valid AT commands without parameters
	fin = fopen(FUZZDAT, "r");
	if(fin == NULL)
		perror("[read]: IN_fopen");

   	//open output.log file for logging
	FILE* fout;
	fout = fopen(OUTLOG, "a+");
	if(fout == NULL)
		perror("[read]: IN_fopen");


	//open /dev/urandom for compatibility: gets random strings to pass as parameters
 	fdrand = fopen("/dev/urandom", "r");
	if(fdrand == NULL)
		perror("frand");
	while ((err = getline(&line, &len,fin)) != -1)
	{		
			if(errno == EINVAL || errno == ENOMEM)
			{
				
				perror("[read] getdelim");
				
				line = NULL;
				fclose(fin);
				fclose(fout);
				
			}
			denl(line);
			printf("\n[+] Writing command: %s", line);

			/*	TEST 0: iterating over generic fuzz data */
			fprintf(fout, "\n========== TEST 0 ===========\n");
			for(i=0; i<MAX_FZORC; ++i) //5 iterations - 5 symbols in fzorc
			{	
				fzorc[i].at=line;
				//if(temp == NULL)
				//{
				//	fprintf(fout, "ERROR: temp is NULL");
				//}
				strncpy(temp, fzorc[i].at, strlen(fzorc[i].at));
     			strncat(temp, fzorc[i].param, strlen(fzorc[i].param));
     			
     			communicate(temp, fout);
   
			    //usleep(1000);
			 }
				
			
			/* TEST 1:  introduced random data after the '=' sign */
	 		fprintf(fout, "\n========== TEST 1 ===========\n");
     		//for (int j =0; j<ITERS; ++j)//for (int j =0; j<MAX_FZORC; ++j)
     		//{

     			fread(randm, 1, 5, fdrand);
     			fzorc[0].at=line;
     			fzorc[0].data=randm;
     			strncpy(temp, fzorc[0].at, strlen(fzorc[0].at));
     			strncat(temp, fzorc[0].param, strlen(fzorc[0].param));
     			strncat(temp, fzorc[0].data, strlen(fzorc[0].data));
     			
        		communicate(temp, fout);

			//}/* TEST 2: 8 symbols after the '=' */
			fprintf(fout, "\n========== TEST 2 ===========\n");
			for (int n = 0; n<MAX_FZORC; ++n)
			{
     			fzorc[0].at=line;
     			fzorc[n].data=fzorc[n].param;
     			strncpy(temp, fzorc[0].at, strlen(fzorc[0].at));
     			strncat(temp, fzorc[n].param, strlen(fzorc[n].param));
     			for( int n1 = 0; n1 < 8; ++n1)
     				strncat(temp, fzorc[n].data, strlen(fzorc[n].data));
     			
     			communicate(temp, fout);

			}

        	
			
			
			for(loop = 0; loop < 100; ++loop)
	{		
			fprintf(fout, "\t\t\n/*Iteration %ld out of 10000*/\n", loop);
			/* TEST 3: random data after the '=' sign is now surrounded by quote marks*/
			fprintf(fout, "\n========== TEST 3 ===========\n");
			for (int k = 0; k<ITERS; ++k)
			{
				fread(randm, 1, 5, fdrand);
     			fzorc[0].at=line;
     			fzorc[0].data=randm;
     			strncpy(temp, fzorc[0].at, strlen(fzorc[0].at));
     			strncat(temp, fzorc[0].param, strlen(fzorc[0].param));
     			strncat(temp, fzorc[3].param, strlen(fzorc[3].param));
     			strncat(temp, fzorc[0].data, strlen(fzorc[0].data));
     			strncat(temp, fzorc[3].param, strlen(fzorc[3].param));
     			
     			communicate(temp, fout);

			}

			/* TEST 4:  random data after the '=' sign then ',' and some more random data*/
			fprintf(fout, "\n========== TEST 4 ===========\n");
			for (int m = 0; m<ITERS; ++m)
			{
				fread(randm, 1, 5, fdrand);
     			fzorc[0].at=line;
     			fzorc[0].data=randm;
     			strncpy(temp, fzorc[0].at, strlen(fzorc[0].at));
     			strncat(temp, fzorc[0].param, strlen(fzorc[0].param));
     			strncat(temp, fzorc[3].param, strlen(fzorc[3].param));
     			strncat(temp, fzorc[0].data, strlen(fzorc[0].data));
     			strncat(temp, fzorc[3].param, strlen(fzorc[3].param));
     			strncat(temp, fzorc[4].param, strlen(fzorc[4].param));
     			strncat(temp, fzorc[0].data, strlen(fzorc[0].data));
     			
     			communicate(temp, fout);

			}

			/* TEST 5: rand numbers in between *#num# symbols after the '=' */
			fprintf(fout, "\n========== TEST 5 ===========\n");
				for (int p =0; p<ITERS; ++p)
     		{
     			 
     			int digits = (rand()%100000000);
     			snprintf(randm2, 20, "%d", digits);
     			//fread(rand, 1, 5, fdrand);
     			fzorc[0].at=line;
     			fzorc[0].data=randm2;
     			strncpy(temp, fzorc[0].at, strlen(fzorc[0].at));
     			strncat(temp, fzorc[0].param, strlen(fzorc[0].param));
     			strncat(temp, fzorc[5].param, strlen(fzorc[5].param));
     			strncat(temp, fzorc[0].data, strlen(fzorc[0].data));
     			strncat(temp, fzorc[6].param, strlen(fzorc[6].param));
     			
     			
        		communicate(temp, fout);

			}

			/* TEST 5.1: rand numbers in between *#num# symbols after the '=' */
				fprintf(fout, "\n========== TEST 5.1 ===========\n");
				for (int p =0; p<ITERS; ++p)
     		{
     			 
     			int digits = (rand()%1000);
     			snprintf(randm2, 20, "%d", digits);
     			//fread(rand, 1, 5, fdrand);
     			fzorc[0].at=line;
     			fzorc[0].data=randm2;
     			strncpy(temp, fzorc[0].at, strlen(fzorc[0].at));
     			strncat(temp, fzorc[0].param, strlen(fzorc[0].param));
     			strncat(temp, fzorc[5].param, strlen(fzorc[5].param));
     			strncat(temp, fzorc[0].data, strlen(fzorc[0].data));
     			strncat(temp, fzorc[6].param, strlen(fzorc[6].param));
     			
     			
        		communicate(temp, fout);

			}
				/* TEST 6: rand numbers in between *#num*num# symbols after the '=' */
				fprintf(fout, "\n========== TEST 6 ===========\n");
				for (int p =0; p<ITERS; ++p)
     		{
     			 
     			int digits = (rand()%1000);
     			snprintf(randm2, 20, "%d", digits);
     			//fread(rand, 1, 5, fdrand);
     			fzorc[0].at=line;
     			fzorc[0].data=randm2;
     			strncpy(temp, fzorc[0].at, strlen(fzorc[0].at));
     			strncat(temp, fzorc[0].param, strlen(fzorc[0].param));
     			strncat(temp, fzorc[5].param, strlen(fzorc[5].param));
     			strncat(temp, fzorc[0].data, strlen(fzorc[0].data));
     			strncat(temp, fzorc[7].param, strlen(fzorc[7].param));
     			strncat(temp, fzorc[0].data, strlen(fzorc[0].data));
     			
     			
        		communicate(temp, fout);

			}

			/* TEST 7: rand numbers in between num,'*#num#',num symbols after the '=' */
				fprintf(fout, "\n========== TEST 7 ===========\n");
				for (int p =0; p<ITERS; ++p)
     		{
     			 
     			int digits = (rand()%1000);
     			snprintf(randm2, 20, "%d", digits);
     			//fread(rand, 1, 5, fdrand);
     			fzorc[0].at=line;
     			fzorc[0].data=randm2;
     			strncpy(temp, fzorc[0].at, strlen(fzorc[0].at)); //AT
     			strncat(temp, fzorc[0].param, strlen(fzorc[0].param));
     			strncat(temp, fzorc[0].data, strlen(fzorc[0].data)); // num
     			strncat(temp, fzorc[4].param, strlen(fzorc[4].param)); //,
     			strncat(temp, fzorc[3].param, strlen(fzorc[3].param)); //"
     			strncat(temp, fzorc[5].param, strlen(fzorc[5].param));//*#
     			strncat(temp, fzorc[0].data, strlen(fzorc[0].data)); //num
     			strncat(temp, fzorc[6].param, strlen(fzorc[6].param)); //#
     			strncat(temp, fzorc[3].param, strlen(fzorc[3].param)); //"
     			strncat(temp, fzorc[4].param, strlen(fzorc[4].param)); //,
     			strncat(temp, fzorc[0].data, strlen(fzorc[0].data)); //num
     			
     			
        		communicate(temp, fout);

			}

			/* TEST 7.1: rand numbers in between num,'*#num#',num symbols after the '='  long seq of numbers*/
				fprintf(fout, "\n========== TEST 7.1 ===========\n");
				for (int p =0; p<ITERS; ++p)
     		{
     			 
     			int digits = (rand()%100000000);
     			snprintf(randm2, 20, "%d", digits);
     			//fread(rand, 1, 5, fdrand);
     			fzorc[0].at=line;
     			fzorc[0].data=randm2;
     			strncpy(temp, fzorc[0].at, strlen(fzorc[0].at)); //AT
     			strncat(temp, fzorc[0].param, strlen(fzorc[0].param));
     			strncat(temp, fzorc[0].data, strlen(fzorc[0].data)); // num
     			strncat(temp, fzorc[4].param, strlen(fzorc[4].param)); //,
     			strncat(temp, fzorc[3].param, strlen(fzorc[3].param)); //"
     			strncat(temp, fzorc[5].param, strlen(fzorc[5].param));//*#

     			snprintf(randm2, 20, "%d", digits);
     			fzorc[0].data=randm2;
     			strncat(temp, fzorc[0].data, strlen(fzorc[0].data)); //num
     			strncat(temp, fzorc[6].param, strlen(fzorc[6].param)); //#
     			strncat(temp, fzorc[3].param, strlen(fzorc[3].param)); //"
     			strncat(temp, fzorc[4].param, strlen(fzorc[4].param)); //,

     			snprintf(randm2, 20, "%d", digits);
     			fzorc[0].data=randm2;
     			strncat(temp, fzorc[0].data, strlen(fzorc[0].data)); //num
     			
     			
        		communicate(temp, fout);

			}

			

			 /*	TEST 8: ATD GPRS commands 
			  https://www.arcelect.com/GSM%20Developer%20Guide%20-%20GSM%20AT%20Commands%20-%20Rev%20%20A.pdf#%5B%7B%22num%22%3A553%2C%22gen%22%3A0%7D%2C%7B%22name%22%3A%22XYZ%22%7D%2C98%2C721%2Cnull%5D*/
			int s =0;
			fprintf(fout, "\n========== TEST 8 ===========\n");
			for(s=0; s<ITERS; ++s) 
			{	int digits = (rand()%1000);
     			snprintf(randm2, 20, "%d", digits);


				fzorc[i].at="ATD*99***";
				fzorc[i].data=randm2;
				strncpy(temp, fzorc[0].at, strlen(fzorc[0].at));
     			strncat(temp, fzorc[0].data, strlen(fzorc[0].data));
     			strncat(temp, fzorc[6].param, strlen(fzorc[6].param)); //#
     			
     			communicate(temp, fout);
   
			    //usleep(1000*1000);
			 }
			 /* TEST 9: Same as TEST 7.1, with ";" at the end*/
			fprintf(fout, "\n========== TEST 9 ===========\n");
			int s1=0;
			for(s1=0; s1<ITERS; ++s1) 
			{	int digits = (rand()%1000);
     			snprintf(randm2, 20, "%d", digits);

				int digits2 = (rand()%100000000);
     			snprintf(randm2, 20, "%d", digits);
     		
     			fzorc[0].at=line;
     			fzorc[0].data=randm2;
     			strncpy(temp, fzorc[0].at, strlen(fzorc[0].at)); //AT
     			strncat(temp, fzorc[0].param, strlen(fzorc[0].param));
     			strncat(temp, fzorc[0].data, strlen(fzorc[0].data)); // num
     			strncat(temp, fzorc[4].param, strlen(fzorc[4].param)); //,
     			strncat(temp, fzorc[3].param, strlen(fzorc[3].param)); //"
     			strncat(temp, fzorc[5].param, strlen(fzorc[5].param));//*#

     			snprintf(randm2, 20, "%d", digits);
     			fzorc[0].data=randm2;
     			strncat(temp, fzorc[0].data, strlen(fzorc[0].data)); //num
     			strncat(temp, fzorc[6].param, strlen(fzorc[6].param)); //#
     			strncat(temp, fzorc[3].param, strlen(fzorc[3].param)); //"
     			strncat(temp, fzorc[4].param, strlen(fzorc[4].param)); //,

     			snprintf(randm2, 20, "%d", digits);
     			fzorc[0].data=randm2;
     			strncat(temp, fzorc[0].data, strlen(fzorc[0].data)); //num
     			strncat(temp, fzorc[8].data, strlen(fzorc[8].data)); //;

     			    			
     			communicate(temp, fout);
   
			    //usleep(1000*1000);
			 }



		}

	
	}

	
	if(!isFreed)
	{
	free(temp);
	}
	line = NULL;
	fclose(fin);
	fclose(fout);
	
	close_dev(device_fd);


}
