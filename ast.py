#!/usr/bin/env python3

from runtime import EspNone

def spjoin(*v):
	return ' '.join(str(x) for x in v)

def sexp(*v):
	def impl(v):
		for x in v:
			if x is None:
				pass
			elif type(x) is str:
				yield x
			elif type(x) is tuple:
				yield f"({' '.join(str(e) for e in x)})"
			elif type(x) is list:
				yield f"[{' '.join(str(e) for e in x)}]"
			elif isinstance(x, Expr):
				yield str(x)
			else:
				raise TypeError("Unknown value in sexp " + str(x))
	
	if v:
		x = 0
		prespace = ""
		
		if v[x] == ' ':
			prespace = " "
			x = 1
		
		return f"{prespace}({spjoin(*impl(v))})"
		
	else:
		return ""

class Expr:
	def __init__(self, token):
		self.token = token
		self.lvalue = True
		self.rvalue = True
	
	def __repr__(self):
		return f"<noimpl {type(self).__name__}.__str__>"
	
	def visit(self, v):
		return getattr(v, "visit_" + type(self).__name__.lower())(self)

class Value(Expr):
	'''Valueant'''
	
	def __init__(self, token, value=...):
		super().__init__(token)
		self.value = token.value if value == ... else value
		self.rvalue = False
	
	def __str__(self):
		return repr(self.value)
	
	def __repr__(self):
		return f"Value({self.value!r})"

class Var(Expr):
	'''Variable'''
	
	def __init__(self, token, mutable=True):
		super().__init__(token)
		self.name = token.value
		self.mutable = mutable
	
	def __str__(self):
		return self.name + ("!" * self.mutable)
	
	def __repr__(self):
		return f"Var({self.name!r}, mutable={self.mutable!r})"

class Spread(Expr):
	'''Spread operator, has its own node because it's syntax'''
	
	def __init__(self, token, var):
		super().__init__(token)
		self.var = var
		self.lvalue = var.lvalue
		self.rvalue = False
	
	def __str__(self):
		return f"...{self.var}"
	
	def __repr__(self):
		return f"Spread({self.var!r})"

class Assign(Expr):
	'''Assignment is syntactic too'''
	
	def __init__(self, token, name, value, op=""):
		super().__init__(token)
		self.name = name
		self.value = value
		self.op = op
	
	def __str__(self):
		return sexp(self.op + "=", self.name, self.value)
	
	def __repr__(self):
		return f"Assign({self.name!r}, {self.value!r}, {self.op!r})"

class Tuple(Expr):
	'''Tuple'''
	
	def __init__(self, token, elems):
		super().__init__(token)
		self.elems = elems
		self.lvalue = all(x.lvalue for x in elems)
		self.rvalue = all(x.rvalue for x in elems)
	
	def append(self, x):
		self.elems.append(x)
	
	def __str__(self):
		return sexp(",", *self.elems)
	
	def __repr__(self):
		return f"Tuple({self.elems!r})"

class Call(Expr):
	'''Call a function'''
	
	def __init__(self, token, func, args):
		super().__init__(token)
		self.func = func
		self.args = args
	
	def __str__(self):
		return sexp("call", self.func, *self.args)
	
	def __repr__(self):
		return f"Call({self.func!r}, {self.args!r})"

class Index(Expr):
	'''Index a value'''
	
	def __init__(self, token, obj, keys):
		super().__init__(token)
		self.obj = obj
		self.keys = keys
	
	def __str__(self):
		return sexp(".", self.obj, [*self.keys])
	
	def __repr__(self):
		return f"Index({self.obj!r}, {self.keys!r})"

class Loop(Expr):
	'''All loop types simplify to this node, an infinite loop'''
	
	def __init__(self, token, body, el=Value(EspNone)):
		super().__init__(token)
		self.body = body
		self.el = el
	
	def __str__(self):
		return sexp("loop", self.body, self.el and ("else", self.el))
	
	def __repr__(self):
		return f"Loop({self.body!r}, {self.el!r})"

class If(Expr):
	'''if statement'''
	
	def __init__(self, token, cond, th, el):
		super().__init__(token)
		
		self.cond = cond
		self.th = th
		self.el = el
	
	def __str__(self):
		return sexp("if", self.cond,
			self.th and ("then", self.th),
			self.el and ("else", self.el)
		)
	
	def __repr__(self):
		return f"If({self.cond!r}, {self.th!r}, {self.el!r})"

