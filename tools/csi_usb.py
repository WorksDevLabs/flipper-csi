#!/usr/bin/env python3
"""CSI Radar USB console: setup, telemetry/raw recording, and simple commands.
Install: python -m pip install pyserial
Credentials are prompted; do not put passwords in shell arguments.
"""
import argparse
import binascii
import getpass
import time
from pathlib import Path


def frame(payload):
    raw = payload.encode("ascii")
    return b"$" + raw + f"*{binascii.crc_hqx(raw, 0xFFFF):04X}\n".encode()


def payload(line):
    if not line.startswith(b"$") or b"*" not in line:
        return None
    body, check = line.strip()[1:].rsplit(b"*", 1)
    try:
        if len(check) != 4 or int(check, 16) != binascii.crc_hqx(body, 0xFFFF):
            return None
        return body.decode("ascii")
    except (ValueError, UnicodeDecodeError):
        return None


def await_ack(port, command, timeout=5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        result = payload(port.readline(512))
        if result and result.startswith("ERR,"):
            raise RuntimeError(result)
        if result and (result == f"ACK,{command}" or result.startswith(f"ACK,{command},")):
            print(result)
            return
    raise TimeoutError("ESP32 yanıt vermedi. USB portunu ve firmware'i kontrol edin.")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("port", help="ESP32 USB CDC portu; örnek COM5 veya /dev/ttyACM0")
    sub = p.add_subparsers(dest="action", required=True)
    sub.add_parser("setup", help="Wi-Fi adı/şifresini sor ve ESP32'ye kaydet")
    sub.add_parser("ping")
    sub.add_parser("info", help="Show ESP firmware handshake")
    sub.add_parser("scan", help="List nearby Wi-Fi SSIDs")
    filt = sub.add_parser("filter", help="Classic/Robust motorunu seç")
    filt.add_argument("mode", choices=["classic", "robust"])
    sub.add_parser("save", help="Aktif eşiği ESP32'ye kaydet")
    cal = sub.add_parser("cal")
    cal.add_argument("--packets", type=int, default=100)
    threshold = sub.add_parser("threshold")
    threshold.add_argument("milli", type=int)
    record = sub.add_parser("record")
    record.add_argument("output", type=Path)
    record.add_argument("--seconds", type=float, default=60)
    record.add_argument("--raw", action="store_true", help="USB'de 10 Hz ham IQ kaydı")
    args = p.parse_args()
    if args.action == "cal" and not 20 <= args.packets <= 2000:
        p.error("packets 20..2000 olmalı")
    if args.action == "threshold" and not 5 <= args.milli <= 2000:
        p.error("milli 5..2000 olmalı")
    if args.action == "record" and args.seconds <= 0:
        p.error("seconds pozitif olmalı")
    command = args.action.upper()
    if args.action == "setup":
        ssid = input("Wi-Fi adı: ").encode("utf-8")
        password = getpass.getpass("Wi-Fi şifresi: ").encode("utf-8")
        if not 1 <= len(ssid) <= 32 or not 8 <= len(password) <= 63 or b"\0" in ssid+password:
            p.error("SSID 1..32, şifre 8..63 bayt olmalı")
        command = "WIFI " + ssid.hex() + " " + password.hex()
        del password
    elif args.action == "filter":
        command = "FILT " + ("1" if args.mode == "robust" else "0")
    elif args.action == "cal":
        command = f"CAL {args.packets}"
    elif args.action == "threshold":
        command = f"THR {args.milli}"
    import serial
    # Avoid reset/boot via DTR/RTS. Native USB CDC on the S2 is the target.
    port = serial.Serial()
    port.port, port.baudrate, port.timeout, port.write_timeout = args.port, 115200, 0.3, 2
    port.dtr = port.rts = False
    with port:
        port.reset_input_buffer()
        if args.action in ("info", "scan"):
            port.write(frame("PING" if args.action == "info" else "SCAN"))
            deadline = time.monotonic() + (5 if args.action == "info" else 12)
            while time.monotonic() < deadline:
                result = payload(port.readline(512))
                if not result:
                    continue
                if result.startswith("ERR,"):
                    raise RuntimeError(result)
                if args.action == "info" and result.startswith("HELLO,"):
                    print(result)
                    return
                if args.action == "scan" and result.startswith("AP,"):
                    fields = result.split(',')
                    if len(fields) == 6:
                        print(bytes.fromhex(fields[5]).decode('utf-8', errors='replace'), 'RSSI', fields[2], 'channel', fields[3])
                if args.action == "scan" and result.startswith("APEND,"):
                    return
            raise TimeoutError('No completed firmware/scan response')
        elif args.action != "record":
            port.write(frame(command))
            await_ack(port, command.split()[0])
        else:
            mode = "RAW 1" if args.raw else "RAW 0"
            # Refuse to overwrite an earlier capture.
            with args.output.open("xb") as out:
                out.write(b"#CSI_RAW_SCHEMA=1;RAW_MAX_HZ=10;TELEMETRY=CSI2\n")
                port.write(frame(mode))
                await_ack(port, "RAW")
                count = 0
                deadline = time.monotonic() + args.seconds
                try:
                    while time.monotonic() < deadline:
                        line = port.readline(512)
                        valid = payload(line)
                        if valid and valid.startswith("CSI2,"):
                            out.write(line); count += 1
                        elif args.raw and line.startswith(b"$RAW,") and line.endswith(b"\n"):
                            fields = line.strip().split(b",")
                            try:
                                if len(fields) == 5 and len(bytes.fromhex(fields[4].decode())) == 128:
                                    out.write(line)
                            except (ValueError, UnicodeDecodeError):
                                pass
                finally:
                    port.write(frame("RAW 0"))
            print(f"{count} doğrulanmış durum satırı kaydedildi: {args.output}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, TimeoutError) as error:
        raise SystemExit(str(error)) from error

