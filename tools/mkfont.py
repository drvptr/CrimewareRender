#!/usr/bin/env python3
#
#	@(#)mkfont.py	1.0
#
# Makes the font atlas gui_SetFont() wants: ASCII 32 to 127 laid out in a
# 16 by 6 grid, white glyphs on transparent, written as a 32 bpp TGA.
#
# The source is a PSF console font, because your machine already has a
# pile of them and the format is a header and a run of bitmaps:
#
#	python3 tools/mkfont.py /usr/share/consolefonts/Lat15-Terminus16.psf.gz \
#		data/font.tga
#
# Look in /usr/share/consolefonts (Debian, Ubuntu) or
# /usr/share/kbd/consolefonts (Arch, Fedora, and FreeBSD's ports of kbd).
#
# You do not have to use this.  Any picture with 96 glyphs in a 16 by 6
# grid works - draw one in GIMP if you would rather.  The engine only
# slices the texture; it has no opinion about how the letters got there.

import gzip
import struct
import sys


def read_font(path):
    """Returns (glyphs, width, height); glyphs are lists of row bitmaps."""
    if path.endswith(".gz"):
        data = gzip.open(path, "rb").read()
    else:
        data = open(path, "rb").read()

    if len(data) > 4 and data[0] == 0x36 and data[1] == 0x04:
        # PSF1: two byte magic, mode, charsize.  Always 8 pixels wide.
        charsize = data[3]
        count = 512 if (data[2] & 0x01) else 256
        width = 8
        height = charsize
        body = data[4:]
        stride = charsize
    elif len(data) > 32 and data[0:4] == b"\x72\xb5\x4a\x86":
        # PSF2: everything is a little endian 32 bit field.
        (headersize, flags, count, charsize, height,
         width) = struct.unpack("<6I", data[8:32])
        body = data[headersize:]
        stride = charsize
    else:
        raise SystemExit("mkfont: %s is not a PSF font" % path)

    bytes_per_row = (width + 7) // 8
    glyphs = []
    for i in range(count):
        start = i * stride
        rows = []
        for y in range(height):
            row = 0
            for b in range(bytes_per_row):
                at = start + y * bytes_per_row + b
                value = body[at] if at < len(body) else 0
                row = (row << 8) | value
            rows.append(row)
        glyphs.append(rows)

    return glyphs, width, height


def write_tga(path, pixels, width, height):
    """32 bpp uncompressed TGA, top down, BGRA on disk."""
    header = bytearray(18)
    header[2] = 2                       # uncompressed truecolour
    header[12] = width & 0xFF
    header[13] = (width >> 8) & 0xFF
    header[14] = height & 0xFF
    header[15] = (height >> 8) & 0xFF
    header[16] = 32                     # bits per pixel
    header[17] = 0x28                   # 8 alpha bits, origin top left

    with open(path, "wb") as f:
        f.write(header)
        f.write(pixels)


def main():
    if len(sys.argv) < 3:
        print(__doc__.strip())
        raise SystemExit(1)

    glyphs, gw, gh = read_font(sys.argv[1])
    atlas_w = gw * 16
    atlas_h = gh * 6
    pixels = bytearray(atlas_w * atlas_h * 4)

    for index in range(96):
        code = 32 + index
        if code >= len(glyphs):
            continue
        col = index % 16
        row = index // 16
        rows = glyphs[code]

        for y in range(gh):
            bits = rows[y]
            for x in range(gw):
                lit = (bits >> (gw - 1 - x)) & 1
                if not lit:
                    continue
                px = col * gw + x
                py = row * gh + y
                at = (py * atlas_w + px) * 4
                pixels[at + 0] = 255
                pixels[at + 1] = 255
                pixels[at + 2] = 255
                pixels[at + 3] = 255

    write_tga(sys.argv[2], bytes(pixels), atlas_w, atlas_h)
    print("mkfont: %s  %dx%d atlas, cells are %dx%d"
          % (sys.argv[2], atlas_w, atlas_h, gw, gh))
    print("mkfont: gui_SetFont(texture, %d, %d);" % (gw, gh))


main()
