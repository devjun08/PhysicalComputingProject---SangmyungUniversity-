#!/usr/bin/env python3
"""
check_comm.py - 레이저 사격 시스템 통신 확인용 (MATLAB 없이 Python으로)

  설치 :  pip install pyserial
  실행 :  python check_comm.py            -> 포트 목록 보여줌
          python check_comm.py COM5       -> COM5 연결 (맥: /dev/cu.usbmodemXXXX)
          python check_comm.py COM5 --count 1000   -> 1000프레임 받고 무손실률 판정 후 종료
          python check_comm.py COM5 --log hits.csv -> 피격 기록 CSV 저장

  실행 중 키보드:  S + Enter (게임 시작), E + Enter (종료), 1~5 + Enter (속도), Q + Enter (끝내기)

  프레임 (27바이트, 10ms마다):
    A5 | Seq | State | HitCnt | Zone | RT_H RT_L | CH1_H CH1_L ... CH9_H CH9_L | CRC8 | 5A
"""
import argparse
import csv
import sys
import threading
import time
from datetime import datetime

HEAD, TAIL, LEN = 0xA5, 0x5A, 27
STATE_NAMES = ["IDLE", "PATROL", "HIT", "DOWN", "RECOVER", "GAME OVER"]
VALID_CMDS = {"S", "E", "1", "2", "3", "4", "5"}


def crc8(data):
    """CRC-8, poly 0x07, init 0x00 - 펌웨어 Proto_CRC8()과 동일"""
    crc = 0
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x07) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def parse_frames(buf):
    """bytearray buf에서 완성된 프레임을 모두 꺼냄. (frames, bad) 반환, buf는 제자리에서 줄어듦"""
    frames, bad = [], 0
    while True:
        k = buf.find(HEAD)
        if k < 0:
            buf.clear()
            break
        del buf[:k]
        if len(buf) < LEN:
            break
        f = buf[:LEN]
        if f[26] == TAIL and crc8(f[1:25]) == f[25]:
            frames.append({
                "seq": f[1], "state": f[2], "hit_cnt": f[3], "zone": f[4],
                "rt": (f[5] << 8) | f[6],
                "adc": [(f[7 + 2 * i] << 8) | f[8 + 2 * i] for i in range(9)],
            })
            del buf[:LEN]
        else:
            bad += 1
            del buf[:1]
    return frames, bad


def list_ports():
    from serial.tools import list_ports as lp
    ports = list(lp.comports())
    if not ports:
        print("연결된 시리얼 포트가 없습니다. USB 케이블(ST-LINK 쪽)을 확인하세요.")
    for p in ports:
        print(f"  {p.device:20s} {p.description}")
    print("\n사용법: python check_comm.py <포트>   예) python check_comm.py COM5")


def keyboard_thread(ser, stop):
    """Enter로 끝나는 한 글자 명령을 MCU로 전송"""
    while not stop.is_set():
        try:
            line = input().strip().upper()
        except EOFError:
            return
        if line == "Q":
            stop.set()
            return
        if line in VALID_CMDS:
            ser.write(line.encode("ascii"))
            print(f"  >> 명령 전송: '{line}'")
        elif line:
            print("  (명령은 S, E, 1~5, Q 중 하나)")


def main():
    ap = argparse.ArgumentParser(description="레이저 사격 통신 확인")
    ap.add_argument("port", nargs="?", help="COM5, /dev/cu.usbmodem1103 등")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--count", type=int, default=0, help="이만큼 받으면 무손실률 판정 후 종료")
    ap.add_argument("--log", help="피격 기록을 저장할 CSV 파일")
    args = ap.parse_args()

    try:
        import serial
    except ImportError:
        print("pyserial이 없습니다:  pip install pyserial")
        return 1

    if not args.port:
        list_ports()
        return 0

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.05)
    except serial.SerialException as e:
        print(f"포트를 열 수 없습니다: {e}")
        print("-> 포트 이름 확인, PuTTY/ComPortMaster/MATLAB 등 다른 프로그램이 포트를 쓰고 있지 않은지 확인")
        return 1
    ser.reset_input_buffer()

    log_f = log_w = None
    if args.log:
        log_f = open(args.log, "w", newline="", encoding="utf-8")
        log_w = csv.writer(log_f)
        log_w.writerow(["time", "seq", "hit_no", "zone", "reaction_ms"])

    stop = threading.Event()
    threading.Thread(target=keyboard_thread, args=(ser, stop), daemon=True).start()

    print(f"{args.port} @ {args.baud}bps 연결됨.  명령: S/E/1~5 + Enter, 끝내기: Q + Enter (또는 Ctrl+C)")
    print("1초마다 상태를 출력합니다. 정상이면 '초당' 약 100, CRC오류·유실 0\n")

    buf = bytearray()
    total = bad = lost = hits = 0
    last_seq = last_hit = None
    sec_count, t_print, t_start = 0, time.time(), time.time()
    last = None

    try:
        while not stop.is_set():
            data = ser.read(max(1, ser.in_waiting))
            if data:
                buf.extend(data)
                frames, b = parse_frames(buf)
                bad += b
                for f in frames:
                    total += 1
                    sec_count += 1
                    if last_seq is not None:
                        lost += (f["seq"] - last_seq - 1) % 256
                    last_seq = f["seq"]
                    if last_hit is None:
                        last_hit = f["hit_cnt"]          # 첫 프레임은 기준값
                    elif f["hit_cnt"] != last_hit:
                        last_hit = f["hit_cnt"]
                        hits += 1
                        print(f"  ** 피격! 구역 {f['zone']}  반응속도 {f['rt']} ms  (누적 {hits})")
                        if log_w:
                            log_w.writerow([datetime.now().isoformat(timespec="milliseconds"),
                                            f["seq"], hits, f["zone"], f["rt"]])
                            log_f.flush()
                    last = f
                    if args.count and total >= args.count:
                        stop.set()
                        break

            now = time.time()
            if now - t_print >= 1.0:
                if last:
                    st = STATE_NAMES[last["state"]] if last["state"] < len(STATE_NAMES) else f"?{last['state']}"
                    adc = " ".join(f"{v:4d}" for v in last["adc"])
                    print(f"[{now - t_start:6.1f}s] 초당 {sec_count:3d} | 누적 {total:6d} | CRC오류 {bad} | 유실 {lost} "
                          f"| 상태 {st:9s} | ADC {adc}")
                else:
                    print(f"[{now - t_start:6.1f}s] 수신 프레임 없음 (받은 바이트 {len(buf)}) "
                          f"-> 포트/보드 리셋/SetSysClock 주석 처리 확인")
                sec_count, t_print = 0, now
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        ser.close()
        if log_f:
            log_f.close()

    expected = total + lost
    print("\n===== 결과 =====")
    print(f"정상 수신 {total} / 유실 {lost} / CRC오류 {bad} / 피격 {hits}")
    if expected:
        rate = 100.0 * total / expected
        print(f"무손실률 {rate:.2f}%  (목표 99.9% 이상 -> {'통과' if rate >= 99.9 else '미달'})")
    if args.log:
        print(f"피격 기록 저장: {args.log}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
