from lark import Lark,Transformer
from lark.tree import pydot__tree_to_png

grammar = r"""
  string: ESCAPED_STRING 
  num: SIGNED_INTEGER
  SIGNED_INTEGER: /[+-]?(0|[1-9][0-9]*)/
  %import common.ESCAPED_STRING
"""
parser = Lark(grammar,start='num')

def parse(text):
  #print(parser.parse(text).pretty())
  tree = parser.parse(text)
  pydot__tree_to_png(tree,"parser_o.png")


parse('42')

