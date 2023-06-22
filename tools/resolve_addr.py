# Usage:
#
# $ python3 resolve_addr.py sold.INFO /proc/27136/maps 0x7f41080088c9
#


import argparse
import re


def parse_log(log_filename):
    loads = []
    for line in open(log_filename):
        matched = re.search(r'\] PT_LOAD mapping: (.*)', line)
        if not matched:
            continue
        load = {}
        for tok in matched[1].split(" "):
            key, value = tok.split("=", 1)
            if key != 'name':
                value = int(value)
            load[key] = value
        loads.append(load)
    return loads


def parse_maps(maps_filename):
    base_addrs = {}
    maps = []
    for line in open(maps_filename):
        toks = line.split()
        if len(toks) == 6:
            addrs, perms, off, dev, inode, path = toks
        elif len(toks) == 5:
            addrs, perms, off, dev, inode = toks
            path = ''
        else:
            assert len(toks) == 7
            addrs, perms, off, t, inode, dev, msg = toks
            path = ''
        begin, end = [int(a, 16) for a in addrs.split('-')]
        if path not in base_addrs:
            base_addrs[path] = begin
        base = base_addrs[path]
        maps.append({
            'path': path,
            'addr': begin,
            'end': end,
            'size': end - begin,
            'base': base,
        })
    return maps


def show_addr(maps, addr, loads=None):
    print('Loaded address: %x' % addr)
    map_found = None
    for map in maps:
        if map['addr'] <= addr and addr < map['end']:
            map_found = map
            break
    else:
        print('Not found in maps')
        return

    print('Loaded path: %s' % map_found['path'])
    vaddr = addr - map['base']
    print('Virtual address before reloc: %x' % vaddr)

    for load in loads:
        if load['vaddr'] > vaddr or vaddr >= load['vaddr'] + load['memsz']:
            continue
        orig_vaddr = vaddr - load['vaddr'] + load['orig_vaddr']
        print('Address in %s: %x' % (load['name'], orig_vaddr))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('log', type=str, help="Path to the log file")
    parser.add_argument('maps', type=str, help="Path to the /proc/<pid>/maps file")
    parser.add_argument('addr', type=str, help="Address to be resolved")
    args = parser.parse_args()

    loads = parse_log(args.log)
    maps = parse_maps(args.maps)
    addr = eval(args.addr)

    show_addr(maps, addr, loads=loads)


if __name__ == "__main__":
    main()
