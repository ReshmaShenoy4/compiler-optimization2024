from lark import Lark, Transformer
from lark.tree import pydot__tree_to_png

grammar = r"""
  string: ESCAPED_STRING
  num: SIGNED_INT
  start:?expr
  expr: expr | e
  e : e DIV d -> div|d
  d : d MUL a -> mul|a
  a : a ADD b -> add |b
  b : b SUB c -> sub |c
  c : NUMBER  | "("e")" #I thought we would have to add parenthesis in the stat but it seems like 
# we have to add to c

# This was added so that we can explicitly see in the tree.
  DIV:"/"
  MUL: "*"
  ADD: "+"
  SUB: "-"

  %import common.NUMBER
  %import common.SIGNED_INT
  %import common.ESCAPED_STRING
  %import common.WS
  %ignore WS
"""
parser = Lark(grammar)

def parse(text):
#  #print(parser.parse(text).pretty())
  tree = parser.parse(text)
  print(tree.pretty()) # note: returns a string, not a tree
  #print(tree)
  #pydot__tree_to_png(tree,"parser_o.png")
 
#Notice the execution starts from bottom. So according to BODMAS rule, you musts ee the tree from bottom.
#The ordering of how you write the rules matters.
#parse('"Hello world"')
parse('42 - (1+2)')