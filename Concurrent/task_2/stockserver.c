/* 
 * stockserver.c  
 */ 
/* $begin stockservermain */
#include "csapp.h"
#define NTHREADS 1024
#define SBUFSIZE 1024

typedef struct{		// Shared Buffer 
    int *buf;		// connfd를 저장하고 있는 buffer
    int n;		// buffer의 size
    int front;		// buf[(front+1)%n] is first item
    int rear;		// buf[rear%n] is last item
    sem_t mutex;	// buffer를 보호하기 위한 semaphore
    sem_t slots;	// Counts available slots
    sem_t items;	// Counts available items
}sbuf_t;

typedef struct stock{	// 주식의 정보를 나타내는 node(Binary Tree 구조)
    int ID;		// 주식의 ID	
    int left_stock;	// 남아있는 주식의 수(구매 가능한 수량)
    int price;		// 주식의 가격
    int readcnt;	// 해당 node를 읽고 있는(by show_stock()) thread의 개수
    sem_t mutex, w;	// semaphore
    struct stock* left;	// left child
    struct stock* right;// right child
}STOCK;

sbuf_t sbuf;		// shared buffer
STOCK* root;		// 주식 정보 tree의 root

void sbuf_init(sbuf_t *sp, int n);				// shared buffer 초기화
void sbuf_deinit(sbuf_t *sp);					// shared buffer free
void sbuf_insert(sbuf_t *sp, int item);				// shared buffer에 item(connected fd) 삽입
int sbuf_remove(sbuf_t *sp);					// shared buffer에서 item(connected fd) 추출
void *thread(void *vargp);					// Worker thread 동작 함수
void communicate_client(int connfd);				// worker thread와 client의 communicate

void sigint_handler(int sig);					// SIGINT handler
void store_stock(FILE* fp, STOCK* cur);				// tree에 저장된 주식의 정보를 파일에 저장
int load_stock();						// 파일에 저장된 주식의 정보를 메모리에 저장
STOCK* insert_stock(STOCK* cur, STOCK* new);			// tree에 node 삽입
void show_stock(int connfd, STOCK* cur, char* msg);		// client의 "show" 요청 처리
void buy(int connfd, char* buf);				// client의 "buy" 요청 처리
void sell(int connfd, char* buf);				// client의 "sell" 요청 처리

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:stockserver:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    pthread_t tid;

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }

    Signal(SIGINT, sigint_handler);	// SIGINT handler
	
    if(load_stock() == 0){		// 주식의 정보를 메모리에 저장
	fprintf(stderr, "stock.txt does not exists\n");
    }

    listenfd = Open_listenfd(argv[1]);	// listen fd 생성
    sbuf_init(&sbuf, SBUFSIZE);		// shared buffer 초기화

    for(int i=0; i < NTHREADS; i++)	// worker threads 생성
	Pthread_create(&tid, NULL, thread, NULL);

    while (1) {
	// Master thread에서 client의 connection 요청 처리
	clientlen = sizeof(struct sockaddr_storage); 
	connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // connfd를 통해 새로운 client와 communicate
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
                    client_port, MAXLINE, 0);
        printf("Connected to (%s, %s)\n", client_hostname, client_port);

	sbuf_insert(&sbuf, connfd);	// connfd 정보를 shared buffer에 삽입
    }
    exit(0);
}
/* $end stockservermain */

/* Shared Buffer 초기화 */
void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = Calloc(n, sizeof(int));	// buffer를 메모리에 할당
    sp->n = n;				// buffer의 size = n
    sp->front = sp->rear = 0;		
    Sem_init(&sp->mutex, 0, 1);		// mutex 1로 초기화
    Sem_init(&sp->slots, 0, n);		// 초기 available slot = n
    Sem_init(&sp->items, 0, 0);		// 초기 available item = 0
}

/* Shared Buffer free */
void sbuf_deinit(sbuf_t *sp)
{
    Free(sp->buf);
}

/* Shared Buffer에 item(connfd) 삽입 */
void sbuf_insert(sbuf_t *sp, int item)
{
    P(&sp->slots);				// available slot에 대한 lock을 잡고
    P(&sp->mutex);				// buf에 item 삽입하기 위해 mutex lock
    sp->buf[(++sp->rear)%(sp->n)] = item;	// item 삽입
    V(&sp->mutex);				// mutex unlock
    V(&sp->items);				// available item 생성
}

