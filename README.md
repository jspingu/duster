# About

duster is a Forth-inspired, stack-oriented programming language that relies on self-modification to achieve complex behavior. I designed and implemented it after reading [this great article on threaded code](https://muforth.dev/threaded-code/) by David Frech.

# Quick Start

### Requirements

- GCC-compatible C23 compiler
- SDL3

### Clone & Build

```bash
git clone https://github.com/jspingu/duster.git
cd duster
make
```

### Run

Run `./duster` without arguments to enter the REPL. Exit with `Ctrl+D`.

```
> "Hello world!" puts nl
Hello world!
```

Script execution is also supported.

*example.dt*

```
"factorial": {
    [if (dup not) then (drop 1 unnest) endif]
    dup -- factorial *
}

10 range "factorial":& map putcons
```

```bash
./duster example.dt
1 1 2 6 24 120 720 5040 40320 362880 
```

# Overview

## Syntax

duster has no grammar. Each token directly corresponds to a single operation. This section lists tokens and their behavior.

A Vim syntax file is available in `duster.vim`.

### Literals

Integer, float, and string literals are implicitly pushed onto the *parameter stack* when encountered.

```
1414 3.14 "foo"  // 1414 3.14 "foo" <- top
```

### Subroutines

Non-literals are executed as subroutine calls when encountered. Here, `swap` is a subroutine that swaps the top two elements of the stack.

```
1 2   // 1 2 <- top
swap  // 2 1 <- top
```

### Compilation Controllers

Brackets are used to control the interpreter's compilation state. In compilation mode, the operations corresponding to each encountered token are not executed immediately. Their execution is deferred with the current *compilation depth* by being compiled into executable code that is pushed onto the *data stack*.

#### `()` - Enter Compilation Mode

The opening and closing parentheses increment and decrement the compilation depth, respectively.

```
1 ( 2 3 )  // 1 <- top
```

#### `[]` - Exit Compilation Mode

The opening and closing square brackets decrement and increment the compilation depth, respectively.

```
( 1 [2] 3 )  // 2 <- top
```

#### `{}` - Enter Subroutine Compilation Mode

The curly braces function identically to the parentheses, except that they also insert instructions at the beginning and end of the compiled code which allow it to be executed as a subroutine.

```
dsp { 1 } call  // 1 <- top
```

### Comments

Comments begin with `//` and continue until the end of the line.

```
// This is a comment
```

## Implementation

duster's interpreter compiles and executes [token-threaded code](https://en.wikipedia.org/wiki/Threaded_code#Token_threading) on a virtual stack machine. The virtual machine has a symbol table for subroutine lookup and three stacks:
- **Parameter stack**: contains arguments to subroutines
- **Data stack**: contains compiled code and other data
- **Call stack**: contains return addresses

All data elements are stored in 32-bit cells. Strings on the stack are reversed, null-terminated, and padded to the cell size.

A "standard library" is defined in `duster.dt` and is loaded automatically. It contains subroutines for basic stack manipulation, control flow, lists, etc.

## Examples

### Named Subroutines

The `:` native function associates a string with the current address at the top of the data stack in the symbol table. Use this with subroutine compilation controllers to define named subroutines.

```
"hello": { "Hello world!" puts nl }  // Define a subroutine named "hello"
hello                                // Call the newly defined subroutine
```

### FizzBuzz

FizzBuzz implementation to demonstrate loops and conditionals.

```
"fizzbuzz": {
    1
    [while (2dup >=) do]
        [if (dup 15 % not) then]
            "FizzBuzz" puts
        [elif (dup 3 % not) then]
            "Fizz" puts
        [elif (dup 5 % not) then]
            "Buzz" puts
        [else]
            dup puti
        [endif]
        nl ++
    [endwhile]
    2drop
}

100 fizzbuzz
```

### Bootstrapping Language Features

The `jmp` native function is pre-defined for unconditional jumps. It will jump to the address stored in the next cell ahead of it in the data stack. A forever loop could be written with this like so:

```
"do-forever": {
    [dsp]                 // Executed at compile-time: push the current data stack pointer onto the parameter stack
        "Hello!" puts nl
    jmp
    [->ds]                // Executed at compile-time: push the saved address onto the data stack, which is where `jmp` will jump to
}
```

This code is hard to understand. We can bootstrap a do-forever loop construct by defining compiling subroutines that will generate the necessary code for us. The principle is similar to macro assemblers, but instead of a preprocessor, we use duster's metaprogramming facilities to generate its own code.

```
"do": { dsp }              // Save the current data stack pointer
"forever": { (jmp) ->ds }  // Compile a jump instruction to the saved address

"do-forever": {
    [do]                   // Exit compilation and call `do`, pushing the current data stack pointer onto the parameter stack
        "Hello!" puts nl
    [forever]              // Exit compilation and call `forever`, which will compile a jump instruction to the saved address
}
```

All control flow constructs are bootstrapped in this way. For example, here are the subroutine definitions for while loops:

```
"stub": { dsp 0 ->ds }
"resolve": { dsp !ds }

"while": { dsp }
"do": { (jz) stub swap }
"endwhile": { (jmp) ->ds resolve }
```

These allow us to write while loops like this:

```
"count-to-ten": {
    1
    [while (dup 10 <=) do]
        dup puti nl
        ++
    [endwhile]
    drop
}
```

### First-Class Functions

Use `:&` to push the data stack address of a named subroutine onto the parameter stack. These can be passed to higher-order functions like `map` to apply a subroutine to each element of a list.

```
"double": { 2 * }

5 range "double":& map putcons  // Output: 0 2 4 6 8 
```

Anonymous functions can be defined by first pushing the current data stack pointer and then entering subroutine compilation. The address of the newly compiled subroutine will be on the parameter stack.

```
5 range dsp { 2 * } map putcons  // Output: 0 2 4 6 8 
```

Programming languages supporting first-class functions typically handle variable capture with closures, where functions implicitly access a data structure containing the encapsulated state. In duster, functions are first-class in the sense that there is no distinction between code and data. Because of this, we can emulate closure-like variable capture by simply compiling entirely new functions at runtime with the desired state embedded within them.

The `capture` subroutine exists for this purpose. It's intended usage is to take a cell from the compile-time parameter stack and compile it into an instruction that pushes that cell onto the runtime parameter stack.

```
5 range dsp {
    dsp swap {
        "Captured: " puts [capture] puti nl
    }
} map

"call":& foreach

// Output
// ------------
// Captured: 0
// Captured: 1
// Captured: 2
// Captured: 3
// Captured: 4
```

Of course, compiling the same function multiple times for different states wastes memory, but this serves as another example of how complex behavior can arise through self-modification in a language with a very simple set of rules.
