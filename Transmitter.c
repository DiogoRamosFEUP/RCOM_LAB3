#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include "utils.h"

int RR_RECEIVED = FALSE;
int REJ_RECEIVED = FALSE;

FileData file;

unsigned char C1 = 0x40; //ver se é este
int fsize;
FILE *fp;
int flag=0, conta=1;

void stoppingll()
{
	conta = 0;
	STOP = FALSE;
	flag = 0;
}

void resetRRRejFlags() //Func para dar reset as flags RR e REJ flags
{
	//adiantei isto para ver se nao me esquecia
	RR_RECEIVED = FALSE;
	REJ_RECEIVED = FALSE;
}

int sendReadDISC(int fd,int toRead) //Func para ler trama
{
	if (toRead == TRUE)
	{
		unsigned char C = 0x0B;
		int k = readSupervisao(C,fd);
		return k;
	}

	unsigned char disc[5];
	unsigned char A=0x03;
	unsigned char C = 0x0B;
	unsigned char BCC1 = A^C;
	disc[0] = FLAG;
	disc[1] = A;
	disc[2] = C;
	disc[3] = BCC1;
	disc[4] = FLAG;

	write(fd,disc,5);
	return 0;
}

int escreverBytes(int fd) // Func para escrever os bytes do código
{
	char ua[5] = {0x7E,0x03,0x07,0x03^0x07,0x7E}; //possiveis códigos
	int res;

	res = write(fd,ua,5);
	printArray(ua,5);
	printf("escreverBytes: %d bytes escritos\n", res);
	return res;
}

void escreverSet(int fd) //Func para escrever o SET
{
	int res;
	char set[5] = {0x7E,0x03,0x03,0x00,0x7E}; //SET possível
	res=write(fd,set,5);
	printf("escreverSet: %d bytes escritos\n", res);
}

void atende() // Func para fazer um timer para o transmissor
{
	int flag=1;
    printf("alarme # %d\n", conta);
    flag=1;
    conta++;
}

int readSupervisao(unsigned char vetor, int fd) //Func para ler codigo da supervisão
{
	char buf[1], codigo[5] = {0x5C, 0x01, vetor, 0x01^vetor, 0x5C};  //combinações possiveis para as flags de combinações
	int contador = 0;
	int error = 0; 

	while (STOP = 0 && contador < 5)  //ler o codigo ate ao fim
	{
		read(fd,buf,1); //ler buffer 1 a 1

		switch(contador) //cirar maquina de estado para ver se as flags são as que nós queremos
		{
			case 0:			//comparar primeiro digito da flag de supervisao correta
			if(buf[0] != codigo[0]) error= -1;
			break;

			case 1:			//comparar segundo digito da flag de supervisao correta
			if(buf[0] != codigo[1]) error=-1;
			break;

			case 2:			//comparar terceiro digito da flag de supervisao correta
			if(buf[0] != codigo[2]) error=-1;
			break;

			case 3:			//comparar quarto digito da flag de supervisao correta
			if(buf[0] != codigo[3]) error=-1;
			break;

			case 4:			//comparar quinto digito da flag de supervisao correta
			if(buf[0] != codigo[4]) error=true;
			break;
		}
		contador++;
		if (contador==5 && error == 0)
		{
			STOP=TRUE;
			return 0;
		}
	}
	return -1;
}

int compareSupervisao(int fd) //Func para comparar o codigo do ficheiro com o que queremos
{
	unsigned char vetor = 0x5C;
	int k = readSupervisao(vetor,fd);
	return k;
}

