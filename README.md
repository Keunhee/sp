# OctaFlip 게임

OctaFlip은 두 명의 플레이어가 번갈아 가며 말을 이동시키는 보드 게임입니다. 각 플레이어는 자신의 색상(빨간색 또는 파란색)의 말을 가지고 있으며, 이동 후에는 인접한 상대 말을 자신의 색으로 변경합니다.

## 게임 규칙

1. 8x8 보드에서 플레이어는 자신의 말을 1칸 또는 2칸 이동할 수 있습니다.
2. 이동은 상하좌우 및 대각선 방향으로 가능합니다.
3. 2칸 이동 시 중간 칸이 비어있어야 합니다.
4. 이동 후 인접한 상대 말은 모두 자신의 색으로 변경됩니다.
5. 유효한 이동이 없으면 패스해야 합니다.
6. 게임은 다음 조건 중 하나를 만족하면 종료됩니다:
   - 한 플레이어의 말이 모두 사라진 경우
   - 보드가 가득 찬 경우
   - 연속으로 두 번 패스한 경우
7. 게임 종료 시 말이 많은 플레이어가 승리합니다.

## 실행 방법

### 서버 실행

```bash
./server
```

서버는 기본적으로 포트 8888에서 실행됩니다.

### 클라이언트 실행

```bash
./client -ip <IP주소> -port <포트> -username <사용자명> [-led]
```

예시:
```bash
./client -ip 127.0.0.1 -port 8888 -username Player1
```

### 옵션

- `-ip`: 서버 IP 주소 (기본값: 127.0.0.1)
- `-port`: 서버 포트 (기본값: 8888)
- `-username`: 플레이어 이름
- `-led`: LED 매트릭스 사용 (옵션)

### 테스트 실행

간편한 테스트를 위해 다음 스크립트를 실행할 수 있습니다:

```bash
./run_test.sh        # 간단한 실행 테스트
./run_full_test.sh   # 전체 기능 테스트
./demo.sh            # 데모 실행
```

`run_test.sh` 스크립트는 서버와 두 클라이언트(Player1, Player2)를 자동으로 실행합니다.
`demo.sh` 스크립트는 서버를 시작하고 클라이언트 실행 방법을 안내합니다.

## 프로젝트 구조

- `octaflip.c/h`: 게임 로직 및 보드 관리
- `server.c`: 서버 구현
- `client.c`: 클라이언트 구현
- `json.c/h`: JSON 파싱 및 생성 라이브러리
- `message_handler.c/h`: 메시지 생성 및 파싱
- `led_matrix.c/h`: LED 매트릭스 제어

## 컴파일 방법

```bash
make clean
make
```

## 주의사항

- LED 매트릭스 기능은 하드웨어가 없을 경우 시뮬레이션 모드로 실행됩니다.
- 각 턴의 제한 시간은 5초입니다. 제한 시간 내에 이동하지 않으면 자동으로 패스됩니다.

## 추가 문서

- `DOCUMENTATION.md`: 프로젝트에 대한 상세 설명서
- `CHECKLIST.md`: 프로젝트 기능 및 품질 점검표