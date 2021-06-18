# HYU-Network-Protocol
2021년도 컴퓨터 네트워크 프로토콜 수업 과제를 위한 레퍼지터리입니다.

### Notes
익숙한 C 언어를 통해 구현하였고, 용이함을 위해 Project 4 는 C++ 을 통해 사용하였습니다.  

- MacOS BigSur (intel processor) 에서 개발하고, 테스트 하였습니다.
- vscode (prettier와 함께 사용하였고, 터미널 4개 5개 띄울때 편안함을 느꼈습니다)
- project 4 의 contents 를 있음직하게 만들기 위해, https://www.json-generator.com/ 사이트에서 생성하였습니다.
- 잘 쓰지는 못했지만, make 를 사용하였습니다.
- posix pthread 를 사용하여 개발하였습니다.

thread, mutex_lock, broadcast, conditional_wait 와 network & host byte 정렬이 어떻게 다른지, port 번호의 충돌은 어떻게 해결할 수 있는지 등에 대해 배울 수 있었습니다!  

# Projects
1. Socket 을 통해, client 측의 메세지를 server 에서 받고, 같은 메세지를 server 에서 client 로 보내 보여주는, 간단한 Echoer 구현
2. Multi Thread 를 이용해, 채팅방 기능을 할 수 있도록 구현
3. FTP 를 구현하여, 클라이언트에서 서버의 directory entry list 를 확인할 수 있고, 파일이름을 입력하면, 서버가 정해주거나(PASV 방식), 클라이언트에서 요청한(Active 방식) Port 를 통해 전달 받을수 있도록 구현
4. Pub Sub Middleware 를 구현하여, Publisher 가 Topic 과 함께 파일의 이름을 입력하면, 파일의 내용과 Topic 을 함께 Middleware(message broker) 에게 전달해 등록하고, Subscriber 가 하나또는 여러개의 원하는 Topic 을 입력하면, 해당 Topic 에 대해 등록된 모든 메시지(initial) 를 받아볼 수 있고, publisher 에 의해 구독중인 토픽의 다른 메세지가 추가로 등록되면, 추가로 받아볼 수 있도록 구현
