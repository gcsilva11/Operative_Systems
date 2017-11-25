// PROJETO SO 2016/2017
//
// João Pedro Costa Ferreiro 2014197760 || Guilherme Cardoso Gomes da Silva 2014226354
#include "header.h"


int main(){

	//Função init
	init();

	//Criacao processo estatisticas
	stat_process = fork();
	if(stat_process==0){
		stat_pid=(long)getpid();

		#if DEBUG
		printf("main: statistics process created. PID:%ld\n", stat_pid);
		#endif

		statistics();
	}
	
	// Serve requests
	else{
		main_pid=(long)getpid();

		#if DEBUG
		printf("main: main process PID:%ld\n", main_pid);
		#endif
		
		pthread_create(&config_thread, NULL, configs, NULL);		
		
		#if DEBUG
		printf("main: configuration thread created.\n");
		#endif

		pthread_create(&scheduler, NULL, scheduler_thread, NULL);
		
		#if DEBUG
		printf("main: scheduler thread created.\n");
		#endif

		while(1);
	}
	return 0;
}

//Config thread
void *configs(){

	int i;

	//Criar named pipe se nao existe
	if((mkfifo(PIPE_NAME,O_CREAT|O_EXCL|0600)<0)&&(errno!=EEXIST)){
		perror("Error creating pipe: ");
		exit(0);
	}

	//Abre o pipe para leitura
	int fd;
	if((fd=open(PIPE_NAME, O_RDONLY))<0){
		perror("Error opening pipe for reading: ");
		exit(0);
	}

	config_change changes;

	while(1){

		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

		if(read(fd, &changes, sizeof(config_change))>0){

			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
			
			if(changes.change_type==0){
				printf("\n\t[CONFIG SERVER] New schedueling policy:\n");
				if(changes.change_type_sec==0){
					printf("\tSet to FIFO (first in first out)\n");
					CURRENT_SCHE_TYPE = 0;

				}
				else if(changes.change_type_sec==1){
					printf("\tSet to Static Priority \n");
					CURRENT_SCHE_TYPE = 1;
				}
				else{
					printf("\tSet to Zip Priority \n");
					CURRENT_SCHE_TYPE = 2;
				}

			}
			else if(changes.change_type==1){
				kill_workers=1;
				for(i=0;i<MAX_THREADS;i++){
					pthread_join(my_thread[i],NULL);
					printf("\n\t[WORKER] Thread closed.\n");
				}
				MAX_THREADS = changes.change_type_sec;
				my_thread = (pthread_t *) realloc(my_thread,MAX_THREADS*sizeof(pthread_t*));
				kill_workers=0;
				for (i=0;i<MAX_THREADS;i++){
					pthread_create(&my_thread[i], NULL, worker, NULL);
				}
				printf("\n\t[CONFIG SERVER] New thread pool: %d threads\n",changes.change_type_sec);

			}

			else if(changes.change_type==2){
				printf("\n\t[CONFIG SERVER] New allowed file list:\n");

				//Limpa array de allowed
				for(i=0;i<sizeof(allowed);i++){
					memset(&allowed[i],0,sizeof(allowed[i]));
				}
				
				for(i=0;i<strlen(changes.allowed_files[i]);i++){
					allowed[i]=changes.allowed_files[i];
					printf("%s ",allowed[i]);
				}
				printf("\n");
			}
		}
	}
}

