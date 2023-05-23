#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

//está com erros

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS0"
#define _POSIX_SOURCE 1 
#define FALSE 0
#define TRUE 1

void alarmPickup();
void sendSET(int fd);
void readUA(int fd);
void sendUA(int fd);
void readUA(int fd);
void sendRR(int fd);
void readRR(int fd);
void sendDISC(int fd);
void readDISC(int fd);

volatile int STOP = FALSE;
volatile int switchwrite_C_RCV = 0;
volatile int switchread_C_RCV = 0;
volatile int switchRR = 0;
volatile int switchreadRR = 0;

struct termios oldtio, newtio;

typedef enum{
    stateStart,
    stateFlagRCV,
    stateARCV,
    stateCRCV,
    stateBCCOK,
    stateOther,
    stateStop
} frameStates;

frameStates frameState = stateStart;

int fd, resendSize, alarmMax, alarmTime, countador_alarme = 0;
char resendStr[255] = {0}, SET[5], UA[5], DISC[5]; 

typedef struct linkLayer{
    char serialPort[50];
    int role; 
    int baudRate;
    int numTries;
    int timeOut;
}linkLayer;

void alarmPickup(){ 
    int res, i;
    printf("Retrying connection in %d seconds...\n", alarmTime);
   
    res = write(fd,resendStr,resendSize);
    printf("%d bytes written\n", res);
    countador_alarme++;
    if (countador_alarme == alarmMax){
        printf("## WARNING: Reached max (%d) retries. ## \nExiting...\n", alarmMax);
        exit(1);
    }
    
    alarm(alarmTime);
}

void sendSET(int fd){
    int resSET;
    printf("--- Sending SET ---\n");
    SET[0] = FLAG_RCV;    
    SET[1] = A_RCV;       
    SET[2] = C_SET;       
    SET[3] = C_SET^A_RCV; 
    SET[4] = FLAG_RCV;    
    resSET = write(fd,SET, 5);
    printf("%d bytes written\n", resSET);
}

void sendUA(int fd){    
    int resUA;
    printf("--- Sending UA ---\n");
    UA[0] = FLAG_RCV;    
    UA[1] = A_RCV;       
    UA[2] = C_UA;        
    UA[3] = C_UA^A_RCV;  
    UA[4] = FLAG_RCV;    
    resUA = write(fd,UA,5);
    printf("%d bytes written\n", resUA);
}

void readSET(int fd){
    int res, SMFlag = 0;
    char buf[255];
    printf("--- Reading SET ---\n");

    while (STOP == FALSE) {       
        res = read(fd,buf,1);     

        switch(frameState){
            case stateStart:
                if (buf[0] == FLAG_RCV)
                    frameState = stateFlagRCV;
                break;

            case stateFlagRCV:
                if (buf[0] == FLAG_RCV){
                    frameState = stateFlagRCV;
                    SMFlag = 0;
                }
                else if (buf[0] == A_RCV || buf[0] == ALT_A_RCV)
                    frameState = stateARCV;
                
                else{
                    frameState = stateStart;       
                    SMFlag = 0;
                }
                break;

            case stateARCV:
                if (buf[0] == FLAG_RCV)
                    frameState = stateFlagRCV;
                
                else if (buf[0] == C_SET)
                    frameState = stateCRCV;
                
                else{
                    frameState = stateStart; 
                    SMFlag = 0;
                }
                break;  

            case stateCRCV:
                if (buf[0] == FLAG_RCV){
                    frameState = stateFlagRCV;
                    SMFlag = 0;
                }
                else if (buf[0] == A_RCV^C_SET || buf[0] == ALT_A_RCV^C_SET)
                    frameState = stateBCCOK;
                        
                else{
                    frameState = stateStart;
                    SMFlag = 0;
                }
                break;
            
            case stateBCCOK:
                frameState = stateStart;
                break;
        } 
        if (buf[0] == FLAG_RCV && SMFlag == 1)
            STOP = TRUE;
        if(buf[0] == FLAG_RCV)
            SMFlag = 1;
    }
    STOP = FALSE;
}

