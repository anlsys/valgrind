#!/bin/python3

import os

f = open("list.txt", "r")
for line in f:
    if line.strip().startswith("#"):
        continue
    test = line.split()
    if test == None or len(test) == 0:
        continue
    if len(test) != 2:
        print("Warning, line misformatted: {}".format(line))
        continue

    source = test[0]
    expect = test[1]

    cmd = "clang -g -fopenmp -Wall -Werror -Wextra {} -o {}.exe".format(source, source)
    os.system(cmd)

    # cmd = "../../../vg-in-place --tool=taskgrind --ignorelist ./{}.exe".format(source)
    # os.popen(cmd).read()
