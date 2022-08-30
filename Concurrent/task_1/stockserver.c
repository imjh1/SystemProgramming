/* 
 * stockserver.c  
 */ 
/* $begin stockservermain */
#include "csapp.h"

typedef struct{	// represents a pool of connected descriptors */
    int maxfd;		// Largest descriptor in read_set
    fd_set read_set;	// Set of all active descriptor
    fd_set ready_set;	// Subset of descriptors ready for reading
    int nready;		// select 함수를 통해 ready descriptors의 수를 나타냄
    int maxi;		// High water index into client array
    int clientfd[FD_SETSIZE];	 // Set of active descriptors
    rio_t clientrio[FD_SETSIZE]; // Set of active read buffers
}pool;

typedef struct stock{ 	// 주식의 정보를 나타내는 node(Binary Tree 구조)
    int ID;		// 주식의 ID
    int left_stock;	// 남아있는 주식의 수(구매 가능한 수량)
    int price;		// 주식의 가격
    /*  
    int readcnt;
    set_t mutex;

        task_2 Thread based Approach에서 사용
        Event driven Approach는 single process를 통해
        One logical control flow와 address space를 가지므로
	Thread based Approach에서 사용하는 synchronize 기법을 사용 X

    */
    struct stock* left;	// left child
    struct stock* right;// right child
}STOCK;

STOCK* root;		// 주식 정보 tree의 root

void init_pool(int listenfd, pool *p);			  	// pool 초기화
void add_client(int connfd, pool *p);			  	// 새로운 client 접속시 pool에 삽입
void check_clients(pool *p);				  	// 각 client로부터 들어온 요청 확인

void sigint_handler(int sig);                                   // SIGINT handler
void store_stock(FILE* fp, STOCK* cur);                         // tree에 저장된 주식의 정보를 파일에 저장
int load_stock();                                               // 파일에 저장된 주식의 정보를 메모리에 저장
STOCK* insert_stock(STOCK* cur, STOCK* new);                    // tree에 node 삽입
void show_stock(int connfd, STOCK* cur, char* msg);	  	// client의 "show" 요청 처리
void buy(int connfd, char* buf);			  	// client의 "buy" 요청 처리
void sell(int connfd, char* buf);			  	// client의 "sell" 요청 처리

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:stockserver:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];
    static pool pool;

    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(0);
    }

    Signal(SIGINT, sigint_handler);	// SIGINT handler

    if(load_stock() == 0){		// 주식의 정보를 메모리에 저장
	fprintf(stderr, "stock.txt does not exists\n");
    }

    listenfd = Open_listenfd(argv[1]);	// listen fd 생성
    init_pool(listenfd, &pool);		// pool 초기화

    while (1) {
	pool.ready_set = pool.read_set;	
	pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);	// Select를 통해 listen/connected fd 상태 확인
	if(FD_ISSET(listenfd, &pool.ready_set)){ // listen fd에 새로운 client로부터 connect 요청 들어온 경우
	    clientlen = sizeof(struct sockaddr_storage); 
	    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);	// connfd를 통해 새로운 client와 communicate
            Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, 
                        client_port, MAXLINE, 0);
            printf("Connected to (%s, %s)\n", client_hostname, client_port);

	    add_client(connfd, &pool);		// connfd 정보를 pool에 저장
	}

	check_clients(&pool);	// 각 connfd로 들어온 요청 확인
    }
    exit(0);
}
/* $end stockservermain */

