; ModuleID = 'Cat_Module'
source_filename = "Cat_Module"
target triple = "x86_64-pc-linux-gnu"

@VERSION = global i32 42

declare void @foo()

define i32 @inner() {
entry:
  %e = alloca i32, align 4
  %d = alloca i32, align 4
  %c = alloca i32, align 4
  store i32 1, ptr %c, align 4
  store i32 1, ptr %d, align 4
  store i32 1, ptr %e, align 4
  %0 = load i32, ptr %d, align 4
  %add = add i32 2, %0
  store i32 %add, ptr %c, align 4
  %1 = load i32, ptr %c, align 4
  ret i32 %1
}

define i32 @main() {
entry:
}
