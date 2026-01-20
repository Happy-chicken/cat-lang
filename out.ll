; ModuleID = 'Cat_Module'
source_filename = "Cat_Module"
target triple = "x86_64-pc-linux-gnu"

define i32 @main() {
entry:
  %a.addr = alloca i32, align 4
  ret i32 0
}

declare void @inner(ptr, i32)

define void @inner.1(ptr %static_link.in, i32 %p) {
entry:
  %p.addr = alloca i32, align 4
  store i32 %p, ptr %p.addr, align 4
  ret void
}