//Worker thread
void *worker(){

	#if DEBUG
	printf("worker: worker thread created\n");
	#endif

	while (1){

		if(kill_workers){
			pthread_exit(NULL);
			return NULL;
		}

		if(request_count < buff.write_pos){

			request_count++;
		
			clock_t begin = clock();

			if (buff.cell[buff.read_pos].is_static==1){
				send_page(buff.cell[buff.read_pos].socket);
				sem_wait(&stat_semaphore);
				strcpy(to_send[to_send->stat_count].name,buff.cell[buff.read_pos].req_buf);
				to_send[to_send->stat_count].static_zip=0;
				clock_t end = clock();
				double ex_time = ((double) (end-begin) )/ CLOCKS_PER_SEC;
				to_send[to_send->stat_count].time = ex_time;
				time_t t = time(NULL);
				struct tm *tm = localtime(&t);
				char current_date[64];
				strftime(current_date,sizeof(current_date), "%c",tm);
				strcpy(to_send[to_send->stat_count].date,current_date);
				to_send->stat_count++;
				sem_post(&stat_semaphore);
			}

			else if(buff.cell[buff.read_pos].is_static==0){
				execute_script(buff.cell[buff.read_pos].socket);
				sem_wait(&stat_semaphore);
				strcpy(to_send[to_send->stat_count].name,buff.cell[buff.read_pos].req_buf);
				to_send[to_send->stat_count].static_zip=1;
				clock_t end = clock();
				double ex_time = ((double) (end-begin) )/ CLOCKS_PER_SEC;
				to_send[to_send->stat_count].time = ex_time;
				time_t t = time(NULL);
				struct tm *tm = localtime(&t);
				char current_date[64];
				strftime(current_date,sizeof(current_date), "%c",tm);
				strcpy(to_send[to_send->stat_count].date,current_date);
				to_send->stat_count++;
				sem_post(&stat_semaphore);
			}
			
			// Terminate connection with client 
			close(buff.cell[buff.read_pos].socket);

			#if DEBUG
			printf("worker:\twrite_pos: %d\t read_pos:%d\n",buff.write_pos,buff.read_pos);
			#endif	

			pthread_mutex_lock(&mutex);		
			buff.read_pos=(buff.read_pos+1)%SIZE_BUF;
			pthread_mutex_unlock(&mutex);
		}
	}
}

//Scheduler thread
void *scheduler_thread(){
	
	int i,count;
	cell aux;

	for (i=0;i<MAX_THREADS;i++){
		pthread_create(&my_thread[i], NULL, worker, NULL);
	}
	
	//Criação do socket
	struct sockaddr_in client_name;
	socklen_t client_name_len = sizeof(client_name);

	while(1){

		// Accept connection on socket
		if ( (buff.cell[buff.write_pos].socket = accept(socket_conn,(struct sockaddr *)&client_name,&client_name_len)) == -1 ) {
			printf("Error accepting connection\n");
			exit(1);
		}

		// Identify new client
		identify(buff.cell[buff.write_pos].socket);

		// Process request
		get_request();

		#if DEBUG
		printf("scheduler:\twrite_pos: %d\t read_pos:%d\n",buff.write_pos,buff.read_pos);
		#endif	

		count = 0;


		pthread_mutex_lock(&mutex);
		while(((buff.write_pos-count-1)>buff.read_pos)){

			//Verifica se a posicao anterior do buffer ja foi lida
			if((buff.write_pos-count-2)>buff.read_pos){
				//Verifica se foi feito na mesma policy que a nova cell
				if((buff.cell[buff.write_pos-count-2].sche_type)==(buff.cell[buff.write_pos-count-1].sche_type)){
					//Faz ajustes de acordo com a policy em pratica
					//(Em fifo nao precisa de ajustar)
					//Static First
					if(CURRENT_SCHE_TYPE==1){
						//Verifica se o anterior é static, se nao, troca (se for static)
						if(((buff.cell[buff.write_pos-count-2].is_static)==0)&&(buff.cell[buff.write_pos-count-1].is_static==1)){
							aux = buff.cell[buff.write_pos-count-2];
							buff.cell[buff.write_pos-count-2]=buff.cell[buff.write_pos-count-1];
							buff.cell[buff.write_pos-count-1]=aux;
							printf("\n[SCHEDULER] Swapped positions.\n");
						}
					}
					if(CURRENT_SCHE_TYPE==2){
						//Verifica se o anterior é zip, se nao, troca (se for zip)
						if(((buff.cell[buff.write_pos-count-2].is_static)==1)&&(buff.cell[buff.write_pos-count-1].is_static==0)){
							aux = buff.cell[buff.write_pos-count-2];
							buff.cell[buff.write_pos-count-2]=buff.cell[buff.write_pos-count-1];
							buff.cell[buff.write_pos-count-1]=aux;
							printf("\n[SCHEDULER] Swapped positions.\n");
						}
					}
				}
			}
			count++;
		}
		pthread_mutex_unlock(&mutex);
	}
}

