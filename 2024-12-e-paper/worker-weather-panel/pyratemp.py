'''
Source https://www.simple-is-better.org/template/pyratemp.html
License: MIT-like

'''
#!/usr/bin/env python
# -*- coding: utf-8 -*-
from __future__ import unicode_literals

__version__ = "0.3.2"
__author__   = ""
__license__  = ""

#=========================================

import os, re, sys, types
if sys.version_info[0] >= 3:
    import builtins
    unicode = str
    long = int
else:
    import __builtin__ as builtins
    from codecs import open

#=========================================
# some useful functions

#----------------------
# string-position: i <-> row,col

def srow(string, i):
 
    return string.count("\n", 0, max(0, i)) + 1

def scol(string, i):
  
    return i - string.rfind("\n", 0, max(0, i))

def sindex(string, row, col):
    
    n = 0
    for _ in range(row-1):
        n = string.find("\n", n) + 1
    return n+col-1

#----------------------

def dictkeyclean(d):
  
    new_d = {}
    for k, v in d.items():
        new_d[str(k)] = v
    return new_d

#----------------------

def dummy(*_, **__):
   
    pass

def dummy_raise(exception, value):
   
    def mydummy(*_, **__):
        raise exception(value)
    return mydummy

#=========================================
# escaping

(NONE, HTML, LATEX, MAIL_HEADER) = range(0, 4)
ESCAPE_SUPPORTED = {"NONE":None, "HTML":HTML, "LATEX":LATEX, "MAIL_HEADER":MAIL_HEADER}

def escape(s, format=HTML):
   
    #Note: If you have to make sure that every character gets replaced
    #      only once (and if you cannot achieve this with the following code),
    #      use something like "".join([replacedict.get(c,c) for c in s])
    #      which is about 2-3 times slower (but maybe needs less memory).
    #Note: This is one of the most time-consuming parts of the template.
    if format is None or format == NONE:
        pass
    elif format == HTML:
        s = s.replace("&", "&amp;") # must be done first!
        s = s.replace("<", "&lt;")
        s = s.replace(">", "&gt;")
        s = s.replace("\"", "&quot;")
        s = s.replace("\"", "&#39;")
    elif format == LATEX:
        s = s.replace("\\", "\\x")    #must be done first!
        s = s.replace("#",  "\\#")
        s = s.replace("$",  "\\$")
        s = s.replace("%",  "\\%")
        s = s.replace("&",  "\\&")
        s = s.replace("_",  "\\_")
        s = s.replace("{",  "\\{")
        s = s.replace("}",  "\\}")
        s = s.replace("\\x","\\textbackslash{}")
        s = s.replace("~",  "\\textasciitilde{}")
        s = s.replace("^",  "\\textasciicircum{}")
    elif format == MAIL_HEADER:
        import email.header
        try:
            s.encode("ascii")
            return s
        except UnicodeEncodeError:
            return email.header.make_header([(s, "utf-8")]).encode()
    else:
        raise ValueError("Invalid format (only None, HTML, LATEX and MAIL_HEADER are supported).")
    return s

#=========================================

#-----------------------------------------
# Exceptions

class TemplateException(Exception):
    pass

class TemplateParseError(TemplateException):
    def __init__(self, err, errpos):
       
        self.err = err
        self.filename, self.row, self.col = errpos
        TemplateException.__init__(self)
    def __str__(self):
        if not self.filename:
            return "line %d, col %d: %s" % (self.row, self.col, str(self.err))
        else:
            return "file %s, line %d, col %d: %s" % (self.filename, self.row, self.col, str(self.err))

class TemplateSyntaxError(TemplateParseError, SyntaxError):
    pass

class TemplateIncludeError(TemplateParseError):
    pass

class TemplateRenderError(TemplateException):
    pass

#-----------------------------------------
# Loader

class LoaderString:
    
    def __init__(self, encoding="utf-8"):
        self.encoding = encoding

    def load(self, s):
        if isinstance(s, unicode):
            u = s
        else:
            u = s.decode(self.encoding)
        return u

class LoaderFile:

    def __init__(self, allowed_path=None, encoding="utf-8"):
       
        if allowed_path and not os.path.isdir(allowed_path):
            raise ValueError("allowed_path has to be a directory.")
        self.path     = allowed_path
        self.encoding = encoding

    def load(self, filename):
       
        if filename != os.path.basename(filename):
            raise ValueError("No path allowed in filename. (%s)" %(filename))
        filename = os.path.join(self.path, filename)

        f = open(filename, "r", encoding=self.encoding)
        u = f.read()
        f.close()

        return u

