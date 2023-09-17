# Espresso Language Specification

## Terms and definitions

### Conformance
#### normativity
All requirements and recommendations are provided with the implicit stipulation that they be implemented "as-if" they are true. The actual implementation may differ provided there is no way for a distinction to be made without invoking unspecified behavior or through environment introspection.

#### shall
In this document "shall" is to be interpreted as a requirement on an implementation; "shall not" is to be interpreted as a prohibition. Failure to meet these requirements qualifies an implementation as non-conforming. If a "shall" or "shall not" requirement is violated, the behavior is undefined. Undefined behavior is otherwise indicated by the words "undefined behavior" or by the omission of any explicit definition of behavior.

#### should
In this document "should" is to be interpreted as a strong recommendation which may be broken in exceptional circumstances; "should not" is to be interpreted as a possible implementation which is strongly recommended against except in exceptional circumstances. The definition of "exceptional" here is left purposefully vague.

#### may
In this document "may" is to be interpreted as an example of a possible implementation for which conformance isn't necessary.

### Behaviors
#### undefined behavior
Behavior for which this standard imposes no requirements. Possible undefined behavior ranges from ignoring the situation completely with unpredictable results, to behaving during translation or execution in a documented manner characteristic of the environment, to terminating a translation or execution (with the issuance of a diagnostic message).

#### unspecified behavior
Behavior which this standard provides two or more possibilities and and may change in future implementations or iterations of this standard. Relying on unspecified behavior means a program is ill-formed and nonportable.

#### implementation-defined behavior
Unspecified behavior where each implementation documents how the choice is made

### Environment
#### abstract machine
The theoretical model used by an implementation in conformance with this standard to define the execution of a program.

#### execution environment
The environment in which the abstract machine executes a program, including the architecture, operating system, and configuration.

#### implementation
The particular set of software that performs translation of programs for, and supports execution of functions in, a particular execution environment

### Phases
#### lexical phase
Translation of source code to a sequence of distinct, non-overlapping tokens.

#### parse phase
Translation of a sequence of tokens to an Abstract Syntax Tree (AST) describing the structure of the program.

#### IR phase
Translation of an AST to an Intermediate Representation (IR), unspecified to the user code, which can be readily interpreted or compiled further into machine instructions.

#### compile phase
Translation of all or part of the IR of a program into machine instructions which can be executed directly by the execution environment. This may result in an executable binary or Just-In-Time (JIT) compiled functions which can be executed without interpretation.

### Values
#### bit
A unit of storage in the execution environment large enough to hold an object that may have exactly one of two values.
#### byte
An addressable unit in the execution environment large enough to hold an object that may have a value in the range `[0, 256)`

### Value categories
All lvalues are also rvalues and uvalues. All prvalues are also rvalues. All uvalues are also lvalues.

#### lvalue
An expression which can appear on the left-hand side of an assignment operator. This includes:
* Variable names
* Array subscripts
* Indexed object members
* Tuples, lists, dictionaries, or function calls
* Type assertions

#### rvalue
An expression which can appear on the right-hand side of an assignment operator. This includes:
* Literals
* Expressions
* Function calls
* Tuples, lists, or dictionaries with no defaults

### Terminology
#### Regex
A regular expression using the PCRE format.

## Design constraints
### Lexical simplicity
The lexical phase should be minimally stateful and receive minimal feedback from downstream phases in normal operation. Tokenization should not exceed the computational complexity of a Deterministic Pushdown Automata (DPA) and should aim for less wherever possible. Whitespace should serve the sole purpose of separating ambiguous tokens and should not be used in the parsing phase. Insertion of a space between two tokens should not change the semantics of the AST they parse to.

The parsing phase may not look ahead more than 1 symbol and may not backtrack. 

## Lexical elements
### Semantics
A token is the minimal lexical element of the language. The categories of tokens are: whitespace, comments, words, literals, operators, and punctuators. If the input stream has been parsed into tokens up to a given character, the next token is the longest sequence of characters which constitute a token.