void readUA(int fd){
    int res, SMFlag = 0;
    char buf[255];
    printf("--- Reading UA ---\n");
    
    while (STOP == FALSE) {          
        res = read(fd,buf,1);            

        switch(frameState){

            case stateStart:
                if (buf[0] == FLAG_RCV)
                    frameState = stateFlagRCV;
                
                break;

            case stateFlagRCV:
                if (buf[0] == FLAG_RCV){
                    frameState = stateFlagRCV;
                    SMFlag = 0;
                }
                else if (buf[0] == A_RCV || buf[0] == ALT_A_RCV)
                    frameState = stateARCV;
                
                else{
                    frameState = stateStart; 
                    SMFlag = 0;
                }
                break;

            case stateARCV:
                if (buf[0] == FLAG_RCV)
                    frameState = stateFlagRCV;
                
                else if (buf[0] == C_UA)
                    frameState = stateCRCV;
                
                else{
                    frameState = stateStart; 
                    SMFlag = 0;
                }
                break;  

            case stateCRCV:
                if (buf[0] == FLAG_RCV){
                    frameState = stateFlagRCV;
                    SMFlag = 0;
                }
                else if (buf[0] == A_RCV^C_UA || buf[0] == ALT_A_RCV^C_UA){
                    frameState = stateBCCOK;
                    STOP = TRUE;
                }
                else{
                    frameState = stateStart;
                    SMFlag = 0;
                }
                break;
            
            case stateBCCOK:
                frameState = stateStart;
                break;          
        } 

        if (buf[0] == FLAG_RCV && SMFlag == 1)
            STOP = TRUE;
        
        if(buf[0] == FLAG_RCV)
            SMFlag = 1;
    } 
    STOP = FALSE;
}

void sendRR(int fd){
    char RR[5];
    int resRR, i;
    printf("--- Sending RR ---\n");
    RR[0] = FLAG_RCV;       
    RR[1] = A_RCV;          
    if (switchRR == 0)      
        RR[2] = C_RR_1;       
    else
        RR[2] = C_RR_0;
    RR[3] = RR[2]^A_RCV;  
    RR[4] = FLAG_RCV;      
    resRR = write(fd,RR,5);

    switchRR = !switchRR;
    printf("\n%d bytes written\n", resRR);
}

void readRR(int fd){
    int res, SMFlag = 0;
    char C_RCV, buf[255];
    printf("--- Reading RR ---\n");
    
    if(switchreadRR == 0)
        C_RCV = C_RR_1;
    else
        C_RCV = C_RR_0;

    while (STOP == FALSE) {       
        res = read(fd,buf,1);        
        switch(frameState){

            case stateStart:
                if (buf[0] == FLAG_RCV)
                    frameState = stateFlagRCV;
                break;

            case stateFlagRCV:
                if (buf[0] == FLAG_RCV){
                    frameState = stateFlagRCV;
                    SMFlag = 0;
                }
                else if (buf[0] == A_RCV || buf[0] == ALT_A_RCV)
                    frameState = stateARCV;
                
                else{
                    frameState = stateStart;          
                    SMFlag = 0;
                }
                break;

            case stateARCV:
                if (buf[0] == FLAG_RCV)
                    frameState = stateFlagRCV;
                
                else if (buf[0] == C_RCV)
                    frameState = stateCRCV;
                
                else{
                    frameState = stateStart; 
                    SMFlag = 0;
                }
                break;  

            case stateCRCV:
                if (buf[0] == FLAG_RCV){
                    frameState = stateFlagRCV;
                    SMFlag = 0;
                }
                else if (buf[0] == A_RCV^C_RCV || buf[0] == ALT_A_RCV^C_RCV){
                    frameState = stateBCCOK;
                    STOP = TRUE;
                }
                else{
                    frameState = stateStart;
                    SMFlag = 0;
                }
                break;
            
            case stateBCCOK:
                frameState = stateStart;
                break;
        } 
        
        if (buf[0] == FLAG_RCV && SMFlag == 1)
            STOP = TRUE;
        
        if(buf[0] == FLAG_RCV)
            SMFlag = 1; 
    } 
    STOP = FALSE;
    switchreadRR = !switchreadRR;
}

void sendDISC(int fd){
    int resDISC;
    printf("--- Sending DISC ---\n");
    DISC[0] = FLAG_RCV;     
    DISC[1] = A_RCV;        
    DISC[2] = C_DISC;       
    DISC[3] = C_DISC^A_RCV; 
    DISC[4] = FLAG_RCV;     
    resDISC = write(fd,DISC,5);
    printf("%d bytes written\n", resDISC);
}

