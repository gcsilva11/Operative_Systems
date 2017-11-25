// PROJETO SO 2016/2017
//
// João Pedro Costa Ferreiro 2014197760 || Guilherme Cardoso Gomes da Silva 2014226354
#include "header.h"


int main(){

	//Abrir pipe para escrita
	int fd;
	if ((fd = open(PIPE_NAME, O_WRONLY))<0){
		perror("Unable to open pipe for writing: ");
		exit(0);
	}

	char option[20];

	printf("Configuration Server setup.\nOptions: 'sch'- Scheduling type 'pool'- Thread pool 'allowed'- Allowed zip files 'exit'- Quit\n");

	while(1){
		config_change new_option;

		memset(option,0,strlen(option));
		printf("Specify next command: ");
		scanf("%s",option);

		if(strcmp(option,"sch")==0){

			new_option.change_type = 0;
			memset(option,0,strlen(option));
			printf("Choose scheduling priority (fifo,static,zip): ");
			scanf("%s",option);

			if(strcmp(option,"fifo")==0){
				new_option.change_type_sec = 0;
				write(fd,&new_option,sizeof(config_change));
			}

			else if(strcmp(option,"static")==0){
				new_option.change_type_sec = 1;
				write(fd,&new_option,sizeof(config_change));
			}

			else if(strcmp(option,"zip")==0){
				new_option.change_type_sec = 2;
				write(fd,&new_option,sizeof(config_change));
			}

			else{
				printf("\nNot accepted, input correct option.\n");
			}
		}

		else if(strcmp(option,"pool")==0){
			new_option.change_type = 1;
			memset(option,0,strlen(option));

			printf("Define new thread pool: ");
			scanf("%s",option);
			new_option.change_type_sec = atoi(option);
			write(fd,&new_option,sizeof(config_change));
		}

		else if(strcmp(option,"allowed")==0){
			memset(option,0,strlen(option));
			new_option.change_type = 2;

			int i;

			for(i=0;i<50;i++){
				strcpy(new_option.allowed_files[i],"");
			}

			printf("Write allowed file name\nMax 50 files, 17 chars each (type 'exit' to stop)\n");
			
			int count = 0;
			scanf("%s",option);
			
			while(strcmp(option,"exit")!=0 && (count<51)){
				strcpy(new_option.allowed_files[count],option);
				strcat(new_option.allowed_files[count],".gz");
				count++;
				memset(option,0,strlen(option));
				scanf("%s",option);
			}
			
			write(fd,&new_option,sizeof(config_change));
		}

		else if(strcmp(option,"exit")==0){		
			printf("\nExiting....\n");
			close(fd);
			exit(0);
		}

		else{
			printf("\nNot accepted, input a proper option\n");
		}
	}
}