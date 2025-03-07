#!/env/python
# Adapted from https://github.com/ilsani/afl-pytmin/tree/master
# ALL RIGHTS RESERVED TO THE ORIGINAL AUTHOR
#
# python afl-pytmin.py <input-dir> <output-dir> <harness-bin>
#

import sys
import os
import subprocess

from queue import Queue
from threading import Thread

AFL_TMIN_CMD = "afl-tmin"

def tmin(workerid, queue, output_dirname, harness_bin):

    while True:

        data = queue.get()
        filename = data["filename"]
        counter = data["counter"]

        output_path = "%s/%d" %(output_dirname, counter)

        args = [
            AFL_TMIN_CMD,
            "-i",
            filename,
            "-o",
            output_path,
            "-t",
            "3000",
            "-m",
            "4096",
            "--",
            harness_bin,
            "@@"
        ]

        print("[i] Processing %s file ... " %(filename))

        with open(os.devnull, 'w') as devnull:
            p = subprocess.Popen(args, stdout=devnull, stderr=devnull, shell=False)
            # p.communicate()
            p.wait()

        # p.stdout.close()

        queue.task_done()

def read_filename(input_dirname):

    for f in os.listdir(input_dirname):
        yield ("%s/%s" %(input_dirname, f)).strip()

def main():

    try:

        n_cores = 5
        input_dirname = sys.argv[1]
        output_dirname = sys.argv[2]
        harness_bin = sys.argv[3]

        if not os.path.exists(output_dirname):
            os.makedirs(output_dirname)

        queue = Queue()

        # Set up some threads
        for i in range(n_cores):
            worker = Thread(target=tmin, args=(i, queue, output_dirname, harness_bin, ))
            worker.setDaemon(True)
            worker.start()

        # Fill the queue
        counter = 0
        for filename in read_filename(input_dirname):

            data = { "filename": filename,
                     "counter": counter }

            queue.put(data)
            counter = counter + 1

        queue.join()

    except Exception as e:
        print("[!] Error: %s" %(str(e)))

if __name__ == "__main__":
    main()