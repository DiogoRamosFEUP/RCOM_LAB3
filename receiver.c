char readSupervisao(int fd, int contador, char C){

	char SET[5]={0x7E,0x03,C,0x03^C,0x7E};

	
	char buf[1];

    //caso de erro
	int res=0;
    res = read(fd,buf,1); /*retorna apos um caracter ter sido inserido*/
	if(res==-1){
	printf("read error\n");
	return ERR;
	}

    //Maquina de estados para receber
	switch(contador){
	case 0:
		if(buf[0]==SET[0])return 0x7E;
		return ERR;

	case 1:
		if(buf[0]==SET[1])return 0x03;
		return ERR;

	case 2:
		if(buf[0]==SET[2])return C;
		//special case;
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
