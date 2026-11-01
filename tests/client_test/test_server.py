
import socket
import threading
import sys

HOST = sys.argv[1] if len(sys.argv) > 1 else "0.0.0.0"
PORT = 8080
BUFFER_SIZE = 65535


def handle_tcp_client(conn, addr):
    print(f"[TCP] Connected: {addr}")

    with conn:
        while True:
            while True:
                try:
                    data = conn.recv(BUFFER_SIZE)

                    if not data:
                        break

                    print(f"[TCP] {addr}: {data!r}")
                    conn.sendall(data)

                except OSError as e:
                    print(f"[TCP] Error: {e}")
                    break

            break

    print(f"[TCP] Disconnected: {addr}")


def tcp_server():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind((HOST, PORT))
        sock.listen()

        print(f"[TCP] Listening on {HOST}:{PORT}")

        while True:
            conn, addr = sock.accept()

            threading.Thread(
                target=handle_tcp_client,
                args=(conn, addr),
                daemon=True
            ).start()


def udp_server():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind((HOST, PORT))

        print(f"[UDP] Listening on {HOST}:{PORT}")

        while True:
            data, addr = sock.recvfrom(BUFFER_SIZE)

            print(f"[UDP] {addr}: {data!r}")
            sock.sendto(data, addr)


if __name__ == "__main__":
    threading.Thread(target=tcp_server, daemon=True).start()
    udp_server()