unsigned char *buildStartPacket(int fd) //Func para construir o pacote inicial
{
	int i=0, j=0;

	fseek(fp,0,SEEK_END);
	fsize = ftell(fp);
	fseek(fp,0,SEEK_SET);

	//criar os blocos da trama 1
	unsigned char A = 0x03;
	unsigned char C = 0x00;
	unsigned char BCC1 = A^C;
	unsigned char BCC2;

	int startBufSize = 9+file.fileSize;

	unsigned char *startBuf = (unsigned char *)malloc(startBufSize); //criar um vetor dinamico para a string

	//pedaços da string
	startBuf[0] = 0x02;	
	startBuf[1] = 0x00;
	startBuf[2] = 0x04;
	int* intloc= (int*)(&startBuf[3]);
	*intloc=fsize;
	startBuf[7] = 0x01;
	startBuf[8] = file.fileSize;

	//insere o file name no array
	for(i = 0; i < file.fileSize;i++)
	{
		startBuf[i+9] = file.arr[i];
	}

	//calcular o tamanho final do array
	int sizeFinal = startBufSize+6;
	for (i = 0; i < startBufSize;i++)
	{
		if (startBuf[i] == 0x7E || startBuf[i] == 0x7D)
			sizeFinal++;
	}

	//calcular BCC2
	BCC2 = startBuf[0]^startBuf[1];
	for (i = 2; i < startBufSize;i++)
	{
		BCC2 = BCC2^startBuf[i];
	}

	unsigned char *dataPackage;
	if (BCC2 == 0x7E)
	{
		dataPackage = (unsigned char *)malloc(sizeFinal+1);
		dataPackage[sizeFinal-3] = 0x7D;
		dataPackage[sizeFinal-2] = 0x5E;
	}

	if (BCC2 == 0x7D)
	{
		dataPackage = (unsigned char *)malloc(sizeFinal+1);
		dataPackage[sizeFinal-3] = 0x7D;
		dataPackage[sizeFinal-2] = 0x5D;
	}

	if (BCC2 != 0x7D && BCC2 != 0x7E)
	{
		 dataPackage = (unsigned char *)malloc(sizeFinal);
		 dataPackage[sizeFinal-2] = BCC2;
	}

	dataPackage[0] = FLAG;
	dataPackage[1] = A;
	dataPackage[2] = 0x00;
	BCC1 = A^dataPackage[2];
	dataPackage[3] = BCC1;

	j = 5;
	int k=4;
	for (i = 0; i < startBufSize;i++)
	{
		if (startBuf[i] == 0x7E)
		{
			dataPackage[k] = 0x7D;
			dataPackage[j] = 0x5E;
			k++;
			j++;
		}

		if (startBuf[i] == 0x7D)
		{
			dataPackage[k] = 0x7D;
			dataPackage[j] = 0x5D;
			k++;
			j++;
		}

		if(startBuf[i] != 0x7D && startBuf[i] != 0x7E)
			dataPackage[k] = startBuf[i];

		j++;
		k++;
	}
	dataPackage[sizeFinal-1] = FLAG;

	int res;
	res = write(fd, dataPackage, sizeFinal);

	printf("%d bytes escritos\n",res);
	return 0;
}

int llopen(int fd) //Func para abrir o ll no transmissor
{
	while(conta < 4) //ler codigo
	{
		writeSet(fd);
		alarm(3);

		while(!flag && STOP == FALSE) // quando abrir ficheiro, verificar codigo
		{
			compareSupervisao(fd);
		}
		if(STOP==TRUE)  //quando parar, ativar alarm
		{
			alarm(0);
			stoppingLL();
			return 0;
		}
		else
			flag=0;
	}
	return -1;
}

int llwrite(int fd) //Func para escrever o ll no transmissor
{
	int res;
	buildStartPacket(fd);
	res = getDataPacket(fd);
	stoppingLL();
	return res;
}

int llclose(int fd) //Func para fechar o ll no transmissor
{
	int receivedDISC = FALSE;
	int res = 0;
	while(conta < 4)
	{
		sendReadDISC(fd,FALSE);
		printf("DISCONNECT .\n");
		alarm(3);

		while(!flag && STOP == FALSE)
		{
			res = sendReadDISC(fd,TRUE);
			
			if (res == 0){
				printf("DISCONNECT Recebido.\n");				
				STOP = TRUE;
				receivedDISC=TRUE;
			}
		}

		if(STOP==TRUE)
		{
			alarm(0);
			stoppingLL();
			break;;
		}
		else
			flag=0;
	}
	if(receivedDISC==FALSE){ //Failsafe da não receção do DISCONNECT
		printf("Não recebido DISCONNECT do Receiver\n");
		return -1;	
	
	}
	res = escreverBytes(fd); //escrever os bytes
	if (res == 5)
	{
		printf("UA enviado...\n");
		printf("------FIM------\n");
	}

	return 0;
}
