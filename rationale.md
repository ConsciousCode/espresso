Notes about the reasons for certain design decisions.

## Syntax
### Comments
Espresso uses `#` as the comment character to match a more scripty aesthetic and support shebang by default. It also has the `//` (integer division) operator, making that a bad choice for comments. Usually, languages which use this for comments don't have a way to get multiline comments, so the pattern from C was borrowed such that `#* ... *#` is a multiline comment. I like how the asterisk feels like emphasis, "wow! something's here!" as opposed to Julia's `#= ... =#`.

### Semicolons
Semicolons in Espresso are optional, but useful as a punctuator to unambiguously separate expressions. Without semicolons, expressions are intrinisically separated by parsing the longest valid expression. This is to lessen the burden of needing to know where to put semicolons in expressions, since all statements have semantics as expressions, and for code prettiness. Semicolons are still suggested to be used after every line to disambiguate though.

### Operators
#### ! and ~
In most C-likes, the `~` operator is used for bitwise negation while `!` is used for boolean negation. Espresso already has the `not` keyword, so also using `!` for boolean negation would be redundant. Rust already went through this line of thinking and settled on `!` for bitwise negation, which can also be used on booleans and doesn't use `~` at all.

#### ++ and --
The increment/decrement operators, both prefix and suffix, were initially going to be supported. After reading the rationale for Rust's exclusion of it, and hearing that Swift actually *removed* them, I decided it wasn't worth supporting.
* [Swift remove the ++ and -- operators](https://github.com/apple/swift-evolution/blob/main/proposals/0004-remove-pre-post-inc-decrement.md)
* Unclear semantics, especially in assignment
* Its primary use in C (pointer increment) is outmoded by iterators
* Increases the burden of operator overloading
* `++x` vs `x += 1` only saves 3 characters, including spaces
* Not applicable to most types

#### && and ||
The `&&` and `||` operators are not included because Espresso already has `and` and `or`, and no other operator has aliases. Generally, I've missed the keywords in languages that lack them, but I haven't missed the symbols in languages that have keywords.

#### ** vs ^^ vs ^
For exponentiation, languages usually choose one of `**`, `^^`, or `^`. Espresso uses `**` because two of the bigger languages it draws inspiration from that even have that operator (Python and JS) use it. `^` isn't viable because it conflicts with xor, and unlike Julia bitwise xor is more important than exponentiation.

#### <<> and <>>
Most langauges don't have explicit operators for rotating shifts despite hardware support. These two were chosen as an extension of the shift pattern, two in the direction of the shift and one in the opposite direction to indicate where the bits will end up. There is no corresponding operator for logical shifts because the difference there is in how the sign bit is handled, zeroed or extended, but all bits are accounted for in rotation.

#### has vs in
`has` is a property ownership operator, while `in` is a membership operator. `has` is used for properties because it's more intuitive to think of a property as being owned by an object rather than contained in it. Originally conceived because the `"property" in x` pattern in JS reverses the expected word order, but `has` and `in` aren't merely interchangeable.

#### Meta-operators
I originally thought `=` could be a meta-operator combining with any arbitrary binary operator to its left, such that eg `+ =` would be the same as `+=`. This is "pretty" from a high level design POV, but in implementing the parser I realized that this combined with the precedence climbing algorithm produced a fundamental contradiction. The normal operator could only be consumed if we knew its precedence was sufficiently high, but the `=` meta-operator would change its precedence to one of the lowest. Thus we need to consume the token to know its precedence, but we can't consume the token until we know its precedence. There were some "clever" hacks to try to get around this (tentatively consuming for sufficient precedence, then bailing if we parse `=` and modifying `self.cur` to semantically combine them) but the complexity grew and grew, all to implement a feature that, let's face it, has no practical value. I only thought of it in the first place because I thought it would make parsing easier, but it turns out it would require either a lot of extra code or I'd have to upgrade the parser to LR(2).

### Strings
#### Interpolation
Espresso uses `\{...}` to denote string interpolation rather than `{...}` from Python or `${...}` from JS. Backslashes are better suited for a language-level interpolation feature as they're already intended for escaping. For the rare case a literal `\{` is desired, such as in Regex, it will usually use either a string call or raw string anyway. In addition, all strings can be implicitly interpolated because the syntax is so clear, and it isn't overloaded as with other interpolation formats. Swift uses `\(...)`, but this has less parity with other interpolation formats and for lack of a better description doesn't "feel" as good to type, possibly because it requires four fingers (left pinkie for shift, right pinkie for backslash, and the right middle and ring fingers for the parentheses) whereas `\{...}` only requires two (both pinkies).

Implicitly interpolated strings are not anticipated to be an issue, unlike in PHP, because the interpolation syntax is hard to mistake. Interestingly, according to [PEP-498](https://peps.python.org/pep-0498/) `\{...}` was considered in Python, and even notes that this is desirable if all strings are formatted, but `{...}` was chosen because of the use of an explicit `f` prefix and to make it more similar to `str.format`.

#### Prefix specifiers
Strings have 6 varieties, single and triple quoted `'` `"` and `` ` ``, borrowed from Python. Backticks indicate raw strings with less escape sequences. String prefixes were originally considered, but dropped because it conflicted with string calls `abc"..."`, introduced semantic whitespace, and the alternatives arguably worked better. The logic to implement it was also unusually tricky without regex; even doing it with regex is tough to do correctly, since in Python they can occur in any order and must be used at most once.

The equivalents of each use case between Python and espresso are:
* `u"..."` - redundant in Python 3 and Espresso because both use unicode internally
* `r"..."` - `` `...` ``
* `b"..."` - `` bytes`...` ``
* `f"..."` - interpolation is implicit in all non-raw strings

#### Named universal character escapes
That is, a string escape sequence like `"\N{character name}"` as seen in C++ and Python. This isn't supported as a language-level feature because of the burden of requiring a database of unicode character names, which in the best case with gzip can still take up to 200 kB for a feature with marginal usefulness. In addition, if this were supported we would also want to support named character sequences, but this could break eg regex expecting a single character unless it's a detail of the regex library. Due to the flexibility of Espresso this could easily be implemented as a library detail.

State of the art succinct data structures for name lookup: [Compact unicode character names](https://gist.github.com/js-choi/320275d05d6f252f6bd55199f76489a6)

There's a possibility even more compression can be achieved using a finite state transducer, which is effectively a generalization of a trie using delta coding

#### Misc string escapes
Octal escapes (other than `\0`) aren't included because they're a potential source of bugs as they're variable-length and undelimited. An octal escape can't be distinguished from a trailing digit, for instance. They're also primarily meant for control characters, which are mostly unused, and are completely covered by hex escapes.

`\v` `\f` `\a` `\b` and `\e` aren't included because of their lack of modern usage. Rust also excludes these escape sequences.

## Internals
### ASCII-only lexing
The core lexer is defined to only respect ASCII, with values above 127 being treated as identifier characters, like Lua. This allows the finer details of unicode source code, such as ID_Start and ID_Continue, to be offloaded to scanning using `` \`...` `` to escape a sequence as an identifier. Compare this to C++ using `\uXXXX` in identifiers. The lexer considers all characters <= 32 (space) to be whitespace, with `\n` being the only recognized newline character. Support for unicode newline characters like `NEL` and `LS` is considered a detail of the scanner, requiring that it convert newline sequences into `\n`.