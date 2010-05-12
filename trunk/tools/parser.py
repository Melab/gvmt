#!/usr/bin/python

#Copyright (C) 2008 Mark Shannon
#GNU General Public License
#For copyright terms see: http://www.gnu.org/copyleft/gpl.html

#Parser routines for GVMT front-end components.

import lex
from common import struct, GVMTException

_tkn_list = 'C{list} of L{lex.Token}'
_code = 'L{CCode} or a C{list} of GSC instruction names'
Declaration = struct('Declaration', ('type', 'C{str}'), ('name', 'C{str}'), 
                     ('location', 'L{lex.Location}'))

CCode = struct('CCode', ('stack', 'L{StackComment}'), ('code', 'C{str}'), 
               ('start', 'L{lex.Location}'))

_items = 'C{list} of L{Declaration}'
StackComment = struct('StackComment', ('inputs', _items), ('outputs', _items))

CompTree = struct('CompTree', ('directives', _tkn_list), 
                  ('instructions', 'C{list} of L{CompilerInst})'))

CompilerInst = struct('CompilerInst', ('name', 'C{str}'), ('pred', _code), 
                      ('exe', _code), ('replace', _code))

InterpreterTree = struct('InterpreterTree', ('directives', _tkn_list), 
                         ('locals', _items), ('instructions', 
                          'C{list} of L{InterpreterInstruction}'))

InterpreterInstruction = struct('InterpreterInstruction', ('name', 'C{str}'), 
                                ('opcode', 'C{int} or C{None}'), 
                                ('location', 'L{lex.Location}'), 
                                ('qualifiers', 'C{list} of C{str}'), 
                                ('code', _code))
Operand = struct('Operand', ('name', 'C{str}'), ('size', 'C{int}'),
                 ('location', 'L{lex.Location}'))

NameAndOperands = struct('NameAndOperands', ('name', 'C{str}'),
                         ('location', 'L{lex.Location}'),
                         ('opcodes', 'C{list} of L{Operand}'))
    
Transformation = struct('Transformation', ('input', 'NameAndOperands'), 
                        ('code', 'C{str}'), 
                        ('condition', 'C{str} or C{None}'),
                        ('output', 'L{NameAndOperands} or C{None}'))
                        
TransformationTree = struct('TransformationTree', ('directives', _tkn_list), 
                            ('preamble', 'L{InterpreterInstruction}'),
                            ('transformations', 'C{list} of L{Transformation}'))

def _syntax_error(tkn, expecting):
    raise GVMTException("%s: Syntax Error. Expecting %s, found '%s'\n" % 
                        (tkn.location, expecting, tkn.text))

def _parse_name_and_operand(lexer):
    t = lexer.next_token()
    if t.kind == lex.NAME:
        name = t.text
    else:
        _syntax_error(t, "a name")
    location = t.location
    t = lexer.next_token()
    opcodes = []
    while t.kind == lex.STREAM_NAME:
        size = t.text.rindex('#')+1
        name = t.text[size:]
        opcodes.append(Operand(name, size, t.location))
        t = lexer.next_token()
    lexer.push_back(t)    
    return NameAndOperands(name, location, opcodes)
            
def _parse_transformation(lexer):
    input = _parse_name_and_operand(lexer)
    code = _parse_c_code_body(lexer)
    t = lexer.next_token()
    if t.kind == lex.NAME and t.text == 'if':
        condition = _parse_condition(lexer)
        t = lexer.next_token()
    elif t.kind == lex.SEMI:
        return Transformation(input, code, None, None)
    else:
        condition = None
    if t.text != '->':
        _syntax_error(t, "'->'")
    output = _parse_name_and_operand(lexer)
    t = lexer.next_token()
    if t.kind != lex.SEMI:
        _syntax_error(t, "';'")
    return Transformation(input, code, condition, output)
    
def _parse_declaration(lexer, stream_allowed):
    t = lexer.next_token()
    if t.kind == lex.NAME or t.kind == lex.DOTS:
        type = t.text
    else:
        _syntax_error(t, "a type")
    location = t.location
    t = lexer.next_token()
    while t.kind == lex.STAR:
        type += '*'
        t = lexer.next_token()
    if t.kind == lex.NAME: 
        return Declaration(type, t.text, location)
    elif t.kind == lex.STREAM_NAME and stream_allowed:
        return Declaration(type, t.text, location)
    else:
        _syntax_error(t, "a name")
        
