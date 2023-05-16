char readSupervisao(int fd, int contador, char C){

	char SET[5]={0x7E,0x03,C,0x03^C,0x7E};

	
	char buf[1];

    
	int res=0;
    res = read(fd,buf,1); /*retorna apos um caracter ter sido inserido*/
	if(res==-1){
	printf("read error\n");
	return ERR;
	}

    //Maquina de estados para receber SET
	switch(contador){
	case 0:
		if(buf[0]==SET[0])return 0x7E;
		return ERR;

	case 1:
		if(buf[0]==SET[1])return 0x03;
		return ERR;

	case 2:
		if(buf[0]==SET[2])return C;
		//caso especial;
		if(C==0x07 && buf[0]==0x0B){
			return 0x0C;
		}
		return ERR;

	case 3:
		if(buf[0]==SET[3])return buf[0];
		return ERR2;

	case 4:
		if(buf[0]==SET[4])return 0X7E;
		return ERR;

    default:
		return ERR;
	}
}


void llopen(int fd, int type){

 char ua[5]={0x7E,0x03,0x07,0x03^0x07,0x7E};

 char readchar[2];
 int contador = 0;

 if(type==0){

	 while (STOP==FALSE) {  /*condição para haver loop e iniciar o ciclo*/

	  readchar[0]=maquina_estados(fd,contador,0x03);
	  printf("0x%02x \n",(unsigned char)readchar[0]);
	  readchar[1]='\0';

	  contador++;

	  if(readchar[0]==ERR)contador=0;
	  if(readchar[0]==ERR2)contador=-1;
        
        //condicao de paragem do ciclo while
	  	if (contador==5){
		 STOP=TRUE;
	    }

	 }
 }
	printf("Sending UA...\n");
    writeBytes(fd,ua);
}
