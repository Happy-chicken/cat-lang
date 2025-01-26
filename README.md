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
This project integrates JIT compiler using LLVM compiler and VM-based interpreter.


...

## syntax
### variable
```language
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
```language
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
```language
while(cond){
    statement...;
}
```

### function
closure?
```language
def add(x:int, y:int)->int
{
    return x+y;
}
```

### list
nested list? Idk...
```language
var l:list<int> = [1, 2, 3];
```

### class
support "__call__" magic method

more magic methods developping?
```language
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
````