/* file descriptor pool 초기화 */
void init_pool(int listenfd, pool *p){
    /* 연결된 connfd 없음 */
    int i;
    p->maxi = -1;
    for(i=0; i< FD_SETSIZE; i++)
	p->clientfd[i] = -1;

    /* listenfd pool에 삽입 */
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

/* 새로 연결된 client의 connfd를 pool에 삽입 */
void add_client(int connfd, pool *p){
    int i;
    p->nready--;
    for(i=0; i<FD_SETSIZE; i++){// Find an available slot
	if(p->clientfd[i] < 0){	// available slot
	    /* connected descriptor를 pool에 삽입 */
	    p->clientfd[i] = connfd;
	    Rio_readinitb(&p->clientrio[i], connfd);  // buffer 초기화

	    /* Add the descriptor to descriptor set */
	    FD_SET(connfd, &p->read_set);
	    
	    /* Update max descriptor and pool high water mark */
	    if(connfd > p->maxfd)
		p->maxfd = connfd;
	    if(i > p->maxi)
		p->maxi = i;
	    break;
	}
    }
    if(i==FD_SETSIZE)
	app_error("add_client error: Too many clients");
}

/* pending input이 들어온 fd의 요청 처리 */
void check_clients(pool *p){	
    int i, connfd, n;
    char buf[MAXLINE];
    rio_t rio;

    for(i=0; (i<=p->maxi) && (p->nready > 0); i++){	// pending input이 들어온 connfd의 요청 처리
	connfd = p->clientfd[i];
	rio = p->clientrio[i];

	/* If the descriptor is ready */
	if((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))){
	    p->nready--;
	    if((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
		char msg[MAXLINE];
		printf("Server received %d bytes on fd %d\n", n, connfd);

		if(!strcmp(buf, "show\n")){	// client가 "show\n" 입력
		    strcpy(msg, "");
		    show_stock(connfd, root, msg);	// tree순회하며 stock정보 msg에 저장
		    Rio_writen(connfd, msg, MAXLINE);	
		}
	
		else if(!strcmp(buf, "exit\n")){// client가 "exit\n" 입력
		    strcpy(msg, "disconnection with server\n");
		    Rio_writen(connfd, msg, MAXLINE);
		    Close(connfd);		// 연결 종료
		    FD_CLR(connfd, &p->read_set);
		    p->clientfd[i] = -1;
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

	    /* EOF detected, remove descriptor from pool */
	    else{
		Close(connfd);
		FD_CLR(connfd, &p->read_set);
		p->clientfd[i] = -1;
	    }
	}
    }
}

/* SIGINT handler */
void sigint_handler(int sig){  
    FILE* fp = fopen("stock.txt", "w");

    store_stock(fp, root);      // tree에 저장되어 있는 stock data를 "stock.txt"에 저장
    fclose(fp);
    exit(0);                    // 프로그램 종료
}

/* tree에 저장된 주식 정보를 fp에 저장 */
void store_stock(FILE* fp, STOCK* cur){  
    if(cur == NULL)
        return;

    // preorder traversal ( node - left child - right child )
    fprintf(fp, "%d %d %d\n", cur->ID, cur->left_stock, cur->price);    // cur node
    store_stock(fp, cur->left);                                         // left child node
    store_stock(fp, cur->right);                                        // right child node
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

        root = insert_stock(root, new); // binary search tree에 새로운 node 삽입
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
        if(cur->ID > new->ID)           // 삽입할 node의 주식 ID가 cur node의 주식 ID보다 작은 경우
            cur->left = insert_stock(cur->left, new);   // cur node의 left child node에 삽입 시도
        else if(cur->ID < new->ID)      // 삽입할 node의 주식 ID가 cur node의 주식 ID보다 큰 경우
            cur->right = insert_stock(cur->right, new); // cur node의 right child node에 삽입 시도
    }

    return cur; // 갱신된 node 정보 return
}

/* cur node 주식 정보 msg에 저장 */
void show_stock(int connfd, STOCK* cur, char* msg){ 
    if(cur == NULL)	
	return;

    char stock_info[30];

    // inorder traversal ( left child - node - right child )

    show_stock(connfd, cur->left, msg);						// left child node
    sprintf(stock_info, "%d %d %d\n", cur->ID, cur->left_stock, cur->price);	// cur node 정보
    strcat(msg, stock_info);							// msg에 cur node 정보 저장
    show_stock(connfd, cur->right, msg);					// right child node
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
		ptr->left_stock -= order_NUM;	// 구매 요청 처리
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
	else				// order_ID가 cur node의 주식 ID보다 큰 경우
	    ptr = ptr->right;		// cur node의 right child 탐색
    }

    // order_ID가 tree에 존재하지 않는 경우
    strcpy(msg, "Order_ID does not exists\n");
    Rio_writen(connfd, msg, MAXLINE);
}

/* client의 "sell" 요청 처리 */
void sell(int connfd, char* buf){ 
    STOCK* ptr = root;
    char msg[30];
    char command[MAXLINE];
    int order_ID, order_NUM;

    sscanf(buf, "%s %d %d", command, &order_ID, &order_NUM);
    /* binary search tree 탐색 */
    while(ptr){
        if(order_ID == ptr->ID){	// order_ID 찾은 경우
            ptr->left_stock += order_NUM;	// 판매 요청 처리
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
