# Providing your own libclang install
If you have libclang installed and Leon can't find it, please open an issue.

Otherwise, if you still need to provide your own libclang, put your libclang `bin` `lib` and `include` in here.

`bin` should contain `libclang.dll`, `libclang.so`, or something similar, depending on your target system.
`lib` should contain `libclang.lib`, `libclang.a`, or something similar, depending on your target system.
`include` should contain `clang-c`.
