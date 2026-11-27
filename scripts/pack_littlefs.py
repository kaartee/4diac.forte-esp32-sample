import os
import sys
from littlefs import LittleFS


def main() -> int:
    args = sys.argv[1:]
    if len(args) != 3:
        print("Usage: python pack_littlefs.py <SRC_DIR> <OUT_BIN> <FS_SIZE>")
        return 1

    src_dir, out_bin, fs_size_str = args
    fs_size = int(fs_size_str, 0)

    block_size = 4096
    fs = LittleFS(
        block_size=block_size,
        block_count=fs_size // block_size,
        prog_size=16,
        read_size=16,
        cache_size=64,
        lookahead_size=32,
        block_cycles=512,
    )

    for root, _dirs, files in os.walk(src_dir):
        rel_root = os.path.relpath(root, src_dir)
        if rel_root != ".":
            fs.makedirs(rel_root.replace(os.sep, "/"), exist_ok=True)
        for fname in files:
            src_path = os.path.join(root, fname)
            dst_path = fname if rel_root == "." else f"{rel_root.replace(os.sep, '/')}/{fname}"
            with open(src_path, "rb") as f_in, fs.open(dst_path, "wb") as f_out:
                f_out.write(f_in.read())

    with open(out_bin, "wb") as f_img:
        f_img.write(fs.context.buffer)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
