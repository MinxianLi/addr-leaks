# Dependency Graphs #

## What is a dependency graph? ##

Dependency graphs are the core data structure that we use to find leaks in programs. It contains a vertex _v_ for every variable in the program, and a vertex _m_ for every chunk of memory that we allocate. An edge from _v_ to _v'_ is introduced for every instruction where the produced result _v_ depends on the value of the operand _v'_. For example, an addition instruction such as `a = b + c;` will generate edges from vertex _a_ to _b_ and _a_ to _c_.

## Example ##

Consider the program below:

```
#include <stdio.h>

int main() {
    int a, b, c;

    scanf("%d", &a);
    scanf("%d", &b);

    c = a + b;
    
    printf("%d\n", c);

    return 0;
}
```

The resulting llvm bitcode when compiling this code and using our
opt flags is:

$ clang -c -emit-llvm prog.c -o prog.bc
$ opt -mem2reg -instnamer -internalize -inline -globaldce < prog.bc > prog2.bc
$ llvm-dis < prog2.bc > prog2.txt

```
; ModuleID = '<stdin>'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-
n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@.str1 = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1

define i32 @main() nounwind uwtable {
entry:
  %a = alloca i32, align 4
  %b = alloca i32, align 4
  %call = call i32 (i8*, ...)* @__isoc99_scanf(i8* getelementptr inbounds ([3 x i8]* @.str, i32 0, i32 0), i32* %a)
  %call1 = call i32 (i8*, ...)* @__isoc99_scanf(i8* getelementptr inbounds ([3 x i8]* @.str, i32 0, i32 0), i32* %b)
  %tmp = load i32* %a, align 4
  %tmp1 = load i32* %b, align 4
  %add = add nsw i32 %tmp, %tmp1
  %call2 = call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([4 x i8]* @.str1, i32 0, i32 0), i32 %add)
  ret i32 0
}

declare i32 @__isoc99_scanf(i8*, ...)

declare i32 @printf(i8*, ...)
```

To generate the dependency graph for it, the run.sh script present in the
AddrLeaks pass should be modified to pass the flag "-x", such as:

```
opt -load ../../../Release+Asserts/lib/AddrLeaks.so -mem2reg -instnamer -internalize -inline -globaldce -x -addrleaks < "$1" > /dev/null #&&
dot -Tpng module.dot -o module.png &&
eog module.png
```

Then, the script can be called passing the name of the target bitcode as argument:

$ ./run.sh prog.bc

This flag will instruct the static analysis to produce a dot representation of the
dependency graph, stored in a file called "module.dot". For the sample program, the
content of this file is:

```
digraph module {
    "(main) a VALUE" -> "(memory) m1 VALUE" [style=dashed]
    "(main) b VALUE" -> "(memory) m3 VALUE" [style=dashed]

    "(main) add VALUE" -> "(main) tmp VALUE"
    "(main) add VALUE" -> "(main) tmp1 VALUE"
    "(main) call VALUE" -> "(main) __isoc99_scanf VALUE"
    "(main) call1 VALUE" -> "(main) __isoc99_scanf VALUE"
    "(main) call2 VALUE" -> "(main) printf VALUE"
    "(main)   ret i32 0 VALUE" -> "(main) i32 0 VALUE"
    "(main) tmp VALUE" -> "(memory) m1 VALUE"
    "(main) tmp1 VALUE" -> "(memory) m3 VALUE"
    "(main) a VALUE" -> "(memory) m1 ADDR"
    "(main) b VALUE" -> "(memory) m3 ADDR"
}
```

This dot file can be processed to generate an image containing the
visual representation for the graph. The _run.sh_ script do it calling the "dot" program
that should be installed in the machine. It can be obtained by installing
_graphviz_.

The dependency graph produced is shown below:

![http://addr-leaks.googlecode.com/svn/trunk/AddrLeaks/module.png](http://addr-leaks.googlecode.com/svn/trunk/AddrLeaks/module.png)

We can see that this dependency graph has two different types of edges: solid and dashed.
The solid edges represent direct data dependency between the values of the linked vertices.
For example, the _add_ vertex depends on the values of _tmp_ and _tmp1_. The dashed edges represents
points-to information. For example, the _a_ vertex depends on the value of the (m1 ADDR)
vertex because it is a pointer and a pointer contains the address of the memory position
it points to. The dashed edge means that when you read the value located in the address
pointed by _a_, you are reading the value stored in that position, hence (m1 VALUE).