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

Here is an example of how to start fuzzing:
```bash
afl-fuzz -i inputs -o output -- ./plfuzz @@
```
This will run a simple fuzzing session with the inputs in the `inputs` folder and outputting to the `output` folder.

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