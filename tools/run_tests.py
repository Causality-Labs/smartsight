#!/usr/bin/env python3
import os
import sys

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS_DIR)

import minitest
import testbase

USAGE = """usage: run_tests.py [--device PATH] [--no-motion] [-k TEXT]... [--failfast]

  --device PATH   RPMsg character device (default /dev/rpmsg0)
  --no-motion     skip tests that move the servos
  -k TEXT         only run tests whose name contains TEXT (repeatable)
  --failfast      stop at the first failure
"""


def parse_args(argv):
    opts = {"device": None, "no_motion": False, "patterns": [], "failfast": False}
    i = 0

    while i < len(argv):
        arg = argv[i]

        if arg in ("-h", "--help"):
            print(USAGE)
            sys.exit(0)
        elif arg == "--no-motion":
            opts["no_motion"] = True
        elif arg == "--failfast":
            opts["failfast"] = True
        elif arg in ("--device", "-k"):
            i += 1
            if i >= len(argv):
                sys.exit(f"{arg} needs a value\n\n{USAGE}")
            if arg == "--device":
                opts["device"] = argv[i]
            else:
                opts["patterns"].append(argv[i])
        else:
            sys.exit(f"unknown argument {arg}\n\n{USAGE}")

        i += 1

    return opts


def main():
    opts = parse_args(sys.argv[1:])

    if opts["device"]:
        os.environ[testbase.DEVICE_ENV] = opts["device"]

    if opts["no_motion"]:
        os.environ[testbase.SKIP_MOTION_ENV] = "1"

    classes = minitest.discover(os.path.join(TOOLS_DIR, "tests"))
    ok = minitest.run(classes, opts["patterns"], opts["failfast"])

    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
