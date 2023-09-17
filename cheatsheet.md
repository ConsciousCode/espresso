## Lexing
### Comments
* Single-line comment `# ...`
* Multi-line comment `#* ... *#`
* Nesting comment `#{ ... }#`

## Literals
* Builtins `none` `true` `false`
* Integers `\d[\d_]*` `0x[\da-fA-F_]+` `0o[0-7_]+` `0b[01_]+`
* Floats `\d[\d_]*\.[\d_]*(e[-+]?\d+)?`
* Strings `'...'` `"..."` `'''...'''` `"""..."""`
* Raw strings `` `...` `` `` \`\`\`...\`\`\` ``
* Escapes `'\{expr}'` `"\{expr}"` `\0\n\t\xBE\uABCD\u{1F4A9}`
* Functions `(...) => block` `name(...) => block`
* Lists `[1, , 2,]`
* Objects `{x, y: z, [a]: b, ...c, d?: 0, e() {}, f() => {}, get g() => {}, set h(x) => {},,,,}`

## Variables
* Name `\w[\w\d]*`
* Mutable `var x[: type] = 0`
* Immutable `let x[: type] = 0`
* Destructure `let {x, y: z} = obj`

## Operators
* (looser binding)
* Statement: `after `;`
* Separation: `,` `:`
* Keyword: `return` `fail` `yield`
* Arrow: `=>`
* Assignment: `=` and all compound assignment operators
* Nullish: `??` `?!`
* Boolean or: `or`
* Boolean and: `and`
* Equality: `==` `!=` `===` `!==`
* Relational: `<` `<=` `>` `>=` `<=>` `in` `is` `has` `as`
* Injection: `<:` `:>`
* Bitwise or: `|`
* Bitwise xor: `^`
* Bitwise and: `&`
* Bitshift: `<<` `>>` `<<<` `>>>` `<<>` `<>>`
* Additive: `+` `-`
* Multiplicative: `*` `/` `%` `//` `%%`
* Exponential: `**`
* Prefix unary: `!` `not` `+` `-`
* `new`
* Call and index: `f(...)` `f[...]`
* Application: `->`
* Access: `.` `::`
* Grouping: `(...)`
* (tighter binding)

## Control flow
* `"if" cond ["then"] th ["else" el]`
* `["loop" always] ["while" cond body] ["then" th] ["else" el]`
* `"for" "(" lv "in" rv ")" body`
* `"try" body ["then" th] ["else" el]`
* `"return" expr` `"fail" expr` `"yield" expr`
  - `"return" "..." expr` Explicit tail call.
  - `"fail" "..." expr` Explicit tail call.
  - `"yield" "..." expr` Delegated iteration.
* `"break" [label]` `"continue" [label]`
  - `break` can target any block, `continue` can only target loops.
* `"which" expr "{" ... "}" ["then" th] ["else" el]` Pattern matching, generalized `switch`.
  - Each match is `"case" [pattern] (":" | "=>") body`
    * `:` is a fallthrough case.
	* `=>` implicitly breaks before the next case.
  - Available patterns:
    * Literal integer or string matches.
    * `"...\{x}..."` string interpolation match. Splits the string and matches if the string components and subcomponents match.
      - This can be thought of like a regex match where the string parts are literal and the interpolations are matched against the group `(.*?)`.
    * `x "...\{y}..."` special string interpolation match. Allows `x` to run first on the string, then the result is queried for the values.
    * `x` At first level, match the value of `x`. At deeper levels, irrefutable match binding to `x`.
	* `("let"|"var") x` Explicitly bind x with `let`/`var` semantics.
	* `x y` Keyword call match, uses `x.case` coroutine to match `y`.
	* `( ... )` Evaluate `x` and match the result.
	* `[ ... ]` Iterate over `x` and match each item, respects rest operator.
	* `{ ... }` Mapping match, iterate over `x` expecting `(key, value)` pairs and match against the equivalent key.
	  - `x: y` Match `x` key, then match y value. `x` cannot be a bound parameter.
	  - `x` Matches `"x": x`
	  - `...x` Matches all other `key: value` pairs not matched as a `dict`.
    * `x | y` Match `x`, if it fails match `y`.
	* `x & y` Match `x`, then match `y`.
	* `! x` Match if `x` fails - cannot contain bound parameters.
	* `x ?` Attempt to match `x`, but do not fail the whole match if `x` fails. Wrapped with `optional[T]`.
	* `[x] op y` Bind value to `x`, then match if `x op y`
	* `x is [not] T` Match `x` if the type is `T`.
	* `x [not] in y` Match `x` if it is in `y`.
	* `x [not] has y` Match `x` if it has the attribute `y`.
	* `x[y]` Index match, matches `x[y]` and binds the value to `x` and `x[y]` to `y`
	  - The bind variable name can be changed using `x[y: z]`
    * `x.y` and `x.[y]` attribute match, fails if the value doesn't match or `E has not x` and binds the value to `x` and `x.y` to `y`
      - The bind variable name can be changed using `x.y: z` or `x.[y: z]`
    * `x if y` Matches `x`, then fails if `y` is falsey.
	* `x case y` Match `x` and `y`, with `y` being a new `which` pattern. Equivalent to Rust's `@` pattern.
	* `x = y` Defaulting match, try to match `x` and if it fails bind `y` to `x`.
	* `Name( ... )` Typed iterable match, start coroutine `Name.case` and pass each value to the coroutine, failing if the coroutine fails.
	* `Name{ ... }` Typed mapping match, same as above but for mappings.
	* `Name( ... ){ ... }` Combined match.
  - `x "and" y` Match `x` and if it's truthy, match the cases in `y`.
  - `x "or" y` Match `x` and if it's falsey, match the cases in `y`.

## Protocols
* Function
  - `call(...args) -> T`
* Iterable
  - `iter[T]() -> iterator[T]`
* Iterator
  - `next[T]() -> T?`
* Mapping
  - `getitem[K, V](key: K) -> V?`
  - `setitem[K, V](key: K, value: V)`
  - `delitem[K](key: K)`
  - `has[K](key: K) -> bool`
* Collection
  - `len() -> int`
  - `in[T](value: T) -> bool`