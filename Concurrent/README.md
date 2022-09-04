# Concurrent Programming(Stock Server 구현)

#### 개발 목표: 여러 client들의 동시 접속 및 서비스를 위한 Concurrent stock server 을 구축
  
  Single Process/Thread 기반의 stock server는 여러 client와 동시에 connection이 불가능함(No Concurrency)
  
  Concurrent한 stock server를 구현하여 여러 client와 동시에 connect하고, 각 client의 요청을 concurrent하게 처리
  
  Event-based 방식과 Thread-based 방식으로 각각 Concurrent stock server를 구현
  
  
<hr/>  
#### 주식 서버 설계

###### 주식 관리

  + 주식은 stock.txt 파일에 table형태로 관리
  + Table의 각 행은 주식 ID, 잔여 주식, 주식 단가를 나타냄

###### 가정

  + 주식 단가는 변동x, 잔여 주식 수만 변동
  + client의 주식 판매 요청은 자유롭게 가능(client가 보유하고 있는 주식 고려 x)
  + client의 주식 구매 요청은 잔여 서버의 주식보다 많은 수의 주식을 요구할 경우 처리하지 않음

###### 동작

  1. 서버 실행
  2. 주식 정보가 저장된 파일(stock.txt)로부터 주식 정보를 읽어 메모리에 load
    2-1. 주식 정보는 주식 ID를 기준으로 BST로 저장하여 구매, 판매 요청을 효율적으로 처리
  3. Client의 요청(Sell, Buy, Show) 처리
  4. 서버 종료시 메모리의 주식 정보를 stock.txt에 저장