def _stack_comment_list(lexer, stream_allowed, terminator, tname):
    t = lexer.peek_token()
    if t.kind == terminator:
        lexer.next_token()
        return []
    clist = [ _parse_declaration(lexer, stream_allowed) ]
    t = lexer.next_token()
    while t.kind == lex.COMMA:
        clist.append(_parse_declaration(lexer, stream_allowed))
        t = lexer.next_token()
    if t.kind != terminator:
        _syntax_error(t, tname)
    return clist
    
def _parse_stack_comment(lexer):
    t = lexer.next_token()
    if t.kind != lex.LPAREN:
        _syntax_error(t, "'('")
    inputs = _stack_comment_list(lexer, True, lex.DOUBLE_DASH, "'--'")
    outputs = _stack_comment_list(lexer, False, lex.RPAREN, "')'")
    return StackComment(inputs, outputs)
    
def _parse_c_code_body(lexer):
    t = lexer.next_token()
    location = t.location
    if t.kind == lex.SEMI:
        return ''
    elif t.kind != lex.LBRACE:
        _syntax_error(t, "'{'")
    code = ['{']
    braces = 1
    line = location.line
    while braces:
        t = lexer.next_token()
        if t.kind ==  lex.LBRACE:
            braces += 1
        elif t.kind == lex.RBRACE:
            braces -= 1
        elif t.kind ==  lex.END_OF_FILE:
            _syntax_error(t, "'}'")
        while t.location.line != line:
            code.append('\n')
            line += 1
        code.append(t.text)  
    return ' '.join(code)
    
def _parse_condition(lexer):
    code = ['(']
    parens = 1
    t = lexer.next_token()
    line = t.location.line
    if t.kind != lex.LPAREN:
        _syntax_error(t, "')'")
    while parens:
        t = lexer.next_token()
        if t.kind ==  lex.LPAREN:
            parens += 1
        elif t.kind == lex.RPAREN:
            parens -= 1
        elif t.kind ==  lex.END_OF_FILE:
            _syntax_error(t, "')'")
        else:
            while t.location.line != line:
                code.append('\n')
                line += 1
        code.append(t.text)  
    return ' '.join(code)
         
def _parse_c_code(lexer):
    stack = _parse_stack_comment(lexer)
    location = lexer.peek_token().location
    code = _parse_c_code_body(lexer)
    return CCode(stack, code, location)
        
def _parse_gsc_code(lexer):        
    arguments_legal = False
    line = -1
    t = lexer.next_token()
    if t.kind != lex.COLON:
        _syntax_error(t, "':'")
    code = []      
    t = lexer.next_token()
    while t.kind != lex.SEMI:
        if t.kind == lex.LPAREN:
            if not arguments_legal:
                _syntax_error(t, 'GSC instruction')
            else:
                code.append(t)
            t = lexer.next_token()
            if t.kind in (lex.NAME, lex.NUMBER, lex.STRING):
                code.append(t)
            else:
                _syntax_error(t, 'a name, number or string')
            t = lexer.next_token()
            while t.kind == lex.COMMA:
                if t.location.line != line:
                    t.text = 'new line'
                    _syntax_error(t, "','")
                code.append(t)
                t = lexer.next_token()
                if t.location.line != line:
                    t.text = 'new line'
                    _syntax_error(t, 'a number or a string')       
                elif t.kind == lex.NUMBER or t.kind == lex.STRING:
                    code.append(t)
                else:
                    _syntax_error(t, 'a number or a string')
            if t.location.line != line:
                t.text = 'new line'
                _syntax_error(t, "')'")
            elif t.kind != lex.RPAREN:
                _syntax_error(t, "')'")
            arguments_legal = False
        elif t.kind == lex.NAME:
            arguments_legal = True
            line = t.location.line
        elif t.kind in (lex.STREAM_INST, lex.NUMBER, lex.ADDRESS):
            arguments_legal = False
        else:
            _syntax_error(t, 'GSC instruction')
        code.append(t)
        t = lexer.next_token()
    return code
    
