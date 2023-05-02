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
	RR_RECEIVED = FALSE;
	REJ_RECEIVED = FALSE;
}

int sendReadDISC(int fd,int toRead) //ler trama
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

int sendInfoFile(int fd, unsigned char *buf, int size) //Func para enviar porçoes do ficheiro para o receiver
{
	int newSize = (size+6),i,j,res,k; // alocação do novo tamanho do ficheiro
	unsigned char BCC2,BCC1; // blocos das tramas

	for (i = 0; i < size; i++)
	{
		if (buf[i] == 0x7E || buf[i] == 0x7D)
		{
			newSize++;
		}
	}

	
	BCC2 = buf[0] ^ buf[1]; //calcular BCC2
	for (i = 2; i < size;i++)
	{
		BCC2 = BCC2^buf[i];
	}

	unsigned char *dataPacket = (unsigned char*)malloc(newSize); //alocar memória para o pacote de dados

	dataPacket[0] = FLAG;
	dataPacket[1] = 0x03;
	dataPacket[2] = C1;
	BCC1 = dataPacket[1]^C1;
	dataPacket[3] = BCC1;

	j = 5;
	k = 4;
	for (i = 0; i < size;i++)
	{
		if (buf[i] == 0x7E)
		{
			dataPacket[k] = 0x7D;
			dataPacket[j] = 0x5E;
			k++;
			j++;
		}

		if (buf[i] == 0x7D)
		{
			dataPacket[k] = 0x7D;
			dataPacket[j] = 0x5D;
			k++;
			j++;
		}

		if(buf[i] != 0x7D && buf[i] != 0x7E)
			dataPacket[k] = buf[i];

		j++;
		k++;
	}

	//indicação do que cada pacote de data é
	dataPacket[newSize-2] = BCC2;
	dataPacket[newSize-1] = FLAG;

	res = write(fd,dataPacket,newSize);
		printf("Quinto byte do datapacket 0x%02x\n",dataPacket[4]);
	if (res == 0 || res == -1)
	{
		printf("sendInfoFile() - %d bytes escritos.\n",res);
		return res;
	}
	return res;
}

int getDataPacket(int fd) //Func para dividir porções do ficheiro em porções equivalentes a bytes do PACKET_SIZE 
{
	int byteslidos = 0,res,lidos = 0;

	unsigned char *dataPacket = (unsigned char *)malloc(PACKET_SIZE); //alocar memoria para o pacote de dados

	while ((byteslidos = fread(dataPacket, sizeof(unsigned char), PACKET_SIZE, fp)) > 0) //ciclo para ler a string bit a bit
	{
		while (conta < 4)
		{
			res = sendInfoFile(fd,dataPacket,byteslidos); //enviar as porções do ficheiro

			printf("O que foi lido : %d/%d \n",lidos,fsize);
			flag = FALSE;
			alarm(3);

			if (res == -1) //debug
			{
				while (!flag){}
				continue;
			}
			else
			{
				while(!flag && (RR_RECEIVED == FALSE && REJ_RECEIVED == FALSE))
				{
					detectRRorREJ(fd);
				}
			}

			if (RR_RECEIVED)
			{
				conta = 0;
				alarm(0);
				printf("RR TRUE - inside rr IF\n");
				printf("NEXT PACKET\n");
				dataPacket = memset(dataPacket, 0, PACKET_SIZE);
				lidos += byteslidos;
				resetRRRejFlags();
				break;
			}
			if (REJ_RECEIVED)
			{
				printf("REJ Received TRUE - inside REJ IF\n");
				printf("RESENDING...\n");
				resetRRRejFlags();
				continue;
			}
			printf("No RR or REJ received \n");

			resetRRRejFlags();
			continue;
		}
		if(conta>=4){
			printf("TIMEDOUT WITH READBYTES %d",lidos);
			return lidos;
		}
	}
	STOP = TRUE;
	return lidos;
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

	printf("%d bytes written\n",res);
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
