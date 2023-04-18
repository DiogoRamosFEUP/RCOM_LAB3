#include <unistd.h>
#include <signal.h>
#include <stdio.h>

void atende() // Func para fazer um timer para o transmissor
{
	int flag=1, conta=1;
    printf("alarme # %d\n", conta);
    flag=1;
    conta++;
}

int readSupervisao(unsigned char vetor, int fd) //Func para ler codigo da supervisão
{
	char buf[1], codigo[5] = {0x5C, 0x01, vetor, 0x01^vetor, 0x5C};  //combinações possiveis para as flags de combinações
	int contador = 0, STOP = 0;
	bool error = false; 

	while (STOP = 0 && contador < 5)  //ler o codigo ate ao fim
	{
		read(fd,buf,1); //ler buffer 1 a 1

		switch(contador) //cirar maquina de estado para ver se as flags são as que nós queremos
		{
			case 0:			//comparar primeiro digito da flag de supervisao correta
			if(buf[0] != codigo[0]) error= true;
			break;

			case 1:			//comparar segundo digito da flag de supervisao correta
			if(buf[0] != codigo[1]) error=true;
			break;

			case 2:			//comparar terceiro digito da flag de supervisao correta
			if(buf[0] != codigo[2]) error=true;
			break;

			case 3:			//comparar quarto digito da flag de supervisao correta
			if(buf[0] != codigo[3]) error=true;
			break;

			case 4:			//comparar quinto digito da flag de supervisao correta
			if(buf[0] != codigo[4]) error=true;
			break;
		}
		contador++;
		if (contador==5 && error == false)
		{
			STOP=1;
			return 0;
		}
	}
	return -1;
}

int compareSupervisao(int fd)
{
	unsigned char vetor = 0x5C;
	int k = readSupervisao(vetor,fd);
	return k;
}

int llopen(int fd)
{
	//acabar dia 25/04/2023
}