### Whitespace
Whitespace is any character with an ASCII value less than or equal to 32 (the space character). They can occur between any pair of tokens without changing the semantics. The only exception is a newline after an identifier, which prevents the adjacency operator.

### Comment
Comments can appear anywhere whitespace is valid. They generally do not affect the semantics, but depending on where they appear they may be included in the program as metadata.

#### Single-line
Single-line comments begin with a hash character `#` and continue until the end of the current line.

#### Multi-line
Multi-line comments begin with `#*` and end with `*#`. No nesting is considered.

### Words
A word is any sequence of characters satisfying the following regular expression (regex):

`(?i)[a-z_][a-z\d_]*`

A subset of these are designated as _keywords_ which have special semantics in some contexts. These are:
* `if` `then` `else` `loop` `while` `for` `with` `try` `do` `which`
* `return` `fail` `yield` `break` `continue` `case`
* `import` `export` `namespace` `proto` `this` `super`
* `and` `or` `not` `in` `is` `as` `has` `after`
* `var` `let` `public` `protected` `private` `static`

The following is a list of reserved words which may become keywords in the future and should be avoided:
* `until` `before` `unless` `likely` `unlikely` `elif` `always`
* `await` `async` `defer` `sync` `atomic` `const` `catch` `where`
* `of` `def` `from` `inline` `noinline` `pure` `define` `ref` `ptr`
* `virtual` `dynamic` `abstract` `final` `new` `delete`
* `native` `extern` `unsafe` `asm` `strict` `debug` `goto`
* `typedef` `template` `using` `finally` `macro` `alias` `fn` `function`

### Constants
#### Numeric
All numeric literals (integer and floating-point) may be followed by a word with no space in between to specify a numeric suffix. All numeric patterns permit trailing underscores - if more than one underscore is present, only the first shall be considered part of the numeric literal and the rest is the suffix. The semantics of this suffix are a lookup of a variable with that name which is called, passing the numeric literal's string value.

#### Integer
Integer literals come in 4 varieties satisfying the following regexes:
* Binary integer: `(?i)0b[01_]*`
* Octal integer: `(?i)0o[0-7_]*`
* Decimal integer: `(?i)\d[\d_]*(?!e)`
* Hexadecimal integer: `(?i)0x(?i)[a-f\d_]*`

#### Floating-point
Floating-point literals come in 2 varieties satisfying the following regexes:
* Decimal float: `(?i)^(?=.*\d)\d[\d_]*\.[\d_]*(e[-+]?[\d_]*)?`
* Hexadecimal float: `(?i)^(?=.*\d)0x[a-f\d_]*\.[a-f\d_]*(p[-+]?[\d_]*)?`

#### String
String literals come in 6 varieties satisfying the following regexes:
* Single-quoted: `'(\\.|.+)*'`
* Double-quoted: `"(\\.|.+)*"`
* Back-quoted: ````(\\.|.+)*````
* Triple-quoted varieties of the above

Single and double-quoted strings can contain character escapes, the `\` character followed by one of the following sequences:
| Sequence | Description           |
| -------- | --------------------- |
| \        | Backslash             |
| '        | Single quote          |
| "        | Double quote          |
| `        | Backtick              |
| n        | Newline               |
| r        | Carriage return       |
| t        | Tab                   |
| 0-9      | 1-digit ASCII decimal |
| xHH      | 2-digit ASCII hex     |
| uHHHH    | 4-digit unicode hex   |
| u{...}   | n-digit unicode hex   |

Back-quoted strings are "raw strings" that can only contain the character escapes for ```\` ``` and `\\`; all other escapes are a literal backslash and following character.

Any newlines which appear in a string shall be converted to the ASCII newline character.