#-----------------------------------------
# Parser

class Parser(object):
    # template-syntax
    _comment_start = "#!"
    _comment_end   = "!#"
    _sub_start     = "$!"
    _sub_end       = "!$"
    _subesc_start  = "@!"
    _subesc_end    = "!@"
    _block_start   = "<!--("
    _block_end     = ")-->"

    # build regexps
    # comment
    #   single-line, until end-tag or end-of-line.
    _strComment = r"""%s(?P<content>.*?)(?P<end>%s|\n|$)""" \
                    % (re.escape(_comment_start), re.escape(_comment_end))
    _reComment = re.compile(_strComment, re.M)

    # escaped or unescaped substitution
    #   single-line ("|$" is needed to be able to generate good error-messges)
    _strSubstitution = r"""
                    (
                    %s\s*(?P<sub>.*?)\s*(?P<end>%s|$)       #substitution
                    |
                    %s\s*(?P<escsub>.*?)\s*(?P<escend>%s|$) #escaped substitution
                    )
                """ % (re.escape(_sub_start),    re.escape(_sub_end),
                       re.escape(_subesc_start), re.escape(_subesc_end))
    _reSubstitution = re.compile(_strSubstitution, re.X|re.M)

    # block
    #   - single-line, no nesting.
    #   or
    #   - multi-line, nested by whitespace indentation:
    #       * start- and end-tag of a block must have exactly the same indentation.
    #       * start- and end-tags of *nested* blocks should have a greater indentation.
    # NOTE: A single-line block must not start at beginning of the line with
    #       the same indentation as the enclosing multi-line blocks!
    #       Note that "       " and "\t" are different, although they may
    #       look the same in an editor!
    _s = re.escape(_block_start)
    _e = re.escape(_block_end)
    _strBlock = r"""
                    ^(?P<mEnd>[ \t]*)%send%s(?P<meIgnored>.*)\r?\n?   # multi-line end  (^   <!--(end)-->IGNORED_TEXT\n)
                    |
                    (?P<sEnd>)%send%s                               # single-line end (<!--(end)-->)
                    |
                    (?P<sSpace>[ \t]*)                              # single-line tag (no nesting)
                    %s(?P<sKeyw>\w+)[ \t]*(?P<sParam>.*?)%s
                    (?P<sContent>.*?)
                    (?=(?:%s.*?%s.*?)??%send%s)                     # (match until end or i.e. <!--(elif/else...)-->)
                    |
                                                                    # multi-line tag, nested by whitespace indentation
                    ^(?P<indent>[ \t]*)                             #   save indentation of start tag
                    %s(?P<mKeyw>\w+)\s*(?P<mParam>.*?)%s(?P<mIgnored>.*)\r?\n
                    (?P<mContent>(?:.*\n)*?)
                    (?=(?P=indent)%s(?:.|\s)*?%s)                   #   match indentation
                """ % (_s, _e,
                       _s, _e,
                       _s, _e, _s, _e, _s, _e,
                       _s, _e, _s, _e)
    _reBlock = re.compile(_strBlock, re.X|re.M)

    # "for"-block parameters: "var(,var)* in ..."
    _strForParam = r"""^(?P<names>\w+(?:\s*,\s*\w+)*)\s+in\s+(?P<iter>.+)$"""
    _reForParam  = re.compile(_strForParam)

    # allowed macro-names
    _reMacroParam = re.compile(r"""^\w+$""")


    def __init__(self, loadfunc=None, testexpr=None, escape=HTML):
      
        if loadfunc is None:
            self._load = dummy_raise(NotImplementedError, "include not supported, since no loadfunc was given.")
        else:
            self._load = loadfunc

        if testexpr is None:
            self._testexprfunc = dummy
        else:
            try:    # test if testexpr() works
                testexpr("i==1")
            except Exception as err:
                raise ValueError("Invalid testexpr. (%s)" %(err))
            self._testexprfunc = testexpr

        if escape not in ESCAPE_SUPPORTED.values():
            raise ValueError("Unsupported escape. (%s)" %(escape))
        self.escape = escape
        self._includestack = []

    def parse(self, template):
       
        self._includestack = [(None, template)]   # for error-messages (_errpos)
        return self._parse(template)

    def _errpos(self, fpos):
        filename, string = self._includestack[-1]
        return filename, srow(string, fpos), scol(string, fpos)

    def _testexpr(self, expr,  fpos=0):
        try:
            self._testexprfunc(expr)
        except SyntaxError as err:
            raise TemplateSyntaxError(err, self._errpos(fpos))

    def _parse_sub(self, parsetree, text, fpos=0):
        curr = 0
        for match in self._reSubstitution.finditer(text):
            start = match.start()
            if start > curr:
                parsetree.append(("str", self._reComment.sub("", text[curr:start])))

            if match.group("sub") is not None:
                if not match.group("end"):
                    raise TemplateSyntaxError("Missing closing tag %s for %s."
                            % (self._sub_end, match.group()), self._errpos(fpos+start))
                if len(match.group("sub")) > 0:
                    self._testexpr(match.group("sub"), fpos+start)
                    parsetree.append(("sub", match.group("sub")))
            else:
                assert(match.group("escsub") is not None)
                if not match.group("escend"):
                    raise TemplateSyntaxError("Missing closing tag %s for %s."
                            % (self._subesc_end, match.group()), self._errpos(fpos+start))
                if len(match.group("escsub")) > 0:
                    self._testexpr(match.group("escsub"), fpos+start)
                    parsetree.append(("esc", self.escape, match.group("escsub")))

            curr = match.end()

        if len(text) > curr:
            parsetree.append(("str", self._reComment.sub("", text[curr:])))

    def _parse(self, template, fpos=0):
        
        # blank out comments
        # (So that its content does not collide with other syntax, and
        #  because removing them completely would falsify the character-
        #  position ("match.start()") of error-messages)
        template = self._reComment.sub(lambda match: self._comment_start+" "*len(match.group(1))+match.group(2), template)

        # init parser
        parsetree = []
        curr = 0            # current position (= end of previous block)
        block_type = None   # block type: if,for,macro,raw,...
        block_indent = None # None: single-line, >=0: multi-line

        # find blocks
        for match in self._reBlock.finditer(template):
            start = match.start()
            # process template-part before this block
            if start > curr:
                self._parse_sub(parsetree, template[curr:start], fpos)

            # analyze block syntax (incl. error-checking and -messages)
            keyword = None
            block = match.groupdict()
            pos__ = fpos + start                # shortcut
            if   block["sKeyw"] is not None:    # single-line block tag
                block_indent = None
                keyword = block["sKeyw"]
                param   = block["sParam"]
                content = block["sContent"]
                if block["sSpace"]:             # restore spaces before start-tag
                    if len(parsetree) > 0 and parsetree[-1][0] == "str":
                        parsetree[-1] = ("str", parsetree[-1][1] + block["sSpace"])
                    else:
                        parsetree.append(("str", block["sSpace"]))
                pos_p = fpos + match.start("sParam")    # shortcuts
                pos_c = fpos + match.start("sContent")
            elif block["mKeyw"] is not None:    # multi-line block tag
                block_indent = len(block["indent"])
                keyword = block["mKeyw"]
                param   = block["mParam"]
                content = block["mContent"]
                pos_p = fpos + match.start("mParam")
                pos_c = fpos + match.start("mContent")
                ignored = block["mIgnored"].strip()
                if ignored  and  ignored != self._comment_start:
                    raise TemplateSyntaxError("No code allowed after block-tag.", self._errpos(fpos+match.start("mIgnored")))
            elif block["mEnd"] is not None:     # multi-line block end
                if block_type is None:
                    raise TemplateSyntaxError("No block to end here/invalid indent.", self._errpos(pos__) )
                if block_indent != len(block["mEnd"]):
                    raise TemplateSyntaxError("Invalid indent for end-tag.", self._errpos(pos__) )
                ignored = block["meIgnored"].strip()
                if ignored  and  ignored != self._comment_start:
                    raise TemplateSyntaxError("No code allowed after end-tag.", self._errpos(fpos+match.start("meIgnored")))
                block_type = None
            elif block["sEnd"] is not None:     # single-line block end
                if block_type is None:
                    raise TemplateSyntaxError("No block to end here/invalid indent.", self._errpos(pos__))
                if block_indent is not None:
                    raise TemplateSyntaxError("Invalid indent for end-tag.", self._errpos(pos__))
                block_type = None
            else:
                raise TemplateException("FATAL: Block regexp error. Please contact the author. (%s)" % match.group())

            # analyze block content (mainly error-checking and -messages)
            if keyword:
                keyword = keyword.lower()
                if   "for"   == keyword:
                    if block_type is not None:
                        raise TemplateSyntaxError("Missing block-end-tag before new block at %s." %(match.group()), self._errpos(pos__))
                    block_type = "for"
                    cond = self._reForParam.match(param)
                    if cond is None:
                        raise TemplateSyntaxError("Invalid for ... at %s." %(param), self._errpos(pos_p))
                    names = tuple(n.strip()  for n in cond.group("names").split(","))
                    self._testexpr(cond.group("iter"), pos_p+cond.start("iter"))
                    parsetree.append(("for", names, cond.group("iter"), self._parse(content, pos_c)))
                elif "if"    == keyword:
                    if block_type is not None:
                        raise TemplateSyntaxError("Missing block-end-tag before new block at %s." %(match.group()), self._errpos(pos__))
                    if not param:
                        raise TemplateSyntaxError("Missing condition for if at %s." %(match.group()), self._errpos(pos__))
                    block_type = "if"
                    self._testexpr(param, pos_p)
                    parsetree.append(("if", param, self._parse(content, pos_c)))
                elif "elif"  == keyword:
                    if block_type != "if":
                        raise TemplateSyntaxError("elif may only appear after if at %s." %(match.group()), self._errpos(pos__))
                    if not param:
                        raise TemplateSyntaxError("Missing condition for elif at %s." %(match.group()), self._errpos(pos__))
                    self._testexpr(param, pos_p)
                    parsetree.append(("elif", param, self._parse(content, pos_c)))
                elif "else"  == keyword:
                    if block_type not in ("if", "for"):
                        raise TemplateSyntaxError("else may only appear after if or for at %s." %(match.group()), self._errpos(pos__))
                    if param:
                        raise TemplateSyntaxError("else may not have parameters at %s." %(match.group()), self._errpos(pos__))
                    parsetree.append(("else", self._parse(content, pos_c)))
                elif "macro" == keyword:
                    if block_type is not None:
                        raise TemplateSyntaxError("Missing block-end-tag before new block %s." %(match.group()), self._errpos(pos__))
                    block_type = "macro"
                    # make sure param is "\w+" (instead of ".+")
                    if not param:
                        raise TemplateSyntaxError("Missing name for macro at %s." %(match.group()), self._errpos(pos__))
                    if not self._reMacroParam.match(param):
                        raise TemplateSyntaxError("Invalid name for macro at %s." %(match.group()), self._errpos(pos__))
                    #remove last newline
                    if len(content) > 0 and content[-1] == "\n":
                        content = content[:-1]
                    if len(content) > 0 and content[-1] == "\r":
                        content = content[:-1]
                    parsetree.append(("macro", param, self._parse(content, pos_c)))

                # parser-commands
                elif "raw"   == keyword:
                    if block_type is not None:
                        raise TemplateSyntaxError("Missing block-end-tag before new block %s." %(match.group()), self._errpos(pos__))
                    if param:
                        raise TemplateSyntaxError("raw may not have parameters at %s." %(match.group()), self._errpos(pos__))
                    block_type = "raw"
                    parsetree.append(("str", content))
                elif "include" == keyword:
                    if block_type is not None:
                        raise TemplateSyntaxError("Missing block-end-tag before new block %s." %(match.group()), self._errpos(pos__))
                    if param:
                        raise TemplateSyntaxError("include may not have parameters at %s." %(match.group()), self._errpos(pos__))
                    block_type = "include"
                    try:
                        u = self._load(content.strip())
                    except Exception as err:
                        raise TemplateIncludeError(err, self._errpos(pos__))
                    self._includestack.append((content.strip(), u))  # current filename/template for error-msg.
                    p = self._parse(u)
                    self._includestack.pop()
                    parsetree.extend(p)
                elif "set_escape" == keyword:
                    if block_type is not None:
                        raise TemplateSyntaxError("Missing block-end-tag before new block %s." %(match.group()), self._errpos(pos__))
                    if param:
                        raise TemplateSyntaxError("set_escape may not have parameters at %s." %(match.group()), self._errpos(pos__))
                    block_type = "set_escape"
                    esc = content.strip().upper()
                    if esc not in ESCAPE_SUPPORTED:
                        raise TemplateSyntaxError("Unsupported escape %s." %(esc), self._errpos(pos__))
                    self.escape = ESCAPE_SUPPORTED[esc]
                else:
                    raise TemplateSyntaxError("Invalid keyword %s." %(keyword), self._errpos(pos__))
            curr = match.end()

        if block_type is not None:
            raise TemplateSyntaxError("Missing end-tag.", self._errpos(pos__))

        if len(template) > curr:            # process template-part after last block
            self._parse_sub(parsetree, template[curr:], fpos+curr)

        return parsetree

