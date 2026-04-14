print("xorshift taps: 7, -9, 13")

def lfsr_xs(lfsr):
    lfsr ^= lfsr >> 7
    lfsr ^= (lfsr << 9) & 0xffff
    lfsr ^= lfsr >> 13
    return lfsr

def print_xorshift_seq(start, len):
    seq = [start]
    lfsr = start
    print(f"0: {hex(lfsr)} {bin(lfsr)}")

    for i in range(len - 1):
        lfsr = lfsr_xs(lfsr)
        seq.append(lfsr)
        print(f"{i + 1}: {hex(lfsr)} {bin(lfsr)}")

    return seq

sync_start_state = 0xac92
sync_len = 16

print("sync lfsr xorshift sequence:\n")
sync_seq = print_xorshift_seq(sync_start_state, sync_len)

print(f"\nstatic const uint16_t sync_seq[{sync_len}] = {{", end="")
for i in sync_seq:
    print(f"{hex(i)}, ", end="")
print("};")

print(f"\nstart_symbol: {hex(sync_start_state)} end_symbol: {hex(sync_seq[-1])}")