class Branch(Expr):
	'''Base class for branching in blocks'''
	
	def __init__(self, token, kind, level=0):
		super().__init__(token)
		self.kind = kind
		self.level = level
		
		if type(level) != int:
			raise TypeError("Branch.level must be int!")
	
	def __str__(self):
		return sexp(self.kind, self.level)
	
	def __repr__(self):
		return f"Branch({self.kind!r}, {self.level!r})"

class Op(Expr):
	'''Simple operation, evaluates to a value'''
	
	def __init__(self, token, *args):
		super().__init__(token)
		
		op = token.value
		self.op = op
		if type(op) is not str:
			raise RuntimeError("op must be string!")
		
		for x in args:
			if not isinstance(x, Expr):
				print(type(x), x)
				raise RuntimeError("Non-expression in op!")
		
		self.args = args
		self.lvalue = False # ops are always r-value
	
	def __str__(self):
		return sexp(self.op, *self.args)
	
	def __repr__(self):
		return f"Op({self.op!r}, {self.args!r}, {self.lvalue!r})"

class Import(Expr):
	'''Import statement, for now just support builtin libraries'''
	
	def __init__(self, token, name):
		super().__init__(token)
		self.name = name
	
	def __str__(self):
		return sexp("import", self.name)
	
	def __repr__(self):
		return f"Import({self.name!r})"

class Proto(Expr):
	'''Proto expression'''
	
	def __init__(self, token, name, parent, pub, priv, stat, methods):
		super().__init__(token)
		
		self.name = name
		self.parent = parent
		self.pub = pub
		self.priv = priv
		self.stat = stat
		self.methods = methods
	
	def __str__(self):
		return sexp("proto",
			self.name and f":{self.name}",
			self.parent and ("is", self.parent),
			self.pub and ("public", self.pub),
			self.priv and ("private", self.priv),
			self.stat and ("static", self.stat)
		)

class Return(Expr):
	'''Return statement'''
	
	def __init__(self, token, value):
		super.__init__(token)
		self.value = value
	
	def __str__(self):
		return sexp("return", self.value)
	
	def __repr__(self):
		return f"Return({self.value!r})"

class Case:
	def __init__(self, token, op, value, body, next):
		super().__init__(token)
		
		self.op = op
		self.value = value
		self.body = body
		self.next = next
	
	def __str__(self):
		return sexp("case", self.op, self.value,
			self.body, self.next and "..."
		)
	
	def __repr__(self):
		return f"Case({self.op!r}, {self.value!r}, {self.body!r}, {self.next!r})"

class Switch(Expr):
	'''
	Switch expression.
	
	This is implemented by separating the predicates from the values/bodies.
	 Predicates keep track of the comparison operation, value to compare
	 against, a body index, and a next index. Blocks 
	'''
	def __init__(self, token, ex, cs, de, th, el):
		super().__init__(token)
		
		self.ex = ex # EXpression
		self.cs = cs # CaseS
		self.de = de # DEfault
		self.th = th # THen
		self.el = el # ELse
	
	def __str__(self):
		return sexp("switch", self.ex,
			*self.cs,
			self.de and ("default", self.de),
			self.th and ("then", self.th),
			self.el and ("else", self.el)
		)

	def __repr__(self):
		return f"Switch({self.ex!r}, {self.cs!r}, {self.de!r}, {self.th!r}, {self.el!r})"

class Block(Expr):
	'''Sequence of expressions evaluating to the last'''
	
	def __init__(self, token, elems, vars=None):
		super().__init__(token)
		
		if type(elems) is not list:
			raise RuntimeError("Wrong type!")
		self.elems = elems
		self.vars = vars or []
		self.lvalue = False
	
	def __str__(self):
		#v = [x for x in self.vars if x.mutable]
		#c = [x for x in self.vars if not x.mutable]
		return sexp("block",
			self.vars and tuple(["var", *self.vars]),
			#c and tuple(["const", *c]),
			*self.elems
		)
	
	def __repr__(self):
		return f"Block({self.elems!r}, {self.vars!r})"

class Prog(Block):
	def __init__(self, elems, vars=None):
		super().__init__(None, elems, vars)
	
	def __repr__(self):
		return f"Prog({self.elems!r}, {self.vars!r})"

class Func(Expr):
	def __init__(self, token, name, args, body):
		super().__init__(token)
		
		self.name = name
		self.args = args
		self.body = body
	
	def __str__(self):
		return sexp("function", self.name, self.args, self.body)
	
	def __repr__(self):
		return f"Func({self.name!r}, {self.args!r}, {self.body!r}"