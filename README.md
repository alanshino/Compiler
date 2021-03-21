# Compiler
`gcc -Wall -g -o compiler compiler.c`

`./compiler your_file.c`

`./compiler test.c`

# About
This project is inspired by write-a-C-interpreter and is largely based on it.

scanner.c 這個檔案只能對輸入程式碼做掃瞄並儲存一些訊息

compiler.c 可以完成編譯

compiler_1.c 可以完成編譯並透過一些參數輸出相應的組合語言