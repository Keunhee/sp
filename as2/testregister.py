#!/usr/bin/env python3
import socket
import json

SERVER_HOST = '127.0.0.1'
SERVER_PORT = 8888

USER1 = 'Alice'
USER2 = 'Bob'

def recv_until_newline(sock, timeout=2.0):
    sock.settimeout(timeout)
    data = b''
    while True:
        chunk = sock.recv(1024)
        if not chunk:
            break
        data += chunk
        if b'\n' in data:
            line, _, remainder = data.partition(b'\n')
            return line.decode('utf-8')
    return data.decode('utf-8')

def make_register_msg(username):
    return json.dumps({'type': 'register', 'username': username}) + '\n'

def wait_for_type(sock, expected_type):
    """
    recv_until_newline를 반복 호출하여 
    JSON이 파싱되고 그 'type'이 expected_type일 때까지 계속 읽음.
    """
    while True:
        raw = recv_until_newline(sock)
        if not raw:
            raise AssertionError(f"서버로부터 메시지를 전혀 받지 못했습니다. 기대 타입: {expected_type}")
        try:
            obj = json.loads(raw)
        except json.JSONDecodeError:
            # JSON 파싱 실패 시 무시하고 계속 읽기
            continue
        msg_type = obj.get('type')
        if msg_type == expected_type:
            return obj
        # 만약 다른 타입이 왔으면 무시하고 계속 수신
        # (예: 'your_turn'이나 'invalid_move' 등)
        # print(f"[디버그] 무시된 메시지 타입: {msg_type}")
        continue

def test_register_and_game_start():
    # --- 클라이언트 1 연결 및 REGISTER → ACK 수신 ---
    sock1 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock1.connect((SERVER_HOST, SERVER_PORT))
    sock1.sendall(make_register_msg(USER1).encode('utf-8'))
    ack1 = recv_until_newline(sock1)
    obj1 = json.loads(ack1)
    assert obj1.get('type') == 'register_ack', f"첫 번째 REGISTER_ACK 실패: {obj1}"

    # --- 클라이언트 2 연결 및 REGISTER → ACK 수신 ---
    sock2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock2.connect((SERVER_HOST, SERVER_PORT))
    sock2.sendall(make_register_msg(USER2).encode('utf-8'))
    ack2 = recv_until_newline(sock2)
    obj2 = json.loads(ack2)
    assert obj2.get('type') == 'register_ack', f"두 번째 REGISTER_ACK 실패: {obj2}"

    # --- 두 클라이언트가 모두 연결되었으므로 반드시 game_start가 온다 ---
    msg1 = wait_for_type(sock1, 'game_start')
    msg2 = wait_for_type(sock2, 'game_start')

    # players 필드 검증
    players1 = msg1.get('players', [])
    players2 = msg2.get('players', [])
    assert isinstance(players1, list) and USER1 in players1 and USER2 in players1, \
        f"game_start players 정보 오류(소켓1): {players1}"
    assert isinstance(players2, list) and USER1 in players2 and USER2 in players2, \
        f"game_start players 정보 오류(소켓2): {players2}"

    print("✅ test_register_and_start.py (수정판): OK")

    sock1.close()
    sock2.close()


if __name__ == '__main__':
    test_register_and_game_start()