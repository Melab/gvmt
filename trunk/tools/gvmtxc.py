import parser, lex, getopt, sys, gvmtas, gsc
import os, common, gvmtic, builtin, compound

options = {
    'h' : "Print this help and exit",
    'I file' : "Add file to list of #include file for lcc sub-process, can be used multiple times",
    'o outfile-name' : 'Specify output file name',
    'v' : 'Echo sub-processes invoked',
    'a' : 'Warn about non-ANSI C.',
    'b bytecode-header-file' : 'Specify bytecode header file',
    'n name' : 'Name of generated interpreter',
    'D symbol' : 'Define symbol in preprocessor',
    'e' : 'Explicit: All bytecodes must be explicitly defined',
    'z' : 'Do not put gc-safe-points on backward edges'
}        

if __name__ == '__main__':    
    opts, args = getopt.getopt(sys.argv[1:], 'I:b:ho:D:n:eL:z')
    #Parse options
    flags = []
    func_name = None
    explicit = False
    lcc_dir = None
    if len(args) != 2:
        common.print_usage(options, "slave-defn master-defn(.gsc)")
        sys.exit(1)
    try:       
        gc_safe = True
        for opt, value in opts:
            if opt == '-h':
                common.print_usage(options, "slave-defn master_defn(.gsc)")
                sys.exit(0)
            elif opt == '-I':
                flags.append('-I%s' % value)
            elif opt == '-b':
                bytecode_h = common.Out(value)
            elif opt == '-D':
                flags.append('-D%s' % value)
            elif opt == '-n':
                func_name = value
            elif opt == '-e':
                explicit = True
            elif opt == '-a':
                flags.append('-A')
            elif opt == '-z':
                gc_safe = False
            elif opt == '-L':
                lcc_dir = value
        if gc_safe:
            flags.append('-Wf-xgcsafe')
        int_file = gsc.read(common.In(args[1]))
        if not int_file.bytecodes or not int_file.bytecodes.master:
            raise common.UnlocatedException("%s does not define an interpreter\n" % args[0])
        trans_file = parser.parse_interpreter(lex.Lexer(args[0]))
        buf = common.Buffer()
        gvmtas.write_header(int_file.bytecodes, buf)
        trans = gvmtic.compile_instructions(trans_file, flags, lcc_dir, buf)
        # Generate new GSC file - Use trans where specified otherwise do nothing...
        int_instructions = {}
        for i in int_file.bytecodes.instructions:
            if 'private' not in i.qualifiers:
                int_instructions[i.name] = i
        gvmtas.assign_opcodes(int_file.bytecodes)
        names = set()
        default_instruction = None
        for i in trans.bytecodes.instructions:
            if i.name == '__default':
                default_instruction = i
        for i in trans.bytecodes.instructions:
            if 'private' not in i.qualifiers:
                names.add(i.name)
                if i.name not in int_instructions:
                    raise common.UnlocatedException("No instruction named '%s' in interpreter\n" % i.name)
                i_i = int_instructions[i.name]
                ops = i.flow_graph.deltas[0]
                i_ops = i_i.flow_graph.deltas[0]
                if ops != i_ops:
                    raise common.UnlocatedException("Operand counts do not match for %s\n" % i.name)
                if i.opcode is not None:
                    if i.opcode != i_i.opcode:
                        raise common.UnlocatedException("Opcodes do not match for %s\n" % i.name)
                else:
                    i.opcode = i_i.opcode
            else:
                if i.name in int_instructions:
                    raise common.UnlocatedException("'%s' is not private in interpreter\n" % i.name)
        for i in int_file.bytecodes.instructions:
            if i.name not in names and 'private' not in i.qualifiers:
                if explicit:
                    raise common.GVMTException("No explicit definition for %s" % i.name)
                # Create new no op instruction.
                ops = i.flow_graph.deltas[0]
                ins = [ builtin.instructions['#@'], builtin.instructions['DROP'] ] * ops
                if default_instruction:
                    ins.append(default_instruction)
                i = compound.CompoundInstruction(i.name, i.opcode, 
                                                 i.qualifiers, ins)
                trans.bytecodes.instructions.append(i)
        if func_name:
            trans.bytecodes.set_name(func_name)
        o = gvmtic.get_output(opts)
        if not o:
            if len(args) == 1:
                dir, f = os.path.split(args[0])
                o = common.Out(os.path.join(dir, f.split('.')[0] + '.gsc'))
            else:
                raise common.GVMTException("No output file specified");
        trans.write(o)
        o.close()
    except common.GVMTException, ex:
        print ex
        sys.exit(1)
#    except SyntaxError, ex:
#        print ex
#        sys.exit(1)
    