def _parse_code(lexer):
    t = lexer.next_token()
    if t.kind == lex.COLON:
        lexer.push_back(t)
        return _parse_gsc_code(lexer)
    elif t.kind == lex.LPAREN:
        lexer.push_back(t)
        return _parse_c_code(lexer)
    else:
        _syntax_error(t, "':' or '('")
           
def _parse_qualifiers(lexer):
    '''
    Parses qualifiers
    @return: A list of qualifiers
    @rtype: C{list} of C{str}
    '''
    qualifiers = []
    t = lexer.next_token()
    if t.kind != lex.LBRACKET:
        _syntax_error(t, "'['")
    t = lexer.next_token()
    while t.kind != lex.RBRACKET:
        if t.kind == lex.NAME:
            qualifiers.append(t.text)
        else:
            _syntax_error(t, "a name")
        t = lexer.next_token()
    return qualifiers
            
def _parse_local(lexer):
    result = _parse_declaration(lexer, False)
    t = lexer.next_token()
#    if t.kind == lex.LBRACKET:
#        t = lexer.next_token()
#        if t.kind != lex.NUMBER:
#            _syntax_error(t, "a number")
#        t = lexer.next_token()
#        if t.kind != lex.RBRACKET:
#            _syntax_error(t, "]")
#        t = lexer.next_token()
    if t.kind != lex.SEMI:
        _syntax_error(t, "';'")
    return result
         
def _parse_interpreter_inst(tkn, lexer):
    opcode = None
    t = lexer.peek_token()
    if t.kind == lex.EQUALS:
        lexer.next_token()
        t = lexer.next_token()
        if t.kind != lex.NUMBER:
            _syntax_error(t, "a number")
        opcode = int(t.text)
        t = lexer.peek_token()
    if t.kind == lex.LBRACKET:
        qualifiers = _parse_qualifiers(lexer)
    else:
        qualifiers = []
    return InterpreterInstruction(tkn.text, opcode, tkn.location, qualifiers, _parse_code(lexer))
         
def parse_interpreter(lexer):
    '''
    Parses an intepreter definition file.
    @return: The syntax tree for the file
    @rtype: L{InterpreterTree}
    '''
    header = []
    _locals = []
    instructions = []
    t = lexer.next_token()
    while t.kind == lex.CPP:
        header.append(t)
        t = lexer.next_token()
    lexer.push_back(t)
    while True:
        t = lexer.next_token()
        if t.kind == lex.END_OF_FILE:
            return InterpreterTree(header, _locals, instructions)
        elif t.kind == lex.NAME:
            if t.text == 'locals':
                t = lexer.next_token()
                if t.kind != lex.LBRACE:
                    _syntax_error(t, "'{'")
                while True:
                    t = lexer.peek_token()
                    if t.kind == lex.RBRACE:
                        t = lexer.next_token()
                        break
                    _locals.append(_parse_local(lexer))
            else:
                instructions.append(_parse_interpreter_inst(t, lexer))
        else:
            _syntax_error(t, "a name")
            
def parse_transformation_file(lexer):
    '''
    Parses a transformation definition file.
    @return: The syntax tree for the file
    @rtype: C{list} of L{Transformation}
    '''
    header = []
    transformations = []
    t = lexer.next_token()
    while t.kind == lex.CPP:
        header.append(t)
        t = lexer.next_token()
    preamble = None
    while t.kind != lex.END_OF_FILE:
        if t.kind == lex.NAME and t.text == '__preamble':
            if lexer.peek_token().kind == lex.LBRACKET:
                qualifiers = _parse_qualifiers(lexer)
            else:
                qualifiers = []
            preamble = InterpreterInstruction('__preamble', None, t.location, 
                                              qualifiers, _parse_code(lexer))
        else:
            lexer.push_back(t)
            transformations.append(_parse_transformation(lexer))
        t = lexer.next_token()
    return TransformationTree(header, preamble, transformations)
    
if __name__ == '__main__':
    import sys
    print parse_interpreter(lex.Lexer(sys.argv[1]))
    

        
