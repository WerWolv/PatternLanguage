## Pattern Language Fuzzing
Small subproject in the pattern language to allow fuzzing the parser for crashes and other issues.

### Pre-requisites
To use the fuzzer, you must first have the [AFL++](https://github.com/AFLplusplus/AFLplusplus) fuzzer installed.
Follow the instructions on their repository on how to build the afl-fuzz and afl-cc binaries.
Keep in mind that if you are compiling from source, the af-cc/afl-c++ binaries must be compiled with atleast
clang-17 support.

### Building
To build the fuzzer you must set the compiler to afl-cc/afl-c++. To achieve this, you need to alter the cmake flags to 
include the following:
```bash
-DCMAKE_C_COMPILER=afl-cc -DCMAKE_CXX_COMPILER=afl-c++ -DLIBPL_ENABLE_FUZZING=ON
```
After this, you can build the fuzzer as normal.   
The binary will be in this source folder.

### Fuzzing
The plfuzz binary takes in a file to parse as an argument.

To fuzz you can now follow the AFL++ tutorials on how to effectively fuzz.
There are some simple inputs in the `inputs` folder that you can use to start fuzzing.
There is also a dictionary file in the `dict` folder that you can use to improve the quality of the fuzzing.

Here is an example of how to start fuzzing:
```bash
afl-fuzz -i inputs -o output -x ./dict/hexpat.dict -- ./plfuzz @@
```
This will run a simple fuzzing session with the inputs in the `inputs` folder and outputting to the `output` folder.

### Minimization

To minimize the found crashes, you can simpy use the `afl-tmin` tool that comes with AFL++.
Here is an example of how to minimize a crash:
```bash
afl-tmin -i output/crashes/<crash_file> -o output/minimized/<crash_file> -- ./plfuzz @@
```

We also provide a small script to minimize all crashes in the `output/crashes` folder:
```bash
python3 afl-pytmin.py output/crashes output/minimized ./plfuzz
```

### Debugging
During the session, if the fuzzer finds crashes or halts, it will output the crashing input to the
`output/crashes` or `output/hangs` folder.   
To debug these cases simply run the plfuzz binary with the file as an argument:
```bash
./plfuzz output/crashes/<crash_file>
```
And you can attach GDB to the process to debug the crash, like so:
```bash
gdb -- ./plfuzz output/crashes/<crash_file>
```