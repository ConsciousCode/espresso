#define ESP_INTERNAL

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cassert>

#include "lex.hpp"
#include "zio.hpp"

extern "C" {
#include "buffer.h"
}

static const char* token_names[] = {
#define TOKEN_STR(name, str, ...) str,
	KEYWORD_LIST(TOKEN_STR)
	OPERATOR_LIST(TOKEN_STR)
#undef TOKEN_STR
	[TK_INT] = "int",
	[TK_REAL] = "real",
	[TK_STRING] = "string",
	[TK_IDENT] = "identifier",
	[TK_COMMENT] = "comment",
	[TK_ERROR] = "ERROR"
};

#define COUNT(...) + 1
#define NUM_KEYWORDS() (0 KEYWORD_LIST(COUNT))

// Check if the next character is a newline and adjust line/column
#define CHECK_NEWLINE() do { \
	if(cur == '\n') { \
		btc = 0; \
		++line; \
		off = offset(); \
	}} while(0);

// Advance the stream and return the token
#define ADV(x) (next(), (TK_##x))

/**
 * Hide the implementation of lexing and all the submethods
**/
struct LexImpl {
	Lexer& L;
	
	int cur;
	int line;
	int btc; // Bytes to column, = column for plain ASCII
	
	//esp_Buffer buf;
	
	LexImpl(Lexer& lex): L(lex) {
		line = 1;
		btc = 0;
		//esp_Buffer_init(&buf, 8);
	}
	
	void push(char d) { }//esp_Buffer_push(&buf, d); }
	void clear() { }//esp_Buffer_clear(&buf); }
	void save() { push(cur); }
	
	size_t offset(){ return L.z->offset(); }
	uint8_t next() { return cur = L.z->getc(); }
	bool eof() { return L.z->eof(); }
	
	uint8_t save_and_next() { save(); return next(); }
	
	int count_sequential(int qc);
	
	TokenType lex_punc();
	void lex_name();
	int64_t lex_int(char maxc);
	int lex_hex(int nd);
	
	bool lex(Token& tok);
	
	bool maybe(uint8_t x) {
		if(cur == x) {
			next();
			return true;
		}
		return false;
	}
};

bool Lexer::lex(Token& tok) {
	return LexImpl(*this).lex(tok);
}

/**
 * Maps single character punctuators to their TokenType
**/
constexpr TokenType tt_lookup(char x, int add) {
	#define TTL(c, name) case c: return (TokenType)(TK_##name + add);
	switch(x) {
		TTL('.', DOT) TTL(',', COMMA) TTL('?', NULLISH)
		TTL(':', COLON) TTL(';', SEMICOLON)
		TTL('(', LPAREN) TTL(')', RPAREN)
		TTL('[', LSQUARE) TTL(']', RSQUARE)
		TTL('{', LCURLY) TTL('}', RCURLY)
		TTL('+', PLUS) TTL('-', MINUS)
		TTL('*', STAR) TTL('/', DIV) TTL('%', MOD)
		TTL('~', BIT_NOT) TTL('^', BIT_XOR)
		TTL('|', BIT_OR) TTL('&', BIT_AND)
		TTL('<', LT) TTL('!', NOT) TTL('=', ASSIGN) TTL('>', GT)
		default: assert(0);
	}
	#undef TTL
}

#define SOLO(x, name) case x[0]: return TK_##name;
#define SOLO_EQ(x, name) \
	case x[0]: return maybe('=')? TK_ASSIGN_##name : TK_##name;
#define SOLO_EQ_OR_DUO(x, solo, duo) \
	case x[0]: if(maybe('=')) return TK_ASSIGN_##solo; \
		return maybe(x[0])? TK_##duo : TK_##solo;
#define SOLO_AND_DUO_EQ(x, solo, duo) \
	case x[0]: return maybe(x[0])? \
		maybe('=')? TK_ASSIGN_##duo : TK_##duo \
		: maybe('=')? TK_ASSIGN_##solo : TK_##solo;

