# Concurrent Programming(Stock Server 구현)

#### 개발 목표: 여러 client들의 동시 접속 및 서비스를 위한 Concurrent stock server 을 구축
  
  Single Process/Thread 기반의 stock server는 여러 client와 동시에 connection이 불가능함(No Concurrency)
  
  Concurrent한 stock server를 구현하여 여러 client와 동시에 connect하고, 각 client의 요청을 concurrent하게 처리
  
  Event-based 방식과 Thread-based 방식으로 각각 Concurrent stock server를 구현
  
  
<hr/>  

#### 주식 서버 설계

##### 주식 관리

  + 주식은 stock.txt 파일에 table형태로 관리
  + Table의 각 행은 주식 ID, 잔여 주식, 주식 단가를 나타냄

##### 가정

  + 주식 단가는 변동x, 잔여 주식 수만 변동
  + client의 주식 판매 요청은 자유롭게 가능(client가 보유하고 있는 주식 고려 x)
  + client의 주식 구매 요청은 잔여 서버의 주식보다 많은 수의 주식을 요구할 경우 처리하지 않음

##### 동작

  1. 서버 실행
  
    ./stockserver <port>
  2. 서버가 실행되면 파일(stock.txt)로부터 주식 정보를 읽어 메모리에 load
  
    주식 정보는 주식 ID를 기준으로 BST로 저장하여 client의 구매, 판매 요청을 효율적으로 처리
  3. Client가 server에 접속
  
    ./stockclient <host> <port>
  4. Client의 요청(Sell, Buy, Show) 처리
  
    show
    buy <stock ID> <count>
    sell <stock ID> <count>
  5. 서버 종료시 메모리에 저장되어 있는 주식 정보를 stock.txt에 store

##### Concurrency Test

  서버 실행 후, 여러 client의 동시 접속 및 요청 처리를 test하기 위해 multiclient program 구현
  multiclient.c 파일의 configuration(client 당 order 수, 주식 개수 등)을 조정하여 다양한 환경에서 test 가능
  
  1. multiclient program 실행
  
    ./multiclient <host> <port> <clients num>
  2. program 실행 시 인자로 입력 받은 client 수 만큼 동시 접속
  3. 각 client는 show, buy, sell 명령을 random하게 수행
