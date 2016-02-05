# Writing a LLVM Pass #

The LLVM project has a great documentation on
[how to write a LLVM Pass](http://llvm.org/docs/WritingAnLLVMPass.html).
We also provide a simple module pass on our source code that shows how to
use the Pointer Analysis. It can be found on the PADriver folder.

## Pointer Analysis Interface ##

We have implemented a pointer analysis algorithm on top of the LLVM infra-structure. The code is available in the `PointerAnalysis.h/cpp` files in this repository. Like any pointer analysis, ours can be divided into two parts: the tools to collect constraints from the program's intermediate representation, and the tools to solve these constraints. After solving the constraints, we can query which locations are aliases.

The interface used by the analysis (found on file `PointerAnalysis.h`) is
listed here as reference:

```
// Add a constraint of type: A = &B
void addAddr(int A, int B);

// Add a constraint of type: A = B
void addBase(int A, int B);

// Add a constraint of type: *A = B
void addStore(int A, int B);

// Add a constraint of type: A = *B
void addLoad(int A, int B);

// Execute the pointer analysis
void solve();

// Return the set of positions pointed by A:
//   pointsTo(A) = {B1, B2, ...}
std::set<int>  pointsTo(int A);

// Print the final result
void print();

// Print the final graph in DOT format
void printDot(std::ostream& output, std::string graphName, 
                std::map<int, std::string> names);
```

The first step of the pointer analysis is to collect constraints. Some
instructions in the program's intermediate representation generate these
constraints. The methods `addAddr`, `addBase`, `addStore`, and `addLoad` are
used to gather these constraints. Each constraints produces some vertices
and edges in the constraint graph.

It is this constraint graph that we will use to solve the points-to problem.
After all the constraints are created, the function  `solve()` must be called
to run the analysis. After this, the function `pointsTo(int)` can be used to
retrieve the set of memory positions pointed by a given identifier. There is
also a `print()` function which prints in the standard output the graph
connections and the points-to set of each identifier.

Notice that in order to keep our implementation simple, variables and memory
positions  are treated by the analysis as the same category of identifiers
(internally represented by integers). It is up to the client the task of pretty
printing these identifiers. In other words, the input given to the analysis,
i.e, the constraints, are just relations between integer values, as one can
infer in the interface above.


## How to run the provided Module Pass ##

As said, we provide on our source code a simple driver, a module pass which
translates the code's instructions to the aforementioned constraints, runs
the analysis and prints the final result. This driver is a very nice place to
start learning about how to use our points-to analysis.

To illustrate how to use the provided driver, we'll make an example. The code
we'll analyze is a simple implementation of a sorting algorithm, listed here:

```
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Insertion Sort
void sort(int* array, const int size) {
	int* c = array+1;
	int* end = array+(size);
	while (c != end) {
		int* pointer = c;
		while (pointer > array && *(pointer-1) > *pointer) {
			// SWAP *(pointer-1) with *(pointer)
			*(pointer-1) = *(pointer-1) + *pointer;
			*pointer = *(pointer-1) - *pointer;
			*(pointer-1) = *(pointer-1) - *pointer;
			
			pointer--;
		}

		c++;
	}
}

int main() {
	const int ARRAY_SIZE = 40000;
	int array[ARRAY_SIZE];

	// Fill array randomly
	srand48(time(NULL));
	for (int i = 0; i < ARRAY_SIZE; ++i) {
		array[i] = (int)(drand48()*5*ARRAY_SIZE);
	}

	// Sort and print final result
	sort(array, ARRAY_SIZE);	
	for (int i = 0; i < ARRAY_SIZE; ++i) {
		printf("%d)\t%d\n", i+1, array[i]);
	}
}
```

Let's call the source file `sort.cpp`. The code uses pointer arithmetics
instead of array indexes, to make the memory operations more explicit. The
steps we must take are the following:

  * Translate the c source code into a bitcode file:
```
  clang -emit-llvm -c sort.c -o sort.bc
```

  * Run the analysis:
```
  opt -load /path/to/the/compiled/PADriver.so -instnamer -analyze -pa sort.bc
```

The output will be printed on the screen, including the memory graph in DOT
format.

The control flow graph (CFG) of the code can be shown at runtime by passing
the option `-view-cfg` to the `opt` command. Alternatively, the CFG can be
written into a `.dot` file in the current working directory by passing the
option `-dot-cfg`.

These DOT format graphs can be converted to an image file using the _graphviz_
tools. Here is an example of how to generate a PNG from a `.dot` file:

```
  dot -Tpng -o output-file.png my-dot-file.dot
```

Where `output-file.png` is the output file and `my-dot-file.dot` is the file
with the graph in DOT format.

Here we have the CFG of the main function of our example, converted to an
image:

![http://addr-leaks.googlecode.com/svn/wiki/images/sample-cfg.png](http://addr-leaks.googlecode.com/svn/wiki/images/sample-cfg.png)

And here is the output graph generated by the analysis, also converted to an
image:

![http://addr-leaks.googlecode.com/svn/wiki/images/sample-graph.png](http://addr-leaks.googlecode.com/svn/wiki/images/sample-graph.png)

Note that the green vertices are the vertices merged after a cycle was found,
while the other, regular, vertices are blue. The red boxes are the set of
positions pointed by a given vertex.

More information regarding the LLVM options (such as `-view-cfg` and
`-dot-cfg`) can be found on the
[LLVM documentation](http://llvm.org/docs/Passes.html).
For information about the parameters of the `dot` tool and other output file
types , run "`dot -?`" or check the manpage (`man dot`).