/**
 * Lex punctuation, both syntactic and operators written as an ad-hoc prefix
 *  tree in code, returning the token code of the operator.
 * 
 * All operators have single and double versions
 * Inplace operators are implemented as the application of an assignment
 *  meta-operator =, which is treated as a separate token.
 * &&&
 * :::
 * ***
**/
TokenType LexImpl::lex_punc() {
	int c = cur;
	next();
	
	switch(c) {
		SOLO(",", COMMA) SOLO(";", SEMICOLON) SOLO("~", BIT_NOT)
		SOLO("(", LPAREN) SOLO(")", RPAREN)
		SOLO("[", LSQUARE) SOLO("]", RSQUARE)
		SOLO("{", LCURLY) SOLO("}", RCURLY)
		SOLO_EQ_OR_DUO("+", PLUS, INC)
		SOLO_EQ("*", STAR) SOLO_EQ("%", MOD)
		SOLO_EQ_OR_DUO("/", DIV, IDIV)
		SOLO_EQ_OR_DUO("^", BIT_XOR, EXP)
		SOLO_EQ_OR_DUO("&", BIT_AND, AND)
		SOLO_EQ_OR_DUO("|", BIT_OR, OR)
		
		case '.': // . .. ... .?
			return maybe('.')?
				maybe('.')? TK_ELLIPSIS : TK_RANGE
				: maybe('?')? TK_HAS : TK_DOT;
		
		case '?': // ? ?= ?? ??=
			return maybe('?')?
				maybe('=')? TK_ASSIGN_NULLISH : TK_NULLISH
				: maybe('=')? TK_ASSIGN_QUESTION : TK_QUESTION;
		
		case '-': // - -= -- ->
			if(maybe('=')) return TK_ASSIGN_MINUS;
			if(maybe('>')) return TK_POINT;
			return maybe('-')? TK_DEC : TK_MINUS;
		
		case ':': // := :: :> :
			if(maybe('=')) return TK_ASSIGN_COLON;
			if(maybe(':')) return TK_BIND;
			return maybe('>')? TK_RBIND : TK_COLON;
		
		case '>': // > >> >= >>= >>> >>>=
			return maybe('>')?
				maybe('>')?
					maybe('=')? TK_ASSIGN_ASH : TK_ASH
					: maybe('=')? TK_ASSIGN_RSH : TK_RSH
				: maybe('=')? TK_GE : TK_GT;
		case '<': // < << <= <=> <<= <:
			if(maybe('<')) return maybe('=')? TK_ASSIGN_LSH : TK_LSH;
			if(maybe(':')) return TK_LBIND;
			return maybe('=')?
				maybe('>')? TK_CMP : TK_LE
				: maybe(':')? TK_LBIND : TK_LT;
		case '!':
			return maybe('=')? maybe('=')? TK_ID_NE : TK_NE : TK_NOT;
		case '=': // = == === =>
			return maybe('=')? maybe('=')? TK_ID_EQ : TK_EQ : TK_ARROW;
		
		default: return TK_ERROR;
	}
}

// Callback for bsearch
int bsearch_strncmp(const void* k, const void* a) {
	auto* key = (esp_Buffer*)k;
	return strncmp((const char*)key->data, (const char*)a, key->size);
}

bool isIdentStart(char x) {
	return isalpha(x) || x == '_' || x == ESPZ_STARTER;
}

bool isIdentRest(char x) {
	return isalnum(x) || x == '_' || x >= ESPZ_STARTER;
}

/**
 * Lex a "name" (generic term for identifiers and keywords) - actual token
 *  creation is handled by the caller
**/
void LexImpl::lex_name() {
	assert(isIdentStart(cur));
	
	// 0x7F is specially treated to enable ZIO readers make a distinction
	//  between start and rest unicode characters.
	if(cur != ESPZ_STARTER) save();
	
	int c;
	while(isIdentRest(c = next())) {
		if(c != ESPZ_STARTER) save();
	}
}

/**
 * General-purpose int parser for bases using decimal digits (binary,
 *  octal, and decimal). Also useful for repeated invocations in
 *  float literal parsing, which could have up to 3 calls: integer
 *  part, fractional part, and exponent.
**/
int64_t LexImpl::lex_int(char maxc) {
	int64_t i = 0;
	int c = cur;
	
	assert(isdigit(c));
	
	do {
		i *= maxc - '0';
		i += c - '0';
		
		do {
			c = next();
		} while(c == '_');
	// Generic isdigit using the maximum digit
	} while(c >= '0' || c <= maxc);
	
	return i;
}

/**
 * Count how many times the current character is repeated, up to a
 *  given parameter. Used for triple quoting. Stream is advanced to the
 *  next char after the quotes
**/
int LexImpl::count_sequential(int upto) {
	int q = cur, i = 1;
	
	while(next() == q && ++i < upto) {}
	
	return i;
}

#define ERROR(msg) do { L.error = msg; goto error; } while(0);

