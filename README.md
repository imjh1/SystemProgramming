# MyShell 구현

3개의 Phase에 걸쳐 Shell을 구현
+ ### Phase1
  
  ###### 개발 목표: 단일 shell command(ls, mkdir, cd, touch, etc..) 입력에 대해 처리하도록 구현(foreground process)
  
  
  ###### 실행 결과
  
    <img width="505" alt="phase1_run" src="https://user-images.githubusercontent.com/91405382/187443647-faf7f276-e39b-4234-ab0c-4f5aa975a909.png">
+ ### Phase2

  ###### 개발 목표: pipeline을 통한 command(ls | grep " ") 입력에 대해 처리하도록 구현(foreground process)
  
  
  ###### 실행 결과
  
  <img width="450" alt="phase2_run" src="https://user-images.githubusercontent.com/91405382/187445271-f8aaa539-678f-4135-a7c5-67e08384d8a6.png">

+ ### Phase3

  ###### 개발 목표: Phase1, Phase2에서 구현한 내용을 background process로 처리할 수 있도록 구현


  ###### 실행 결과
  
  <img width="600" alt="phase3_run" src="https://user-images.githubusercontent.com/91405382/187445826-105472b6-929a-4cef-b14d-d73c4d730677.png">