void readDISC(int fd){
    int res, SMFlag = 0;
    char buf[255];
    printf("--- Reading DISC ---\n");

    while (STOP == FALSE) {       
        res = read(fd,buf,1);        

        switch(frameState){

            case stateStart:
                if (buf[0] == FLAG_RCV)
                    frameState = stateFlagRCV;
                break;

            case stateFlagRCV:
                if (buf[0] == FLAG_RCV){
                    frameState = stateFlagRCV;
                    SMFlag = 0;
                }
                else if (buf[0] == A_RCV || buf[0] == ALT_A_RCV)
                    frameState = stateARCV;
                
                else{
                    frameState = stateStart; 
                    SMFlag = 0;
                }
                break;

            case stateARCV:
                if (buf[0] == FLAG_RCV)
                    frameState = stateFlagRCV;
                
                else if (buf[0] == C_DISC)
                    frameState = stateCRCV;
                
                else{
                    frameState = stateStart; 
                    SMFlag = 0;
                }
                break;  

            case stateCRCV:
                if (buf[0] == FLAG_RCV){
                    frameState = stateFlagRCV;
                    SMFlag = 0;
                }
                else if (buf[0] == A_RCV^C_DISC || buf[0] == ALT_A_RCV^C_DISC){
                    frameState = stateBCCOK;
                    STOP = TRUE;
                }
                else{
                    frameState = stateStart;
                    SMFlag = 0;
                }
                break;
            
            case stateBCCOK:
                frameState = stateStart;                
                break;
        } 
        
        if (buf[0] == FLAG_RCV && SMFlag == 1)
            STOP = TRUE;
        
        if(buf[0] == FLAG_RCV)
            SMFlag = 1;
    } 
    STOP = FALSE;
}

int llopen(linkLayer connectionParameters){
    int i = 0;

    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY );     
    if (fd < 0) { perror(connectionParameters.serialPort); exit(-1); }
    if (tcgetattr(fd,&oldtio) == -1) { 
        perror("tcgetattr");
        exit(-1);
    }

    
    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 0;   
    newtio.c_cc[VMIN]     = 1;   




    tcflush(fd, TCIOFLUSH);     

    sleep(1);
    if (tcsetattr(fd,TCSANOW,&newtio) == -1) {    
        perror("tcsetattr");
        exit(-1);
    }

    printf("--- New termios structure set ---\n");

    if (connectionParameters.role == 0){
        (void) signal(SIGALRM, alarmPickup); 
        sendSET(fd);     
        for(i = 0; i < 5; i++){
            resendStr[i] = SET[i];
        }
        resendSize = 5;

        alarmMax = connectionParameters.numTries;
        alarmTime = connectionParameters.timeOut;

        alarm(alarmTime);     
        printf("--- UA State Machine has started ---\n"); 
        readUA(fd);   
        alarm(0);     
        printf("--- UA READ OK ! ---\n");
        countador_alarme = 0;
    
        sleep(1);
        return 0;
    }

    else if(connectionParameters.role == 1){
        printf("--- SET State Machine has started ---\n"); 

        readSET(fd);
        printf("--- SET READ OK ! ---\n");
        sendUA(fd);

        sleep(1);

        return 0;
    }
    return -1;
}

int llwrite(char* buf, int bufSize){
    int i, j, resData, auxSize = 0;
    char xor = buf[0], auxBuf[2000];
    for(i = 1; i < bufSize; i++)
        xor = xor^buf[i];

  
    for(i = 0; i < bufSize; i++)
        auxBuf[i] = buf[i];
    
    auxSize = bufSize;

    for(i = 0; i < auxSize; i++){
        if(auxBuf[i] == 0x5d){ 
            for(j = auxSize+1; j > i+1; j--)
                auxBuf[j] = auxBuf[j-1];
            auxBuf[i+1] = 0x7d;
            auxSize++;
        }
    }

    for(i = 1; i < auxSize; i++){
        if(auxBuf[i] == 0x5c){ 
            auxBuf[i] = 0x5d;
            for(j = auxSize+1; j > i+1; j--)            
                auxBuf[j] = auxBuf[j-1];
            auxBuf[i+1] = 0x7c;
            auxSize++;
        }
    }

    char str[auxSize+6];

    str[0] = FLAG_RCV;          
    str[1] = A_RCV;             
    if (switchwrite_C_RCV == 0) 
        str[2] = C_NS_0;       
    else
        str[2] = C_NS_1;
    switchwrite_C_RCV = !switchwrite_C_RCV;
    str[3] = str[2]^A_RCV;      

    for(j = 0; j < auxSize; j++)
        str[j+4] = auxBuf[j];
    
    if(xor == 0x5c){
        auxSize++;
        str[auxSize+4] = 0x5d;
        str[auxSize+5] = 0x7c;
        str[auxSize+6] = FLAG_RCV;

    }
    else{
        str[auxSize+4] = xor;
        str[auxSize+5] = FLAG_RCV;
    }

    for(j = 0; j < auxSize+6; j++)
        resendStr[j] = str[j];
    resendSize = auxSize+6;

    resData = write(fd, str, auxSize+6);
    printf("%d bytes written\n", resData);

    alarm(alarmTime); 
    readRR(fd);
    printf("--- RR READ OK ! ---\n");
    alarm(0);
    countador_alarme = 0;
    printf("--- RR Checked ---\n");
    return 1;
}