#define ESCAPE(x, e) case x: c = e; break
#define COMMON_ESC(eof) \
	ESCAPE('0', '\0'); \
	ESCAPE('t', '\t'); \
	ESCAPE('n', '\n'); ESCAPE('r', '\r'); \
	case '\'': case '"': case '`': case '\\': case '\n': break; \
	case ESPZ_EOF: ERROR("EOF while lexing " eof); \
	default: push('\\'); break

int hexval(int x) {
	assert(isxdigit(x));
	// lo4('0') = 0
	// lo4('a' or 'A') = 1
	// =lo4(x) + 9*isAlpha(x)
	return (x&0xF) + 9*(x>>6);
}

int LexImpl::lex_hex(int nd) {
	int i = 0, c;
	do {
		if(!isxdigit(c = next())) {
			ERROR("Invalid hex escape sequence");
		}
		
		i = i<<4 | hexval(c);
	} while(--nd);

error:
	return i;
}

/**
 * Generic lexing iterator, returns whether or not a token was read (false
 *  on error). The output token will be changed regardless of the return.
 * 
 * @param lex The lexer to iterate
 * @param tok The output token, it will be mangled even if the lexer
 *  fails to lex a token.
**/
bool LexImpl::lex(Token& tok) {
	uint64_t ip;
	int off = offset(), c, q, qc;
	
	// Skip all space characters, keeping track of newline and column
	while(cur <= ' ') {
		next();
		CHECK_NEWLINE();
	}
	
	btc += offset() - off;
	
	tok.line = line;
	tok.btc = btc;
	
	switch(cur) {
		// Comments
		case '#':
			clear();
			
			if(next() == '*') {
				tok.flags.reset();
				tok.flags[TF_MLINE] = true;
				do {
					if(next() == '*') {
						if(next() == '#') break;
						push('*');
					}
					save();
					CHECK_NEWLINE();
				// EOF in comments is fine
				} while(cur != ESPZ_EOF);
			}
			else if(cur == '{') {
				int level = 1;
				do {
					if(maybe('}')) {
						if(maybe('#')) --level;
						push('}');
					}
					else if(maybe('#')) {
						if(maybe('{')) ++level;
						push('#');
					}
					save();
					CHECK_NEWLINE();
				} while(level && cur != ESPZ_EOF);
			}
			else {
				while(cur != ESPZ_EOF && cur != '\n') {
					save_and_next();
				}
			}
			
			btc += offset() - off;
			
			tok.type = TK_COMMENT;
			tok.str = esp_Buffer_decode_utf8(&buf);
			
			// Comments have a known stop char, so we need to skip
			if(cur != ESPZ_EOF) next();
			
			goto maybe_nl;
		
		// Real, dot, range, or ellipses
		case '.':
			if(isdigit(c = next())) {
				goto dot;
			}
			else if(maybe('.')) {
				tok.type = maybe('.')? TK_ELLIPSIS : TK_RANGE;
			}
			else {
				tok.type = TK_DOT;
			}
			goto no_nl;
		
		// 0b???, 0o???, 0???, 0.???, or 0x???
		case '0':
			tok.type = TK_INT;
			
			// Special base prefixes
			switch(next()) {
				case 'b':
					tok.ival = lex_int('1');
					break;
				
				case 'o': // Octal MUST be explicit
					tok.ival = lex_int('7');
					break;
				
				// Obsolete 0-prefixed octal generates no warnings
				case '0'...'9': goto decimal;
				case '.': goto dot;
				
				/* TODO: No check for overflow or type promotion */
				case 'x':
					ip = 0;
					while(isxdigit(c = next())) {
						ip = ip<<4 | hexval(c);
						while(c == '_') c = next();
					}
					
					tok.ival = ip;
					break;
			}
			goto no_nl;
		
		// Decimal int or real
		case '1' ... '9':
		decimal:
			ip = lex_int('9');
			
			// Real confirmed, label is for .??? from above
			if(cur == '.') {
			dot:
				uint64_t fp = lex_int('9');
				int digits = offset() - off, ex = 0;
				
				c = cur;
				if(c == 'e' || c == 'E') {
					c = next();
					
					if(c == '-' || c == '+') {
						next();
					}
					
					ex = lex_int('9');
					if(c == '-') ex = -ex;
				}
				
				// Avoid multiplying the pows together which could lose
				//  precision - instead, they're scaled then added which
				//  won't lose precision
				tok.rval = fp*pow(10, ex - digits) + ip*pow(10, ex);
				tok.type = TK_REAL;
			}
			goto no_nl;
		
		case '$':
			tok.flags = 0;
			for(;;) {
				switch(next()) {
					case 'r': tok.flags[TF_RAW] = true; break;
					case 'b': tok.flags[TF_BYTES] = true; break;
					case 'f': tok.flags[TF_FORMAT] = true; break;
					
					case '\'': case '"': case '`':
						tok.type = TK_STRING;
						goto lex_string;
					
					default: ERROR("Invalid string literal prefix");
				}
			}
		
		// Normal string (no qualifiers)
		case '\'': case '"': case '`':
			tok.flags = 0;
		
		lex_string:
			clear();
			
			q = cur;
			qc = count_sequential(3);
			btc += qc;
			
			// Empty string
			if(qc == 2) goto maybe_nl;
			
			do {
				c = save_and_next();
				if(c == '\\') {
					if(tok.flags[TF_RAW]) {
						// Raw strings keep backslashes, but we still need to
						//  handle escaping the quote character
						if(save_and_next() == ESPZ_EOF) {
							ERROR("EOF while lexing string");
						}
						c = next();
					}
					else if(tok.flags[TF_BYTES]) {
						/**
						 * Handle escape sequences for byte literals
						 *  (unicode escapes not supported, and hex escapes
						 *  produce that byte value instead of that
						 *  codepoint).
						**/
						switch(c = next()) {
							COMMON_ESC("bytes");
							
							// Hex encodes bytes instead of codepoints
							case 'x':
								c = lex_hex(2);
								if(L.error) ERROR(L.error);
								break;
						}
					}
					else {
						/**
						 * Handle escape sequences for normal strings,
						 *  including unicode escapes. Returns true on error.
						**/
						switch(c = next()) {
							COMMON_ESC("string");
							
							case 'U':
								c = lex_hex(8);
								goto unicode_esc;
							case 'u':
								c = lex_hex(4);
								goto unicode_esc;
							case 'x':
								c = lex_hex(2);
								goto unicode_esc;
							
							unicode_esc:
								// Build UTF-8
								if(c > 0x7F) {
									if(c <= 0x07FF) {
										// 2-byte unicode
										push((c>>6)&0x1F | 0xc);	
									}
									else {
										if(c <= 0xFFFF) {
											// 3-byte unicode
											push((c>>12)&0x0F | 0xE0);
										}
										else if(c <= 0x10FFFF) {
											// 4-byte unicode
											push((c>>18)&0x07 | 0xF0);
											push((c>>12)&0x3F | 0x80);
										}
										else {
											ERROR("Codepoint out of range");
										}
										
										push((c>>6)&0x3F | 0x80);
									}
									
									c = c&0x3F | 0x80;
								}
								break;
						}
					}
					
					// Push the last byte of whatever the escape represents
					push(c);
				}
				
				CHECK_NEWLINE();
				
				if(cur == q && count_sequential(qc) == qc) {
					break;
				}
			} while(cur != ESPZ_EOF);
			
			if(cur == ESPZ_EOF) {
				ERROR("EOF while lexing string literal");
			}
			
			btc += offset() - off;
			tok.type = TK_STRING;
			goto maybe_nl;
		
		// All keywords use lowercase
		case 'a' ... 'z': {
			clear();
			lex_name();
			
			void* x = bsearch(
				&buf, token_names,
				NUM_KEYWORDS(), sizeof(char*),
				&bsearch_strncmp
			);
			
			if(x) {
				tok.type = (TokenType)((const char**)x - token_names);
			}
			else {
				tok.type = TK_IDENT;
			}
			goto no_nl;
		}
		
		// Keywords are always lowercase ASCII, so if it starts with these
		//  then we don't have to check if it's a keyword
		case 'A' ... 'Z':
		case '_': case ESPZ_STARTER:
			lex_name();
			goto no_nl;
		
		case ESPZ_INVALID:
			ERROR("Invalid unicode codepoint");
		
		// Anything else is an operator, difficult to inline because of
		//  the complex control flow simplified by returns.
		default:
			tok.type = lex_punc();
			goto no_nl;
	}
	
	// Tokens which can't contain newlines don't handle column
	//  incrementing, so we need to do it externally here.
no_nl:
	btc += offset() - off;
	
	// Column incrementing has been taken care of
maybe_nl:
	return true;

error:
	tok.type = TK_ERROR;
	return false;
}

const char* Token::c_str() const {
	return token_name(type);
}

const char* Token::token_name(TokenType tt) {
	return (tt < _TK_COUNT)? token_names[tt] : "INVALID";
}