#-----------------------------------------
# Evaluation

# some checks
assert len(eval("dir()", {"__builtins__":{"dir":dir}})) == 1, \
    "FATAL: eval does not work as expected (%s)."
assert compile("0 .__class__", "<string>", "eval").co_names == ("__class__",), \
    "FATAL: compile does not work as expected."

class EvalPseudoSandbox:
  

    safe_builtins = {
        "True"      : True,
        "False"     : False,
        "None"      : None,

        "abs"       : builtins.abs,
        "chr"       : builtins.chr,
        "divmod"    : builtins.divmod,
        "hash"      : builtins.hash,
        "hex"       : builtins.hex,
        "isinstance": builtins.isinstance,
        "len"       : builtins.len,
        "max"       : builtins.max,
        "min"       : builtins.min,
        "oct"       : builtins.oct,
        "ord"       : builtins.ord,
        "pow"       : builtins.pow,
        "range"     : builtins.range,
        "round"     : builtins.round,
        "sorted"    : builtins.sorted,
        "sum"       : builtins.sum,
        "unichr"    : builtins.chr,
        "zip"       : builtins.zip,

        "bool"      : builtins.bool,
        "bytes"     : builtins.bytes,
        "complex"   : builtins.complex,
        "dict"      : builtins.dict,
        "enumerate" : builtins.enumerate,
        "float"     : builtins.float,
        "int"       : builtins.int,
        "list"      : builtins.list,
        "long"      : long,
        "reversed"  : builtins.reversed,
        "set"       : builtins.set,
        "str"       : builtins.str,
        "tuple"     : builtins.tuple,
        "unicode"   : unicode,

        "dir"       : builtins.dir,
    }
    if sys.version_info[0] < 3:
        safe_builtins["unichr"] = builtins.unichr

    def __init__(self):
        self._compile_cache = {}
        self.vars_ptr = None
        self.eval_allowed_builtins = self.safe_builtins.copy()
        self.register("__import__", self.f_import)
        self.register("exists",  self.f_exists)
        self.register("default", self.f_default)
        self.register("setvar",  self.f_setvar)
        self.register("escape",  self.f_escape)

    def register(self, name, obj):
     
        self.eval_allowed_builtins[name] = obj

    def _check_code_names(self, code, expr):
        
        for name in code.co_names:
            if name[0] == "_" and name != "_[1]":  # _[1] is necessary for [x for x in y]
                raise NameError("Name %s is not allowed in %s." % (name, expr))
        # recursively check sub-codes (e.g. lambdas)
        for const in code.co_consts:
            if isinstance(const, types.CodeType):
                self._check_code_names(const, expr)

    def compile(self, expr):
        
        if expr not in self._compile_cache:
            c = compile(expr, "", "eval")
            self._check_code_names(c, expr)
            self._compile_cache[expr] = c
        return self._compile_cache[expr]

    def eval(self, expr, variables):
     
        sav = self.vars_ptr
        self.vars_ptr = variables

        try:
            x = eval(self.compile(expr), {"__builtins__": self.eval_allowed_builtins}, variables)
        except NameError:
            # workaround for lambdas like ``sorted(..., key=lambda x: my_f(x))``
            vars2 = {"__builtins__": self.eval_allowed_builtins}
            vars2.update(variables)
            x = eval(self.compile(expr), vars2)

        self.vars_ptr = sav
        return x

    def f_import(self, name, *_, **__):
  
        if self.vars_ptr is not None  and  name in self.vars_ptr  and  isinstance(self.vars_ptr[name], types.ModuleType):
            return self.vars_ptr[name]
        else:
            raise ImportError("import not allowed in pseudo-sandbox; try to import %s yourself (and maybe pass it to the sandbox/template)" % name)

    def f_exists(self, varname):
      
        return (varname in self.vars_ptr)

    def f_default(self, expr, default=None):
      
        try:
            r = self.eval(expr, self.vars_ptr)
            if r is None:
                return default
            return r
        #TODO: which exceptions should be catched here?
        except (NameError, LookupError, TypeError, AttributeError):
            return default

    def f_setvar(self, name, expr):

        self.vars_ptr[name] = self.eval(expr, self.vars_ptr)
        return ""

    def f_escape(self, s, format="HTML"):
   
        if isinstance(format, (str, unicode)):
            format = ESCAPE_SUPPORTED[format.upper()]
        return escape(unicode(s), format)