int llread(char* packet){
    int i = 0, j, res, xor, bytes_read, SMFlag = 0, destuffFlag = 0, skip = 0;
    char aux, aux2, C_RCV, buf[1], str[1050];
    frameState = stateStart;
    if(switchread_C_RCV == 0)
        C_RCV = C_NS_0;
    else
        C_RCV = C_NS_1;

    while (STOP == FALSE){       
        res = read(fd,buf,1);   
        
        switch(frameState){

            case stateStart:
                if (buf[0] == FLAG_RCV)
                    frameState = stateFlagRCV;
                break;

            case stateFlagRCV:
                if (buf[0] == FLAG_RCV){
                    frameState = stateFlagRCV;
                    SMFlag = 0;
                }
                else if (buf[0] == A_RCV || buf[0] == ALT_A_RCV)
                    frameState = stateARCV;
                
                else{
                    frameState = stateStart; 
                    SMFlag = 0;
                }
                break;

            case stateARCV:
                if (buf[0] == FLAG_RCV)
                    frameState = stateFlagRCV;
                
                else if (buf[0] == C_RCV)
                    frameState = stateCRCV;
                
                else{
                    frameState = stateStart; 
                    SMFlag = 0;
                }
                break;  

            case stateCRCV:
                if (buf[0] == FLAG_RCV){
                    frameState = stateFlagRCV;
                    SMFlag = 0;
                }
                else if (buf[0] == A_RCV^C_RCV || buf[0] == ALT_A_RCV^C_RCV)
                    frameState = stateBCCOK;
                                
                else{
                    frameState = stateStart;
                    SMFlag = 0;
                }
                break;
            
            case stateBCCOK:
                frameState = stateStart;
                break;
        } 

         
        if (buf[0] == 0x5d)
            destuffFlag = 1;

        if (destuffFlag && buf[0] == 0x7c){
            str[i-1] = 0x5c;
            skip = 1;
        } 
        else if (destuffFlag && buf[0] == 0x7d){
            skip = 1;
        } 
         
        if (!skip){
            str[i] = buf[0];
            if (i > 0)
                aux = str[i-1];
            i++;
        }
        else{
            skip = 0;
            destuffFlag = 0;
        }

        if (buf[0] == FLAG_RCV && SMFlag && i > 0){
                xor = str[4];
                for(j = 5; j < i - 2; j++)
                    xor = xor^str[j];
                if(aux == xor){
                    STOP = TRUE;
                    printf("\n --- FRAME READ OK! ---\n");
                }
                else{
                    printf("XOR VALUE IS: 0x%02x\nShould be: 0x%02x\n", (unsigned int)(xor & 0xff), (unsigned int)(aux & 0xff));
                    printf("\n --- BCC2 FAILED!---\n");
                    return -1;
                }
            
        }

        if(buf[0] == FLAG_RCV)
            SMFlag = 1;
    }
    STOP = FALSE;
    switchread_C_RCV = !switchread_C_RCV;

    for(j = 4; j < i-2; j++)
        packet[j-4] = str[j];

    bytes_read = i-6;

    sendRR(fd);

    return bytes_read;
}

int llclose(linkLayer connectionParameters, int showStatistics){ 
    int i;
    printf("\n\n --- CLOSING CONNECTION ---\n\n");

    if(connectionParameters.role == 0){ //tx
        sendDISC(fd);
        for(i = 0; i < 5; i++)
            resendStr[i] = DISC[i];
        alarm(alarmTime);
        readDISC(fd);
        printf("--- DISC READ OK ! ---\n");
        alarm(0);
        alarmCounter = 0;
        printf("\n\n --- READY FOR CLOSURE ---\n\n");
        sendUA(fd);
        sleep(1);
        if (tcsetattr(fd,TCSANOW,&oldtio) == -1){
            perror("tcsetattr");
            exit(-1);
        }
        
        close(fd);
        return 0;
    }

    if(connectionParameters.role == 1){ //rx    
        sleep(1); 

        readDISC(fd);
        printf("--- DISC READ OK ! ---\n");

        sendDISC(fd);
        for(i = 0; i < 5; i++)
            resendStr[i] = DISC[i];
        alarm(alarmTime);
        readUA(fd);
        alarm(0);
        alarmCounter = 0;

        printf("--- UA READ OK ! ---\n");

        ("\n\n --- READY FOR CLOSURE ---\n\n");
        if (tcsetattr(fd,TCSANOW,&oldtio) == -1){
            perror("tcsetattr");
            exit(-1);
        }
        
        close(fd);
        return 0;
    }

    return -1;
}