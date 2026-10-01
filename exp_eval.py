from lark import Lark, Transformer
from lark.tree import pydot__tree_to_png


grammar = r"""
  string: ESCAPED_STRING
  num: SIGNED_INT
  expr: expr | e
  ?e : e SUB d -> sub |d
  ?d : d ADD a -> add |a
  ?a : a MUL  b -> mul |b
  ?b : b DIV c -> div |c
  ?c : NUMBER  | "("e")"

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

class MyTransformer(Transformer):
    
    def NUMBER(self, value):
        #(value, ) = value
        return int(value)


    def div(self, items):
        if len(items) == 1: 
            return items[0]
        else:
            a,_,b= items # To include the operator

            if(b==0):
                raise ValueError("Division by zero is not allowed")
            else:
                return a/b
            
    def mul(self, items):
        if len(items) == 1: 
            return items[0]
        else:
            a,_,b= items
            return a*b
 
    
    def add(self, items):
        if len(items) == 1: 
            return items[0]
        else:
            a,_,b= items
            return a+b
    
    def sub(self, items):
        if len(items) == 1: 
            return items[0]
        else:
            a,_,b= items
            return a-b
        

    # This was added because you wanted to show only the output and not the entire tree structure.
    def expr(self,items):
        return items[0]
    
parser = Lark(grammar,start='expr')

def parsing(exp):
    tree = parser.parse(exp)
    print(tree.pretty())
    result = MyTransformer().transform(tree)
    print(f"Operation result is",result)
    return result


#TESTS
# Default
parsing("42 + 1")
parsing("42 + 1 * 2")
parsing("42 - 1 + 2")
parsing("42 + 4 / 2")
parsing("42 - (1 + 2)")

##Additional
parsing('42 + (2*1)')
parsing('(42*2) -1')
parsing('(42*2 -1)')
parsing('42*2 -1')
parsing('0-5')
parsing('42 - 1*5 +2')
# Error
#parsing("42/0")
#parsing('42/0 -1') 
#parsing('(42*2 -1')
