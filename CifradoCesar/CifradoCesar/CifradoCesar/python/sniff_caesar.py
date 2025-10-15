from scapy.all import sniff, TCP, Raw, IP
import sys

# Descifrado César: letras rotan 26, dígitos rotan 10
def dec_caesar(shift, data: bytes) -> str:
    s = shift % 26
    sd = (shift % 10)
    inv = (26 - s) % 26
    invd = (10 - sd) % 10
    out = []
    for b in data:
        c = chr(b)
        if 'a' <= c <= 'z':
            out.append(chr(((ord(c)-97 + inv) % 26) + 97))
        elif 'A' <= c <= 'Z':
            out.append(chr(((ord(c)-65 + inv) % 26) + 65))
        elif '0' <= c <= '9':
            out.append(chr(((ord(c)-48 + invd) % 10) + 48))
        else:
            out.append(c)
    return ''.join(out)

def handle(pkt):
    if pkt.haslayer(TCP) and pkt.haslayer(Raw) and pkt.haslayer(IP):
        payload = bytes(pkt[Raw].load)
        if len(payload) >= 2:  # esperamos [shift][cipher...]
            shift = payload[0]
            plain = dec_caesar(shift, payload[1:])
            print(f"{pkt[IP].src}:{pkt[TCP].sport} -> {pkt[IP].dst}:{pkt[TCP].dport} | "
                  f"len={len(payload)} | shift={shift} | plaintext='{plain}'")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Uso: sudo python3 sniff_caesar.py <iface> [puerto]")
        sys.exit(1)
    iface = sys.argv[1]
    port = sys.argv[2] if len(sys.argv) > 2 else "3333"
    bpf = f"tcp port {port}"
    print(f"Sniffeando en iface={iface} filtro='{bpf}' ... Ctrl-C para salir")
    sniff(iface=iface, filter=bpf, prn=handle, store=False)
