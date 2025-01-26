# Welcome to Cat-Lang language

```
                                                                ________________________________________
                                                                |               |\__/,|   (`\          |
                                                                |             _.|o o  |_   ) )         |
                                                                |-------------(((---(((----------------|        
                                                                |      ___          ___                |       
                                                                |     /  /\        /  /\        ___    |       
                                                                |    /  /:/       /  /::\      /  /\   |       
                                                                |   /  /:/       /  /:/\:\    /  /:/   |       
                                                                |  /  /:/  ___  /  /:/~/::\  /  /:/    |       
                                                                | /__/:/  /  /\/__/:/ /:/\:\/  /::\    |       
                                                                | \  \:\ /  /:/\  \:\/:/__\/__/:/\:\   |       
                                                                |  \  \:\  /:/  \  \::/    \__\/  \:\  |        
                                                                |   \  \:\/:/    \  \:\         \  \:\ |        
                                                                |    \  \::/      \  \:\         \__\/ |       
                                                                |     \__\/        \__\/               |      
                                                                |______________________________________|
```

## Info
This project integrates JIT compiler using LLVM and VM-based interpreter.


...

## syntax
### variable
```python
var a:int = 1;
var a:double = 1.1;
var:str = "hello, catlang!"
// local variable
{
    var a:int=2;
    ...
}
```

### control flow
```python
if (true)
{
    print("True");
}
else
{
    print("false");
}
```

### loop
only support while

developing for loop in python style...
```python
while(cond){
    statement...;
}
```

### function
closure? overload?
```python
def add(x:int, y:int)->int
{
    return x+y;
}
```

### list
nested list? Idk...
```python
var l:list<int> = [1, 2, 3];
var s:int = l[0];
```

### class
support "__call__" magic method, inheritance and ploymorphism.

more magic methods developping?
``` python
class base
{
    init(self){}
    method(self){}
}
class derived < base
{
    var x:int;
    var y:int;
    init(self, x:int, y:int)
    {
        self.x=x;
        self.y=y;
        return self;
    }
    method(self, param.:type)->type
    {
        statement...;
    }
    __call__(self, x:int)->int
    {
        return self.x * x;
    }
}
var base_class:base = base();
var derived_class:derived = derived(1, 2);
base_class.method();
derived_class.method();
```

## How to use
- install cmake, llvm-14, libgc, (ninja)
- mkdir build && cd build
- cmake -G (Ninja) ../
- ninja ./
- If you set isUseJIT = flase, you need to use clang++ to compile the .ll file to ELF file.