#### Interpolated strings
Interpolated strings support arbitrary nesting, and thus must be be split into 3 token regexes and a pushdown automata.
* Begin-interpolate: `(['"]|['"]{3}).*?\\\{` (push group 1)
* Between-interpolate: `.*?\\\{`
* End-interpolate: `.*?(['"]|['"]{3})` (pop, compare to group 1; syntax error if mismatched)

The between-interpolate and end-interpolate regexes should only be matched when the parser encounters an extra closing brace `}`, matching the opening escape pattern `\{`.

### Punctuators
Punctuators satisfy one of the following sequences:
* Syntax: `(` `)` `[` `]` `{` `}` `.` `..` `...` `:` `::` `;` `,`

### Operators
Operators satisfy one of the following sequences:
* Comparison: `<` `<=` `>` `>=` `==` `!=` `===` `!==` `<=>`
* Unary: `not` `@`
* Arithmetic: `+` `-` `*` `/` `%` `**` `//` `%%`
* Bitwise: `!` `&` `|` `^` `<<` `<<<` `>>` `>>>` `<<>` `<>>`
* Boolean: `and` `or` `??` `?!`
* Access: `->`
* Other: `.=` `..=` `,=` `:=` `::=` `??=` `@=` `~=`

Assignment operators are any of the arithmetic, bitwise, or non-keyword boolean operators followed by an equals character `=` with no other characters in between.

## Syntax
### Expression
An expression is a sequence of operators and operands that specifies computation of a value, declares a variable, generates side effects, or performs some combination thereof.

The grouping of operators and operands is indicated by the syntax. The order of evaluation of subexpressions and their side effects is depth-first from beginning to end unless otherwise specified.

### Relaxed identifier
A relaxed identifier is an identifier with relaxed restrictions for certain contexts. It consists of any valid token which is not a punctuator.

### Atoms
Atoms are any of the following expressions:
* Constants
* Compound literals
* Identifiers
* Subexpressions within enclosing parentheses: `( expression )`
* One of the keyword expressions

### Compound literals
#### List
A list compound literal is an atom with the following syntax:

`list = "[" ",".("..."? expression?)+ ","? "]";`

A list can be an lvalue provided all subexpressions are also lvalues. If there is one or more rvalue subexpressions, the list is a prvalue.

#### Object
An object compound literal is an atom with the following syntax:

```
computed_key = "[" expression "]"
key = relaxed_id | computed_key;
mapping = key ":" expression;
method = ("get" | "set" | "delete")? key "(" params ")" block;
property = ("public" | "protected" | "private")? (mapping | method);
entry = property | "..." expression;
object = "{" ",".(entry?)+ ","? "}";
```

An object is an lvalue if it contains only lvalue entries. It is a plvalue if it has any plvalues. A plvalue object shall contain no more than one `...` entry, and shall not contain any methods. If there is one or more rvalue subexpressions, or one or more methods, the object is a prvalue.

An object literal shall not contain duplicated mapping keys. If a computed key evaluates to an existing mapping key, its value overwrites the existing key.

### Keyword expressions

#### var and let
The `var` keyword denotes a mutable variable, while `let` denotes an immutable variable. Both have the same syntax, as follows:

```pypeg
type_lval = id "(" params ")";
func_lval = type_lval "{" statements "}";
decl = tuple_lval | list_lval | object_lval | type_lval | func_lval;
varlet = ("var" | "let") ",".decl
```

Variable bindings are hoisted to the top of the current scope and initialized to none if no initializer is provided.

### return
`return` is a unary rvalue expression with the `never` type. It immediately halts execution of the current function, returning the value of the expression if provided, or `none` if not.

### fail
`fail` is a unary rvalue expression with the `never` type. It immediately halts execution and resumes at the next enclosing `try`-`else` block in the current function, or returns a failure object with the value of the expression if provided, or `none` if not.

### yield
`yield` is a unary rvalue expression with the `option[any]` type. It immediately halts execution of the current function and returns control to the coroutine caller with the value provided, else `none`. If the coroutine is resumed, the `yield` expression returns the value provided to the `next` call, else `none`.