#-----------------------------------------
# basic template / subtemplate

class TemplateBase:
  

    def __init__(self, parsetree, renderfunc, data=None):

        #TODO: parameter-checking?
        self.parsetree = parsetree
        if isinstance(data, dict):
            self.data = data
        elif data is None:
            self.data = {}
        else:
            raise TypeError("data must be a dict (or None).")
        self.current_data = data
        self._render = renderfunc

    def __call__(self, **override):
    
        self.current_data = self.data.copy()
        self.current_data.update(override)
        u = "".join(self._render(self.parsetree, self.current_data))
        self.current_data = self.data       # restore current_data
        return _dontescape(u)               # (see class _dontescape)

    def __unicode__(self):
        return self.__call__()
    def __str__(self):
        return self.__call__()

#-----------------------------------------
# Renderer

class _dontescape(unicode):

    __slots__ = []


class Renderer(object):
  
    def __init__(self, evalfunc, escapefunc):
        
        #TODO: test evalfunc
        self.evalfunc = evalfunc
        self.escapefunc = escapefunc

    def _eval(self, expr, data):
        try:
            return self.evalfunc(expr, data)
        #TODO: any other errors to catch here?
        except (TypeError, NameError, LookupError, AttributeError, SyntaxError) as err:
            raise TemplateRenderError("Cannot eval expression %s. (%s: %s)" %(expr, err.__class__.__name__, err))

    def render(self, parsetree, data):
   
        _eval = self._eval  # shortcut
        output = []
        do_else = False     # use else/elif-branch?

        if parsetree is None:
            return ""
        for elem in parsetree:
            if   "str"   == elem[0]:
                output.append(elem[1])
            elif "sub"   == elem[0]:
                output.append(unicode(_eval(elem[1], data)))
            elif "esc"   == elem[0]:
                obj = _eval(elem[2], data)
                #prevent double-escape
                if isinstance(obj, _dontescape) or isinstance(obj, TemplateBase):
                    output.append(unicode(obj))
                else:
                    output.append(self.escapefunc(unicode(obj), elem[1]))
            elif "for"   == elem[0]:
                do_else = True
                (names, iterable) = elem[1:3]
                try:
                    loop_iter = iter(_eval(iterable, data))
                except TypeError:
                    raise TemplateRenderError("Cannot loop over %s." % iterable)
                for i in loop_iter:
                    do_else = False
                    if len(names) == 1:
                        data[names[0]] = i
                    else:
                        data.update(zip(names, i))   #"for a,b,.. in list"
                    output.extend(self.render(elem[3], data))
            elif "if"    == elem[0]:
                do_else = True
                if _eval(elem[1], data):
                    do_else = False
                    output.extend(self.render(elem[2], data))
            elif "elif"  == elem[0]:
                if do_else and _eval(elem[1], data):
                    do_else = False
                    output.extend(self.render(elem[2], data))
            elif "else"  == elem[0]:
                if do_else:
                    do_else = False
                    output.extend(self.render(elem[1], data))
            elif "macro" == elem[0]:
                data[elem[1]] = TemplateBase(elem[2], self.render, data)
            else:
                raise TemplateRenderError("Invalid parse-tree (%s)." %(elem))

        return output

