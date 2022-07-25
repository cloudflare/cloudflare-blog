from socket import SOCK_STREAM, AF_INET, SOL_SOCKET, SO_REUSEADDR, SOL_TCP, TCP_MAXSEG, SO_RCVBUF, SO_SNDBUF, IPPROTO_IP, TCP_NODELAY, TCP_QUICKACK
import ctypes
import os
import socket
import struct
import sys
import time
import threading
import select

rcv_buf = int(sys.argv[1])
fill_mtd = sys.argv[2]
drain_mtd = sys.argv[3]
packet_size = int(sys.argv[4])

IP_BIND_ADDRESS_NO_PORT = 24
IP_MTU = 14


sd = socket.socket(AF_INET, SOCK_STREAM, 0)
sd.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
sd.setsockopt(SOL_TCP, TCP_MAXSEG, 1024)
sd.setsockopt(SOL_TCP, TCP_NODELAY, 1)
if rcv_buf:
    sd.setsockopt(SOL_SOCKET, SO_RCVBUF, rcv_buf)
sd.bind(('127.0.0.3', 1234))

sd.listen(32)

cd = socket.socket(AF_INET, SOCK_STREAM, 0)
cd.setsockopt(SOL_TCP, TCP_MAXSEG, packet_size)
cd.setsockopt(SOL_SOCKET, SO_SNDBUF, 1024*1024*4)
cd.setsockopt(SOL_TCP, TCP_NODELAY, 1)
cd.setsockopt(IPPROTO_IP, IP_BIND_ADDRESS_NO_PORT, 1)

cd.bind(('127.0.0.2', 1235))
cd.connect(('127.0.0.3', 1234))

# move to ESTAB state ASAP, since tcp_rcv_established is for ESTAB indeed
ssd, _ = sd.accept()
time.sleep(1)

done = False

waitinsec = 0.05

waitforfiller=threading.Semaphore()
waitforfiller.acquire()


def thread_fill(fill_mtd):
    (fill_mtd, _, payload_sz) = fill_mtd.partition('+')
    if fill_mtd == 'chunk':
        chunk_max_inflight = 10
    elif fill_mtd == 'sync':
        chunk_max_inflight = 0
        fill_mtd = 'chunk'
    else:
        chunk_max_inflight = None

    if not payload_sz:
        payload_sz = 1024
    else:
        if '+' in payload_sz:
            (payload_sz, _, chunk_max_inflight) = payload_sz.partition('+')
            chunk_max_inflight = int(chunk_max_inflight)
        payload_sz = int(payload_sz)

    burst_payload = b'a'* (16*1024*1024)
    chunk_payload = b'a' * payload_sz

    print("[+] fill=%s chunk_payload/inflight=%s/%s" % (fill_mtd, payload_sz, chunk_max_inflight))
    cd.setblocking(False)
    wff = waitforfiller
    # if fill_mtd == 'chunk':
    #     cd.send(b'a'*1024*80)
    while not done:
        try:
            if fill_mtd == 'burst':
                cd.send(burst_payload)
                #ssd.setsockopt(SOL_TCP, TCP_QUICKACK, 1)
                if wff:
                    wff.release()
                    wff = None
                # ti = cd.getsockopt(socket.SOL_TCP, socket.TCP_INFO, 512)
                # (_, inflight) = struct.unpack_from("24sh", ti)
                # print("inflight=%d\n", inflight)
                select.select([], [cd], [], waitinsec / 1000)
                continue
            elif fill_mtd == 'chunk':
                # cd.send(b'a'*1024*8)
                # cd.send(chunk_payload)
                # cd.send(chunk_payload)
                # cd.send(chunk_payload)
                cd.send(chunk_payload)
                ssd.setsockopt(SOL_TCP, TCP_QUICKACK, 1)
                inflight = sys.maxsize
                t0 = time.time()
                while inflight > chunk_max_inflight:
                    ti = cd.getsockopt(socket.SOL_TCP, socket.TCP_INFO, 512)
                    (_, inflight) = struct.unpack_from("24sh", ti)
                    if time.time() - t0 > 8:
                        print("[!] failed to sync inflight")
                        raise BlockingIOError
            else:
                raise "XXX"
            if wff:
                wff.release()
                wff = None
        except BlockingIOError:
            time.sleep(waitinsec)

def thread_drain(drain_mtd):
    waitforfiller.acquire()
    (drain_mtd, _, bytespersec) = drain_mtd.partition('+')
    if not bytespersec:
        bytespersec = 1024
    else:
        if bytespersec.endswith('Kbit'):
            bytespersec = int(bytespersec[:-4])*1000//8
        elif bytespersec.endswith('Mbit'):
            bytespersec = int(bytespersec[:-4])*1000*1000//8
        else:
            bytespersec = int(bytespersec)

    print("[+] drain=%s bytespersec=%s" % (drain_mtd, bytespersec))
    bytesperiter = int(bytespersec * waitinsec)
    ssd.setblocking(False)
    offset = 0.
    # time.sleep(1)
    # ssd.recv(bytespersec)
    i = 0
    while not done:
        wanted = waitinsec - offset
        t0 = time.time()
        if drain_mtd == 'none':
            pass
        elif drain_mtd == 'cont':
            try:
                ssd.recv(bytesperiter)
            except BlockingIOError:
                pass
        else:
            raise "XXX"
        time.sleep(wanted)
        td = time.time()-t0
        offset = td - wanted
        if False: # staircase rbuf
            i += 1
            print(i)
            ssd.setsockopt(SOL_SOCKET, SO_RCVBUF, i * 64*1024)
            try:
                ssd.setsockopt(SOL_TCP, socket.TCP_WINDOW_CLAMP, b'\xff\xff\xff\x7f')
            except:
                pass



fill_thrd = threading.Thread(target=thread_fill, args=(fill_mtd,))
fill_thrd.start()
drain_thrd = threading.Thread(target=thread_drain, args=(drain_mtd,))
drain_thrd.start()
try:
    time.sleep(5)
except KeyboardInterrupt:
    print("Ctrl+C pressed...")
done = True
fill_thrd.join()
drain_thrd.join()
print("[+] done")
os.system('ss -memoi sport = :1234 or dport = :1234|cat')
os.system('nstat -az TcpExtTCPRcvCollapsed TcpExtTCPRcvCoalesce TcpExtTCPRcvQDrop TcpExtTCPOFOQueue TcpExtTCPBacklogCoalesce')