/* Shared Buffer에서 connfd 제거 */
int sbuf_remove(sbuf_t *sp)
{
    int item;
    P(&sp->items);				// available item에 대한 lock을 잡고
    P(&sp->mutex);				// buf의 item을 read하기 위해 mutex lock
    item = sp->buf[(++sp->front)%(sp->n)];	// read item
    V(&sp->mutex);				// mutex unlock
    V(&sp->slots);				// available slot 생성
    return item;
}

/* Worker Thread 동작 함수 */
void *thread(void *vargp){	
    Pthread_detach(pthread_self());	// self join
    while(1){
	int connfd = sbuf_remove(&sbuf);// shared buffer에서 connfd 추출하여 
	communicate_client(connfd);	// 해당 client와 communicate
	Close(connfd);			// client와 연결 종료시 connfd close
    }
}

/* client와 communicate */
void communicate_client(int connfd){	
    int n;
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);

    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
	char msg[MAXLINE];

        printf("thread %d received %d bytes on fd %d\n", (int)pthread_self(), n, connfd);

        if(!strcmp(buf, "show\n")){	// client가 "show\n" 입력
	    strcpy(msg, "");
            show_stock(connfd, root, msg);	// tree 순회하며 stock정보 msg에 저장
	    Rio_writen(connfd, msg, MAXLINE);
	}

        else if(!strcmp(buf, "exit\n")){// client가 "exit\n" 입력
	    strcpy(msg, "disconnection with server\n");
            Rio_writen(connfd, msg, MAXLINE);
	    break;
        }

        else if(!strncmp(buf, "buy", 3))	// client가 buy~~~ 입력
            buy(connfd, buf);			// buy 요청 처리

        else if(!strncmp(buf, "sell", 4))	// client가 sell~~~ 입력
            sell(connfd, buf);			// sell 요청 처리

        else{				// client가 잘못된 command 입력
	    strcpy(msg, "command exception\n");
            Rio_writen(connfd, msg, MAXLINE);
	}
    }	
}

/* SIGINT handler */
void sigint_handler(int sig){
    FILE* fp = fopen("stock.txt", "w");

    store_stock(fp, root);	// tree에 저장되어 있는 stock data를 "stock.txt"에 저장
    sbuf_deinit(&sbuf);
    fclose(fp);
    exit(0);			// 프로그램 종료
}

/* tree에 저장된 주식 정보를 fp에 저장 */
void store_stock(FILE* fp, STOCK* cur){  	
    if(cur == NULL)
	return;

    // preorder traversal ( node - left child - right child )
    fprintf(fp, "%d %d %d\n", cur->ID, cur->left_stock, cur->price);	// cur node
    store_stock(fp, cur->left);						// left child node
    store_stock(fp, cur->right);					// right child node
    free(cur);
}

/* 주식의 정보를 메모리에 저장 */
int load_stock(){	
    FILE* fp = fopen("stock.txt", "r");
    int ID, left_stock, price;

    if(fp == NULL)
	return 0;

    root = NULL;
    while(fscanf(fp, "%d %d %d", &ID, &left_stock, &price) != EOF){
	/* 새로운 node 할당 */
	STOCK* new = (STOCK*)malloc(sizeof(STOCK));
	new->ID = ID;
	new->left_stock = left_stock;
	new->price = price;
	new->left = NULL;
	new->right = NULL;
	sem_init(&new->mutex, 0, 1);	// semaphore(mutex) 1로 초기화
	sem_init(&new->w, 0, 1);	// semaphore(w) 1로 초기화
	new->readcnt = 0;

	root = insert_stock(root, new);	// binary search tree에 새로운 node 삽입
    }
   
    fclose(fp); 
    return 1;
}

/* tree에 새로운 node 삽입 */
STOCK* insert_stock(STOCK* cur, STOCK* new){	
    /* cur node가 NULL인 경우 */
    if(cur == NULL)	
	cur = new;
    
    /* cur node가 NULL이 아닌 경우 */
    else{		
	if(cur->ID > new->ID)		// 삽입할 node의 주식 ID가 cur node의 주식 ID보다 작은 경우
	    cur->left = insert_stock(cur->left, new);	// cur node의 left child node에 삽입 시도
	else if(cur->ID < new->ID)	// 삽입할 node의 주식 ID가 cur node의 주식 ID보다 큰 경우
	    cur->right = insert_stock(cur->right, new);	// cur node의 right child node에 삽입 시도
    }

    return cur;	// 갱신된 node의 정보 return
}