#-----------------------------------------
# template user-interface (putting it all together)

class Template(TemplateBase):


    def __init__(self, string=None,filename=None,parsetree=None, encoding="utf-8", data=None, escape=HTML,
            loader_class=LoaderFile,
            parser_class=Parser,
            renderer_class=Renderer,
            eval_class=EvalPseudoSandbox,
            escape_func=escape):
        if [string, filename, parsetree].count(None) != 2:
            raise ValueError("Exactly 1 of string,filename,parsetree is necessary.")

        tmpl = None
        # load template
        if filename is not None:
            incl_load = loader_class(os.path.dirname(filename), encoding).load
            tmpl = incl_load(os.path.basename(filename))
        if string is not None:
            incl_load = dummy_raise(NotImplementedError, "include not supported for template-strings.")
            tmpl = LoaderString(encoding).load(string)

        # eval (incl. compile-cache)
        templateeval = eval_class()

        # parse
        if tmpl is not None:
            p = parser_class(loadfunc=incl_load, testexpr=templateeval.compile, escape=escape)
            parsetree = p.parse(tmpl)
            del p

        # renderer
        renderfunc = renderer_class(templateeval.eval, escape_func).render

        #create template
        TemplateBase.__init__(self, parsetree, renderfunc, data)


#=========================================
