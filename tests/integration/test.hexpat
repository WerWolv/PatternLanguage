struct ChildStruct {
    u16 a;
    u8 b;
    u48 c;
};

struct MyStruct {
    u64 d;
    ChildStruct child;
    u32 e;
};

MyStruct data @ 0x0;
