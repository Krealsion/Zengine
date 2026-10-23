# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Joshua DeMoss
"""The package's archive, made the same from the same files: a zip (Windows) or a tar.gz (Linux)
of one folder, entries sorted, every time the commit's, no owner, no host.

    python -I pack.py <folder> <archive .zip or .tar.gz> <epoch seconds>

The archive holds the folder itself as its one top-level entry. A Linux file keeps its mode, so a
launcher stays executable; a link is kept as a link. Exit 0 with the archive written, 2 on a usage
error or an empty folder.
"""
import gzip
import io
import os
import stat
import sys
import tarfile
import time
import zipfile


def entries(folder):
    base = os.path.dirname(os.path.abspath(folder))
    out = []
    for d, dirs, files in os.walk(folder):
        dirs.sort()
        for name in sorted(dirs) + sorted(files):
            full = os.path.join(d, name)
            out.append((os.path.relpath(full, base).replace(os.sep, "/"), full))
    out.append((os.path.basename(os.path.abspath(folder)), os.path.abspath(folder)))
    return sorted(out)


def make_zip(folder, archive, epoch):
    stamp = time.gmtime(max(epoch, 315532800))[:6]  # a zip's clock starts in 1980
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for rel, full in entries(folder):
            if os.path.isdir(full):
                info = zipfile.ZipInfo(rel + "/", stamp)
                info.external_attr = (0o40755 << 16) | 0x10
                z.writestr(info, b"")
            else:
                info = zipfile.ZipInfo(rel, stamp)
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = (0o100644 if not os.access(full, os.X_OK) else 0o100755) << 16
                with open(full, "rb") as fh:
                    z.writestr(info, fh.read())


def make_tar_gz(folder, archive, epoch):
    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode="w", format=tarfile.PAX_FORMAT) as t:
        for rel, full in entries(folder):
            info = t.gettarinfo(full, arcname=rel)
            info.mtime, info.uid, info.gid, info.uname, info.gname = epoch, 0, 0, "", ""
            info.mode = stat.S_IMODE(info.mode)
            info.pax_headers = {}
            if info.isfile():
                with open(full, "rb") as fh:
                    t.addfile(info, fh)
            else:
                t.addfile(info)
    with open(archive, "wb") as out:
        with gzip.GzipFile(filename="", mode="wb", fileobj=out, mtime=0, compresslevel=9) as gz:
            gz.write(raw.getvalue())


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    folder, archive, epoch = argv[0], argv[1], int(argv[2])
    if not os.path.isdir(folder) or not os.listdir(folder):
        print("pack: %s is no folder holding anything: an empty package is refused" % folder)
        return 2
    if archive.endswith(".zip"):
        make_zip(folder, archive, epoch)
    elif archive.endswith(".tar.gz"):
        make_tar_gz(folder, archive, epoch)
    else:
        print("pack: %s is neither a .zip nor a .tar.gz" % archive)
        return 2
    print("pack: %s, %d bytes" % (archive, os.path.getsize(archive)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