//Função de inicialização de variáveis
void init(void *arg){

	int i;

	//Ler config.txt
	load_config();

	//Tratamento sinais
	sigset_t all_signals;
	if(sigfillset(&all_signals)!=0){
		printf("Erro a bloquear sinais\n");
		exit(0);
	}

	//Remove os sinais necessarios
	sigdelset(&all_signals,SIGINT);
	sigdelset(&all_signals,SIGUSR1);
	sigdelset(&all_signals,SIGUSR2);

	//Ignora os restantes
	sigprocmask(SIG_SETMASK,&all_signals,NULL);

	//Memoria partilhada
	int sizeof_send = sizeof(*to_send) * SIZE_BUF;
	shmid = shmget(IPC_PRIVATE,sizeof_send,0766 | IPC_CREAT);
	if(shmid == -1){
        perror("shmget");       
        exit(1);
    }
	to_send = (statistic *) shmat(shmid,NULL,0);
	to_send->stat_count = 0;

	//Semaforo
	sem_init(&stat_semaphore,1,1);

	//Sinal para limpeza de memoria
	signal(SIGINT,cleanup);

	//Criação das threads
	my_thread = (pthread_t *) realloc(my_thread,MAX_THREADS*sizeof(pthread_t*));

	//Configuração da porta a ser usada
	if ((socket_conn=fireup(SERVER_PORT))==-1)
		exit(1);
	
	//Inicialização variaveis do buffer
	buff.write_pos=0;
	buff.read_pos=0;
	for (i=0;i<SIZE_BUF;i++){
		buff.cell[i].is_static=-1;
	}

	return;
}

//Ler config.txt
int load_config(){

	FILE *config = fopen("config.txt","r");
	char linha[256];
	char *token;
	char *search ="=";
	int i = 0;

	//Primeira linha -> SERVERPORT
	fgets(linha,sizeof(linha),config);
	token = strtok(linha,search);
	token = strtok(NULL,"\n");
	SERVER_PORT = atoi(token);
	printf("Server port set to %d\n",SERVER_PORT);
	
	//Segunda linha -> HANDLING (FIFO/STATIC/ZIP)
	fgets(linha,sizeof(linha),config);
	token = strtok(linha,search);
	token = strtok(NULL,"\n");
	if(strcmp(token,"FIFO")==0){
		printf("Set schedueling type to FIFO\n");
		CURRENT_SCHE_TYPE = 0;
	}
	else if(strcmp(token,"STATIC")==0){
		printf("Set schedueling type to STATIC PRIORITY\n");
		CURRENT_SCHE_TYPE = 1;
	}
	else if(strcmp(token,"ZIP")==0){
		printf("Set schedueling type to ZIP PRIORITY\n");
		CURRENT_SCHE_TYPE = 2;
	}
	else{
		printf("Error - Unknown Handling Option (Line 3 in config.txt)\nPlease state FIFO/STATIC/ZIP	\nExiting....\n");
		exit(0);
	}

	//Terceira linha -> THREAD POOL
	fgets(linha,sizeof(linha),config);
	token = strtok(linha,search);
	token = strtok(NULL,"\n");
	MAX_THREADS = atoi(token);
	printf("Set max number of threads to %d\n",MAX_THREADS);
	
	//Quarta linha -> Accepted Zip Files
	fgets(linha,sizeof(linha),config);
	token = strtok(linha,search);
	token = strtok(NULL,"\n");
	allowed[i] = strtok(token,",");
	printf("List of allowed zips:\n");
	while(allowed[i]!= NULL){
		printf("%s ",allowed[i]);
		i++;
		allowed[i] = strtok(NULL,",");
	}
	printf("\n");

	#if DEBUG
	printf("load_config: config.txt read.\n");
	#endif

	return 0;
}

//Trata das estatísticas
int statistics(){

	printf("[STATISTICS] PID:%ld\n", stat_pid);

	signal(SIGUSR1,stat_handler); //kill -USR1 pid
	signal(SIGUSR2,stat_handler); //kill -USR2 pid
	
	while(1){
		//Se o pedido ainda nao foi processado pelas estatisticas
		if(to_send->stat_count>processed_stats){
			//Verifica se o pedido foi static ou zip
			if(to_send[processed_stats].static_zip==0){
				information.total_static++;
				information.avg_static = ((information.avg_static*(processed_stats)+to_send->time))/(processed_stats+1);
			}
			else{
				information.total_zip++;
				information.avg_zip = ((information.avg_static*(processed_stats)+to_send->time))/(processed_stats+1);
			}
			processed_stats++;
		}
	}
	return 0;
}

// Identifica cliente (address and port) from socket
void identify(int socket){

	char ipstr[INET6_ADDRSTRLEN];
	socklen_t len;
	struct sockaddr_in *s;
	int port;
	struct sockaddr_storage addr;

	len = sizeof addr;
	getpeername(socket, (struct sockaddr*)&addr, &len);

	// Assuming only IPv4
	s = (struct sockaddr_in *)&addr;
	port = ntohs(s->sin_port);
	inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);

	#if DEBUG
	printf("identify: received new request from %s port %d\n",ipstr,port);
	#endif

	return;
}

