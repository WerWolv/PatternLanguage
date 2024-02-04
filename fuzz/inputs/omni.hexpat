u8 a = 43 [[export]];
u8 c = 11 [[hidden]];
u8 d @ 0x00 [[export]];

u8 a[while(0)] @ 0x00 [[export]];

struct A {
   u8 a [[export]];
   u8 c [[hidden]];
} [[format("A")]];

enum B : u8 {
  A = 43;
  B = 432,
  C = 62
};

bitfield C {
    u8 a : 23 [[export]];
    u8 c : 252 [[hidden]];
} [[format("A")]];

fn test() {
   a = 3;
   c = 4;
};