# 서버 동작 검증용 환경 세팅 (Linux 환경이 아닐 경우)
개발환경(Visual Studio Code 등등)에서 docker 확장기능을 지원해야 합니다. 
1. https://www.docker.com/에서 Docker Desktop을 설치합니다.
2. 개발환경에 docker extension을 설치합니다.
3. 아래 링크를 참조하여 docker 터미널에서 netcat을 설치합니다. (sudo apt-get install netcat) 
https://jost-do-it.tistory.com/entry/Docker-%EC%BB%A8%ED%85%8C%EC%9D%B4%EB%84%88-%EB%82%B4%EC%97%90%EC%84%9C-apt-get-install-%EC%8B%9C-Unable-to-loacate-package-%ED%8C%A8%ED%82%A4%EC%A7%80%EB%AA%85-%ED%95%B4%EA%B2%B0%EB%B0%A9%EB%B2%95
4. docker 터미널을 서버용 1개, 클라이언트용 2개해서 총 3개 실행하고 아래 시나리오를 줄별로 실행하면 됩니다. (클라이언트의 경우 각 줄을 각 클라이언트가 번갈아가며 실행)

# 정상 동작 검증 시나리오

 * Server commands

./server

 * Client1 commands

chmod 755 1.sh
./1.sh

 * Client2 commands

chmod 755 2.sh
./2.sh

client1의 명령어를 먼저 시작하고 
2초 정도 후에 client2에서도 명령어를 바로 입력하는 것이 권장됩니다.