// Processes request from client
void get_request(){
	
	int i, j, found_get=0;
	while ( read_line(buff.cell[buff.write_pos].socket,SIZE_BUF) > 0 ) {
		if(!strncmp(buf,GET_EXPR,strlen(GET_EXPR))) {
			// GET received, extract the requested page/script
			found_get=1;
			i=strlen(GET_EXPR);
			j=0;
			while( (buf[i]!=' ') && (buf[i]!='\0') ){
				buff.cell[buff.write_pos].req_buf[j++]=buf[i++];
			}
			buff.cell[buff.write_pos].req_buf[j]='\0';
		}
	}

	// Currently only supports GET 
	if(found_get!=1){
		printf("Request from client without a GET\n");
		exit(1);
	}

	// If no particular page is requested then we consider htdocs/index.html
	if(!strlen(buff.cell[buff.write_pos].req_buf)){
		sprintf(buff.cell[buff.write_pos].req_buf,"index.html");
	}

	if (strlen(buff.cell[buff.write_pos].req_buf) >= 3 && strcmp(buff.cell[buff.write_pos].req_buf + strlen(buff.cell[buff.write_pos].req_buf) - 3, ".gz") == 0) {
		buff.cell[buff.write_pos].is_static=0;
		buff.cell[buff.write_pos].sche_type=CURRENT_SCHE_TYPE;
	}

	else{
		buff.cell[buff.write_pos].is_static=1;
		buff.cell[buff.write_pos].sche_type=CURRENT_SCHE_TYPE;
	}

	#if DEBUG
	printf("get_request: client requested the following page: %s\n",buff.cell[buff.write_pos].req_buf);
	#endif
	
	buff.write_pos=(buff.write_pos+1)%SIZE_BUF;

	return;
}

// Send message header (before html page) to client
void send_header(int socket){

	#if DEBUG
	printf("send_header: sending HTTP header to client\n");
	#endif

	sprintf(buf,HEADER_1);
	send(socket,buf,strlen(HEADER_1),0);
	sprintf(buf,SERVER_STRING);
	send(socket,buf,strlen(SERVER_STRING),0);
	sprintf(buf,HEADER_2);
	send(socket,buf,strlen(HEADER_2),0);

	return;
}

// Execute script
void execute_script(int socket){

	char temp_buf[SIZE_BUF];

	sprintf(temp_buf,"gzip -d htdocs/%s",buff.cell[buff.read_pos].req_buf);

	int sys_call = system(temp_buf);
	
	if(sys_call != 0 && sys_call != 512){
		#if DEBUG
		printf("execute_script: client requested inexistent file: %s\n",buff.cell[buff.read_pos].req_buf);
		#endif
	}

	else{
		#if DEBUG
		if (sys_call==0)
			printf("execute_script: %s file decompressed\n",buff.cell[buff.read_pos].req_buf);
		if (sys_call==512)
			printf("execute_script: %s file is already decompressed\n",buff.cell[buff.read_pos].req_buf);
		#endif

		memset(temp_buf, 0, sizeof(temp_buf));
		strncpy(temp_buf,buff.cell[buff.read_pos].req_buf,(strlen(buff.cell[buff.read_pos].req_buf)-3));
		memset(buff.cell[buff.read_pos].req_buf, 0, sizeof(buff.cell[buff.read_pos].req_buf));
		strcpy(buff.cell[buff.read_pos].req_buf,temp_buf);

		#if DEBUG
		printf("execute_script: client requested the following page: %s\n",buff.cell[buff.write_pos].req_buf);
		#endif

		send_page(buff.cell[buff.read_pos].socket);
	}
	
	return;
}

// Send html page to client
void send_page(int socket){

	FILE * fp;

	// Searchs for page in directory htdocs
	sprintf(buf_tmp,"htdocs/%s",buff.cell[buff.read_pos].req_buf);
	

	#if DEBUG
	printf("send_page: searching for %s\n",buf_tmp);
	#endif

	// Verifies if file exists
	if((fp=fopen(buf_tmp,"rt"))==NULL) {
		#if DEBUG
		printf("send_page: page %s not found, alerting client\n",buf_tmp);
		#endif

		printf("\tThe requested page does not exist\n");
		not_found(socket);
	}
	else {
		// First send HTTP header back to client
		send_header(socket);

		#if DEBUG
		printf("send_page: sending page %s to client\n",buf_tmp);
		#endif

		while(fgets(buf_tmp,SIZE_BUF,fp))
			send(socket,buf_tmp,strlen(buf_tmp),0);
		
		// Close file
		fclose(fp);
	}
	return;
}

