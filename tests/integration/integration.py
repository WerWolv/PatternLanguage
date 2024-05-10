# Integration tests for the plcli CLI, and imhex --pl

import sys
import os
import subprocess
import tempfile
import shutil

if len(sys.argv) < 2:
    print("Usage: python integration.py <beginning of the plcli command>")
    exit(1)

# Check if command exists
if shutil.which(sys.argv[1]) is None:
    print(f"Command {sys.argv[1]} not found")
    exit(1)

beginning = sys.argv[1:]
def success_run(args: list) -> tuple[int, str, str]:
    cmd = [*beginning, *args]
    
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    
    if process.returncode != 0:
        print(f"Command {cmd} failed with exit code {process.returncode}")
        print(f"Stdout: {stdout}")
        print(f"Stderr: {stderr}")
        exit(1)
    return stdout

def failure_run(args: list) -> tuple[int, str, str]:
    cmd = [*beginning, *args]
    
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    if process.returncode == 0:
        print(f"Command {cmd} succeded although it should have failed")
        print(f"Stdout: {stdout}")
        print(f"Stderr: {stderr}")
        exit(1)
    elif process.returncode < 0: # Check for signal termination: not the way we want our process to fail
        print(f"Command {cmd} failed with signal termination exit code {process.returncode}")
        print(f"Stdout: {stdout}")
        print(f"Stderr: {stderr}")

with tempfile.TemporaryDirectory() as tmpdir:
    stdout = success_run(["format", "--input", "tests/integration/3ds.hexpat.3ds", "--pattern", "tests/integration/3ds.hexpat", "--output", os.path.join(tmpdir, "out.json")])
    with open(os.path.join(tmpdir, "out.json"), "r") as f, open("tests/integration/3ds.hexpat.json", "r") as f2:
        assert f.read() == f2.read(), "3DS pattern output does not match expected output"

failure_run(["format", "--input", "tests/integration/3ds.hexpat.3ds", "--pattern", "tests/integration/invalid.hexpat"])
failure_run(["format", "--input", "tests/integration/3ds.hexpat.3ds", "--pattern", "tests/integration/non_existing.hexpat"])

print("Tests successful")
