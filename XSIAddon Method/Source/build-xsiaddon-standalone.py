#!/usr/bin/env python3
"""Build XSISelectFix.xsiaddon without launching Autodesk Softimage.

The container format follows known-good Softimage 2015 add-ons: XML metadata,
with each file stored as a 12-byte decimal-size header plus zlib data, then
Base64 encoded.  A verification pass decodes every item before success.
"""

from __future__ import print_function

import argparse
import base64
import os
import sys
import textwrap
import zlib
from xml.etree import ElementTree


ITEMS = (
    ("XSISelectFix.dll", r"\Application\bin\XSISelectFix",
     os.path.join("build", "Release", "XSISelectFix.dll")),
    ("XSISelectFix.enable", r"\Application\bin\XSISelectFix",
     os.path.join("build", "Release", "XSISelectFix.enable")),
    ("XSISelectFixLoader.py", r"\Application\Plugins",
     os.path.join("startup", "XSISelectFixLoader.py")),
)


def payload_for(data):
    size_header = str(len(data)).encode("ascii").ljust(12, b"\0")
    encoded = base64.b64encode(size_header + zlib.compress(data, 9))
    if not isinstance(encoded, str):
        encoded = encoded.decode("ascii")
    return "\n".join(textwrap.wrap(encoded, 64))


def decode_payload(text):
    packed = base64.b64decode("".join(text.split()))
    expected_size = int(packed[:12].rstrip(b"\0").decode("ascii"))
    data = zlib.decompress(packed[12:])
    if len(data) != expected_size:
        raise ValueError("decoded size %d does not match header %d" %
                         (len(data), expected_size))
    return data


def xml_escape(value):
    return (value.replace("&", "&amp;").replace("<", "&lt;")
            .replace(">", "&gt;").replace('"', "&quot;"))


def build_document(source_root):
    metadata = (
        ("AddonVersion", "2.0"), ("Categories", ""),
        ("Nature", "Auxiliary"), ("PlatForms", "Windows x64"),
        ("Title", "XSISelectFix"), ("AuthorName", "XSISelectFix"),
        ("AuthorEMail", ""), ("Company", ""), ("URL", ""),
        ("Language", ""),
        ("Summary", "Fixes Intel OpenGL transform-gizmo picking in Softimage 2015."),
        ("Description", "Loads a process-local OpenGL selection compatibility fix without replacing Autodesk or Windows files."),
        ("Usage", "Install, restart Softimage, and use transform manipulators normally."),
        ("Image", ""), ("InstallDir", "XSISelectFix"),
        ("DefaultDestination", "5"),
    )

    lines = ['<?xml version="1.0" encoding="iso-8859-1"?>', "",
             '<xsi_file type="Addon" xsi_version="13.0.114.0" syntax_version="1.0">']
    for name, value in metadata:
        lines.append("   <%s>%s</%s>" % (name, xml_escape(value), name))

    originals = {}
    for item_id, (filename, destination, relative_path) in enumerate(ITEMS, 1):
        path = os.path.join(source_root, relative_path)
        if not os.path.isfile(path):
            raise IOError("required file was not found: " + path)
        with open(path, "rb") as source:
            data = source.read()
        originals[(filename, destination)] = data
        description = "XSISelectFix\\" + relative_path.replace(os.sep, "\\")
        lines.append(
            '   <AddonItem filename="%s" destination="%s" type="Others" '
            'description="%s" mode="438" id="%d">' %
            (xml_escape(filename), xml_escape(destination),
             xml_escape(description), item_id)
        )
        lines.extend(payload_for(data).splitlines())
        lines.append("</AddonItem>")
    lines.append("</xsi_file>")
    return ("\r\n".join(lines) + "\r\n").encode("iso-8859-1"), originals


def verify_document(document, originals):
    root = ElementTree.fromstring(document)
    if root.tag != "xsi_file" or root.get("type") != "Addon":
        raise ValueError("invalid add-on root element")
    seen = set()
    for item in root.findall("AddonItem"):
        key = (item.get("filename"), item.get("destination"))
        if key not in originals:
            raise ValueError("unexpected add-on item: %r" % (key,))
        if item.get("type") != "Others" or item.get("mode") != "438":
            raise ValueError("invalid item attributes: %r" % (key,))
        if decode_payload(item.text or "") != originals[key]:
            raise ValueError("round-trip mismatch: %r" % (key,))
        seen.add(key)
    if seen != set(originals):
        raise ValueError("one or more package items are missing")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", default=os.path.dirname(os.path.abspath(__file__)))
    parser.add_argument("--output", help="output .xsiaddon path")
    args = parser.parse_args()

    source_root = os.path.abspath(args.source_root)
    output = os.path.abspath(args.output or os.path.join(
        source_root, "dist", "XSISelectFix.xsiaddon"))
    document, originals = build_document(source_root)
    verify_document(document, originals)
    output_dir = os.path.dirname(output)
    if not os.path.isdir(output_dir):
        os.makedirs(output_dir)
    with open(output, "wb") as target:
        target.write(document)
    print("Created and verified: " + output)
    print("Embedded files: %d" % len(originals))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as error:
        print("XSISelectFix .xsiaddon build failed: " + str(error), file=sys.stderr)
        sys.exit(1)
