#!/usr/bin/python

#Copyright (C) 2008 Mark Shannon
#GNU General Public License
#For copyright terms see: http://www.gnu.org/copyleft/gpl.html

#Lexer code for the GVMT front-end components.

import re
from common import GVMTException

#try:
#    from collections import namedtuple
#except:
#    from namedtuple import namedtuple
    
class Location(object):
    '''
    @ivar file: name of the file
    @ivar line: line number
    '''
    def __init__(self, file, line):
        self.file = file
        self.line = line

    def __str__(self):
        return '%s:%s' % (self.file, self.line) 

class Token(object):
    
    __slots__ = [ 'kind', 'text', 'location' ]
    
    def __init__(self, kind, text, file, line):
        self.kind = kind
        self.text = text
        self.location = Location(file, line)
        
    def __repr__(self):
        return 'Token(%s, %r, %s)' % (kind_names[self.kind], 
                                      self.text, self.location) 

    #Kinds
NAME = 1
COLON = 2
SEMI = 3
LBRACE = 4
RBRACE = 5
ADDRESS = 6
STREAM = 7
DOUBLE_DASH = 8
LPAREN = 9
RPAREN = 10
STRING = 11
COMMA = 12
STREAM_INST = 13
STREAM_NAME = 14
CPP = 15
OTHER = 16
NUMBER = 17
LBRACKET = 18
RBRACKET = 19
STAR = 20
DOTS = 21
EQUALS = 22
ERROR = 23

kind_names = (
    "**ERROR**",
    "NAME",
    "COLON",
    "SEMI",
    "LBRACE",
    "RBRACE",
    "ADDRESS",
    "STREAM",
    "DOUBLE_DASH",
    "LPAREN",
    "RPAREN",
    "STRING",
    "COMMA",
    "STREAM_INST",
    "STREAM_NAME",
    "CPP",
    "OTHER",
    "NUMBER",
    "LBRACKET",
    "RBRACKET",
    "STAR",
    "...",
    "EQUALS",
    "**ERROR**",
    "END OF FILE"
)

END_OF_FILE = -1

_C_COMMENT = r'/\*.*?\*/'
LINE_COMMENT = r'//.*'
_NAME = r'[A-Za-z_][A-Za-z0-9_]*'
_SEMI = ';'
_COLON = ':'
_COMMA = ','
_LBRACE = r'\{'
_RBRACE = r'\}'
_LPAREN = r'\('
_RPAREN = r'\)'
_LBRACKET = r'\['
_RBRACKET = r'\]'
_NUMBER = r'0x[a-fA-F0-9]+|\d+'
_ADDRESS = r'&(%s)' % _NAME
_DOUBLE_DASH = '--'
_STRING = r'"[^"]*?"'
_C_CHAR = r"'[^']+?'"
_STREAM_INST = r'#[+-@]|#\d+|#\[\d+\]'
_STREAM_NAME = r'#+%s' % _NAME

_OTHER = (r'\.\.\.|->|<<=|>>=|<<|>>|==|!=|>=|<=|\|=|&=|\+=|-=|\*=|/=|' +
          r'\+\+|\|\||\^|&&|\||&|\+|-|\*|%|/|\?|\<|\>|=|!|~|\.')

_others = set('+-/<>!~?.')
_others.add('>>=')
_others.add('<<=')
_others.add('>>')
_others.add('<<')
_others.add('==')
_others.add('++')
_others.add('!=')
_others.add('>=')
_others.add('<=')
_others.add('->')
_others.add('||')
_others.add('&&')
_others.add('|')
_others.add('&')
_others.add('^')
_others.add('+=')
_others.add('-=')
_others.add('/=')
_others.add('*=')
_others.add('&=')
_others.add('|=')

_MATCHER = re.compile(
    r'^(%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s)' %
    ( _C_COMMENT, _NAME, _SEMI, _COLON, _LBRACE, _RBRACE, LINE_COMMENT,
      _LPAREN, _RPAREN, _DOUBLE_DASH, _STRING, _ADDRESS, _OTHER, _C_CHAR, 
      _STREAM_INST, _STREAM_NAME, _COMMA, _NUMBER, _LBRACKET, _RBRACKET,
    )
)

_kinds = { ';' : SEMI, ':' : COLON, '(' : LPAREN , '{' : LBRACE, 
           ')' : RPAREN , '}' : RBRACE, ',' : COMMA, '[' : LBRACKET, 
           ']' : RBRACKET, '*' : STAR, '...' : DOTS, '=' : EQUALS
}

class Lexer(object):
    '''Lexer for GVMT front-end components'''
       
    def __init__(self, file, filename = None):
        '''Creates a lexer for file. 
        Uses filename as name or file.name if none provided'''
        if isinstance(file, str):
            self.file = open(file, 'r')
            self.filename = file
        else:
            self.file = file
            if filename is None:
                try:
                    self.filename = file.name
                except:
                    self.filename = ''
            else:
                self.filename = filename
        self.line = None
        self.line_no = 0
        self.pending = []
    
    def _tokenise_line(self, line):
        tokens = []
        line = line.lstrip()
        while line:
            m = _MATCHER.match(line)
            if m:
                text = m.group(0)
                if text[0] == '#':
                    if text[1] in '+-[0123456789@':
                        kind = STREAM_INST
                    else:
                        kind = STREAM_NAME
                elif text[0] == '&':
                    kind = ADDRESS
                elif text in _others:
                    kind = OTHER
                elif text in _kinds:
                    kind = _kinds[text]
                elif text[0]  == '"':
                    kind = STRING
                elif text[0]  == "'":
                    kind = STRING
                elif text[0] in '0123456789':
                    kind = NUMBER
                elif text[0] == '/':
                    #Comment
                    line = line[len(text):].lstrip()
                    continue
                elif text == '--':
                    kind = DOUBLE_DASH
                else:
                    kind = NAME
                tokens.append(Token(kind, text, self.filename, self.line_no))
                line = line[len(text):].lstrip()
            else:
                raise GVMTException("%s:%s: Syntax Error. Illegal token '%s'\n" 
                            % (self.filename, self.line_no, line.split()[0]))
        return tokens
        
    def _queue_tokens(self):
        while not self.pending:
            line = self.file.readline()
            self.line_no += 1
            if not line:
                self.pending = [ 
                    Token(END_OF_FILE, '', self.filename, self.line_no) ]
                return
            if line[0] == '#':
                line = line.rstrip()
                self.pending = [Token(CPP, line, self.filename, self.line_no)]
                return
            self.pending = self._tokenise_line(line)
            self.pending.reverse()
       
    def next_token(self):
        '''
        Fetches the next token.
        @return: The next token
        @rtype: Token
        '''
        self._queue_tokens()
        return self.pending.pop()
        
    def peek_token(self):
        '''
        Returns the next token without modifying the state.
        @return: The next token
        @rtype: Token
        '''
        self._queue_tokens()
        return self.pending[-1]
        
    def push_back(self, tkn):
        '''
        Pushes a token to the stream.
        @param tkn: The token to push back
        @type tkn: Token
        '''
        assert isinstance(tkn, Token)
        self.pending.append(tkn)
        
        
if __name__ == '__main__':
    import sys
    lexer = Lexer(sys.argv[1])
    tkn = Token(0, '', '', 0)
    while tkn.kind != END_OF_FILE:
        tkn = lexer.next_token()
        print tkn
