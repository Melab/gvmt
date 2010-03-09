import sys, tex_instruction

def create_option_file(suffix):
    name = suffix
    __import__(name)
    mod = sys.modules[name]
    options = mod.options
    out = open('./%s_options.tex' % name, 'w')
    print >> out, '\\begin{description}'
    olist = [('-' + opt, options[opt]) for opt in options]
    olist.sort()
    for k, v in olist:
        print >> out, '\\item[%s] %s' % (tex_instruction.texify(k), tex_instruction.texify(v))
    print >> out, '\\end{description}'
    print >> out, ''
    out.close()

for suffix in ('ic', 'c', 'as', 'cc', 'link', 'xc'):
    create_option_file('gvmt' + suffix)
    create_option_file('objects')