/* cur node 주식 정보 msg에 저장 */
void show_stock(int connfd, STOCK* cur, char* msg){ 
    if(cur == NULL)
	return;

    char stock_info[30];

    // inorder traversal ( left child - node - right child )
    // favors reader(show)

    show_stock(connfd, cur->left, msg);						// left child node

    P(&cur->mutex);
    cur->readcnt++;
    if(cur->readcnt == 1)
	P(&cur->w);	// cur node를 read(show) 중엔 write(buy, sell) 불가능 하도록 locking
    V(&cur->mutex);

    sprintf(stock_info, "%d %d %d\n", cur->ID, cur->left_stock, cur->price);	// cur node 정보

    P(&cur->mutex);
    cur->readcnt--;
    if(cur->readcnt == 0)
	V(&cur->w);     // cur node를 read(show) 중이 아닐 때 unlocking
    V(&cur->mutex);
    strcat(msg, stock_info);							// msg에 cur node 정보 저장

    show_stock(connfd, cur->right, msg); 					// right child node
}

/* client의 "buy" 요청 처리 */
void buy(int connfd, char* buf){ 
    STOCK* ptr = root;
    char msg[30];
    char command[MAXLINE];
    int order_ID, order_NUM;

    sscanf(buf, "%s %d %d", command, &order_ID, &order_NUM);
    /* binary search tree 탐색 */
    while(ptr){	
	if(order_ID == ptr->ID){	// order_ID 찾은 경우
	    if(ptr->left_stock >= order_NUM){	// 잔여 주식이 주문 수량보다 많은 경우 구매 가능

		/* 다른 thread가 해당 node의 정보를 read(show)중이거나
		   write(buy, sell) 중일 경우 wait */
		P(&ptr->w);	// locking
		/* Critical section */
		ptr->left_stock -= order_NUM;	// 구매 요청 처리
		/* Writing happens */
		V(&ptr->w);	// unlocking

		strcpy(msg, "[buy] success\n");
		Rio_writen(connfd, msg, MAXLINE);
	    }	

	    else{				// 잔여 주식이 주문 수량보다 적은 경우 구매 불가
		strcpy(msg, "Not enough left stocks\n");
		Rio_writen(connfd, msg, MAXLINE);
	    }

	    return;
	}

	else if(order_ID < ptr->ID)	// order_ID가 cur node의 주식 ID보다 작은 경우
	    ptr = ptr->left;		// cur node의 left child 탐색
	else				// order_ID가 cur_node의 주식 ID보다 큰 경우
	    ptr = ptr->right;		// cur node의 right child 탐색
    }

    //order_ID가 tree에 존재하지 않는 경우
    strcpy(msg, "Order_ID does not exists\n");
    Rio_writen(connfd, msg, MAXLINE);
}

/* client의 sell 요청 처리 */
void sell(int connfd, char* buf){ 
    STOCK* ptr = root;
    char msg[30];
    char command[MAXLINE];
    int order_ID, order_NUM;

    sscanf(buf, "%s %d %d", command, &order_ID, &order_NUM);
    /* binary search tree 탐색 */
    while(ptr){
        if(order_ID == ptr->ID){	// order_ID 찾은 경우

            /* 다른 thread가 해당 node의 정보를 read(show)중이거나
               write(buy, sell) 중일 경우 wait */
	    P(&ptr->w);		// locking
	    /* Critical section */
            ptr->left_stock += order_NUM;	// 판매 요청 처리
	    /* Writing happens */
	    V(&ptr->w);		// unlocking
		
	    strcpy(msg, "[sell] success\n");
            Rio_writen(connfd, msg, MAXLINE);

            return;
        }

        else if(order_ID < ptr->ID)	// order_ID가 cur node의 주식 ID보다 작은 경우
            ptr = ptr->left;		// cur node의 left child 탐색
        else				// order_ID가 cur node의 주식 ID보다 큰 경우
            ptr = ptr->right;		// cur node의 right child 탐색
    }

    // order_ID가 tree에 존재하지 않는 경우
    strcpy(msg, "Order_ID does not exists\n");
    Rio_writen(connfd, msg, MAXLINE);
}
