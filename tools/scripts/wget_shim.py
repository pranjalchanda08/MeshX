#!/usr/bin/env python3
"""
    MeshX wget shim for Windows host environments
    Provides a portable, zero-external-dependency drop-in replacement for wget
    so that scripts like Zephyr SDK setup.cmd execute seamlessly.
"""
import sys
import os
import urllib.request

def main():
    args = sys.argv[1:]
    if not args:
        print("wget shim: missing URL or parameters")
        return 0

    url = None
    output_path = None
    quiet = False

    i = 0
    while i < len(args):
        arg = args[i]
        if arg in ("-O", "--output-document"):
            i += 1
            if i < len(args):
                output_path = args[i]
        elif arg.startswith("-O"):
            output_path = arg[2:]
        elif arg in ("-q", "--quiet"):
            quiet = True
        elif arg in ("-V", "--version"):
            print("GNU Wget 1.21.4 (MeshX portable shim)")
            return 0
        elif arg.startswith("-"):
            # Skip unsupported flags like -c, -t, etc.
            pass
        else:
            url = arg
        i += 1

    if not url:
        return 0

    if not output_path:
        output_path = os.path.basename(url.split("?")[0])
        if not output_path:
            output_path = "download.bin"

    if not quiet:
        print(f">> Downloading: {url} -> {output_path}")

    try:
        def reporthook(block_num, block_size, total_size):
            if not quiet and total_size > 0:
                percent = min(100.0, block_num * block_size / total_size * 100)
                sys.stdout.write(f"\rProgress: {percent:.1f}%")
                sys.stdout.flush()

        urllib.request.urlretrieve(url, output_path, reporthook=reporthook if not quiet else None)
        if not quiet:
            print("\n>> Download complete.")
        return 0
    except Exception as e:
        print(f"\n[ERROR] Download failed: {e}", file=sys.stderr)
        return 1

if __name__ == "__main__":
    sys.exit(main())