// Reads a line (of at most 'n' bytes) from socket
int read_line(int socket,int n){

	int n_read;
	int not_eol; 
	int ret;
	char new_char;

	n_read=0;
	not_eol=1;

	while (n_read<n && not_eol) {
		ret = read(socket,&new_char,sizeof(char));
		if (ret == -1) {
			printf("Error reading from socket (read_line)");
			return -1;
		}
		else if (ret == 0) {
			return 0;
		}
		else if (new_char=='\r') {
			not_eol = 0;
			// consumes next byte on buffer (LF)
			read(socket,&new_char,sizeof(char));
			continue;
		}		
		else {
			buf[n_read]=new_char;
			n_read++;
		}
	}

	buf[n_read]='\0';
	#if DEBUG
	printf("read_line: new line read from client socket: %s\n",buf);
	#endif
	
	return n_read;
}

// Creates, prepares and returns new socket
int fireup(int port){

	int new_sock;
	struct sockaddr_in name;

	// Creates socket
	if ((new_sock = socket(AF_INET, SOCK_STREAM, 0))==-1) {
		printf("Error creating socket\n");
		return -1;
	}

	// Binds new socket to listening port 
 	name.sin_family = AF_INET;
 	name.sin_port = htons(port);
 	name.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(new_sock, (struct sockaddr *)&name, sizeof(name)) < 0) {
		printf("Error binding to socket\n");
		return -1;
	}

	// Starts listening on socket
 	if (listen(new_sock, 5) < 0) {
		printf("Error listening to socket\n");
		return -1;
	}
	return(new_sock);
}

// Sends a 404 not found status message to client (page not found) 
void not_found(int socket){

 	sprintf(buf,"HTTP/1.0 404 NOT FOUND\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,SERVER_STRING);
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"Content-Type: text/html\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"<HTML><TITLE>Not Found</TITLE>\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"<BODY><P>Resource unavailable or nonexistent.\r\n");
	send(socket,buf, strlen(buf), 0);
	sprintf(buf,"</BODY></HTML>\r\n");
	send(socket,buf, strlen(buf), 0);

	return;
}

//Cleanup before closing
void cleanup(int sig){

	if(stat_process==0){
		exit(0);

	}

	else{
		int i;

		printf("\n\t[SERVER] TERMINATING\n");

		unlink(PIPE_NAME);
		#if DEBUG
		printf("cleanup: pipe closed.\n");
		#endif

		pthread_mutex_destroy(&mutex);
		sem_destroy(&stat_semaphore);
		#if DEBUG
		printf("cleanup: semaphores deleted.\n");
		#endif

		shmdt(to_send);
		shmctl(shmid,IPC_RMID,NULL);
		#if DEBUG
		printf("cleanup: shared memory deleted.\n");
		#endif

		kill_workers=1;
		for(i=0;i<MAX_THREADS;i++){
			pthread_join(my_thread[i],NULL);
			#if DEBUG
			printf("cleanup: worker thread id:%d closed.\n",i);
			#endif
		}
		free(my_thread);

		pthread_cancel(scheduler);
		#if DEBUG
		printf("cleanup: scheduler thread closed.\n");
		#endif

		pthread_cancel(config_thread);
		#if DEBUG
		printf("cleanup: configuration thread closed.\n");
		#endif
		
		close(socket_conn);
		#if DEBUG
		printf("cleanup: socket connection closed.\n");
		#endif

		exit(0);
	}
}

void stat_handler(int sig){

	if(sig == SIGUSR1){
		printf("\t[STATISTICS] Information requested\n");
		printf("Total static requests served: %d\n",information.total_static);
		printf("Total zip requests served: %d\n",information.total_zip);
		printf("Average static request serve time: %f\n",information.avg_static);
		printf("Average zip request serve time: %f\n",information.avg_zip);
	}

	if(sig == SIGUSR2){
		printf("\t[STATISTICS] Reset requested\n");
		information.total_static=0;
		information.total_zip=0;
		information.avg_static=0;
		information.avg_zip=0;
		#if DEBUG
		printf("stat_handler: reset sucesseful\n");
		#endif
	}
}