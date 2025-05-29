# 서버 동작 검증용 환경 세팅 (Linux 환경이 아닐 경우)
개발환경(Visual Studio Code 등등)에서 docker 확장기능을 지원해야 합니다. 
1. https://www.docker.com/에서 Docker Desktop을 설치합니다.
2. 개발환경에 docker extension을 설치합니다.
3. 아래 링크를 참조하여 docker 터미널에서 netcat을 설치합니다. (sudo apt-get install netcat) 
https://jost-do-it.tistory.com/entry/Docker-%EC%BB%A8%ED%85%8C%EC%9D%B4%EB%84%88-%EB%82%B4%EC%97%90%EC%84%9C-apt-get-install-%EC%8B%9C-Unable-to-loacate-package-%ED%8C%A8%ED%82%A4%EC%A7%80%EB%AA%85-%ED%95%B4%EA%B2%B0%EB%B0%A9%EB%B2%95
4. docker 터미널을 서버용 1개, 클라이언트용 2개해서 총 3개 실행하고 아래 시나리오를 줄별로 실행하면 됩니다. (클라이언트의 경우 각 줄을 각 클라이언트가 번갈아가며 실행)

# 정상 동작 검증 시나리오

*Server commands

./server

*client 1 commands

nc 0.0.0.0 8888
{"type":"register","username":"Alice"},
{"type":"move","username":"Alice","sx":1,"sy":1,"tx":3,"ty":3},
{"type":"move","username":"Alice","sx":3,"sy":3,"tx":4,"ty":4},
{"type":"move","username":"Alice","sx":4,"sy":4,"tx":5,"ty":5},
{"type":"move","username":"Alice","sx":5,"sy":5,"tx":6,"ty":6},
{"type":"move","username":"Alice","sx":6,"sy":6,"tx":7,"ty":7}

*client 2 commands

nc 0.0.0.0 8888
{"type":"register","username":"Bob"},
{"type":"move","username":"Bob","sx":8,"sy":1,"tx":7,"ty":2},
{"type":"move","username":"Bob","sx":7,"sy":2,"tx":6,"ty":3},
{"type":"move","username":"Bob","sx":6,"sy":3,"tx":5,"ty":4},
{"type":"move","username":"Bob","sx":5,"sy":4,"tx":4,"ty":3},
{"type":"move","username":"Bob","sx":4,"sy":3,"tx":3,"ty":2}
