#Compound instructions: instructions made up of other instructions

import common, builtin, operators, gtypes
import flow_graph

def _redundant(l, uses, targetted):
    """Should match:
BRANCH_T(a) 1 TSTORE_4(c) HOP(b) TARGET(a) 0 TSTORE_4(c) TARGET(b) TLOAD_4(c).
Also no other BRANCH or HOP may target (a) or (b),
and their should be no other uses of (c)."""
    classes = ( builtin.Branch, builtin.Number, builtin.TStore, 
                builtin.Hop, builtin.Target, builtin.Number, 
                builtin.TStore, builtin.Target, builtin.TLoad )
    i = 0
    if len(l) < len(classes):
        return False
    assert len(l) == len(classes)
    for i in range(len(l)):
        if l[i].__class__ != classes[i]:
            return False
    # Additional checks.
    return (l[0].way and
            l[1].value == 1 and
            l[5].value == 0 and
            l[0].index == l[4].index and
            l[3].index == l[7].index and
            l[2].index == l[6].index and
            l[6].index == l[8].index and
            uses[l[2].index] == 1 and
            targetted[l[4].index] == 1 and
            targetted[l[7].index] == 1)
    
def _hop_threading(ilist):
    threadables = {}
    for i in range(1, len(ilist)):
        if (isinstance(ilist[i-1], builtin.Target) and
            isinstance(ilist[i], builtin.Hop)):
            threadables[ilist[i-1].index] = ilist[i].index
    for i in range(1, len(ilist)):
        if ilist[i].__class__ is builtin.Branch:
            if ilist[i].index in threadables:
                ilist[i] = builtin.Branch(threadables[ilist[i].index], ilist[i].way)
        elif ilist[i].__class__ is  builtin.Hop:
            if ilist[i].index in threadables:
                ilist[i] = builtin.Hop(threadables[ilist[i].index])
    return ilist
       
# This function cleans up the output from the current gvmt-lcc compiler.
def _lcc_post(ilist):
    ilist = _hop_threading(ilist)
    # Count of used jumps to targets.
    targetted = {}
    for i in ilist:
        if i.__class__ in (builtin.Branch, builtin.Hop):
            if i.index not in targetted:
                targetted[i.index] = 0
            targetted[i.index] += 1
    i = 0
    # Now remove pointless TARGETs
    while i < len(ilist):
        if ilist[i].__class__ is builtin.Target:
            if ilist[i].index not in targetted:
                ilist = ilist[:i] + ilist[i+1:]
        i += 1
    #Remove multiple LINE & FILEs
    i = 0
    while i < len(ilist):
        if ilist[i].__class__ in (builtin.Line, builtin.File):
            last_line = None
            last_file = None
            start = i
            while True:
                if ilist[i].__class__ is builtin.Line:
                    last_line = ilist[i]
                elif ilist[i].__class__ is builtin.File:
                    last_line = None
                    last_file = ilist[i]
                else:
                    break
                i += 1
                if i == len(ilist):
                    break
            replace = []
            if last_file:
                replace.append(last_file)
            if last_line:
                replace.append(last_line)
            assert replace
            ilist = ilist[:start] + replace + ilist[i:]
            i = start + len(replace)
        else:
            i += 1
    uses = {}
    for i in ilist:
        if i.__class__ is builtin.TLoad:
            uses[i.index] = uses.get(i.index, 0) + 1
    # lcc converts (a > b) to (a > b ? 1 : 0). So look for BRANCH_T 1 HOP TARGET 0 TARGET and invert test.
    # Need to check that no other BRANCHes or HOPs have TARGETs to be removed.
    i = 0
    while i < len(ilist):
        if ilist[i].__class__ is builtin.ComparisonOp and ilist[i].op in [
            operators.eq, operators.ne, operators.lt, 
            operators.gt, operators.le, operators.ge]:
            if _redundant(ilist[i+1: i + 10], uses, targetted):
                ilist = ilist[:i] + [ ilist[i].invert() ] + ilist[i+10:]
        i += 1
    # Remove unnecessary temps
    i = 0
    dead = set()
    while i < len(ilist):
        if ilist[i].__class__ is builtin.TStore:
            if ilist[i].index not in uses:
                dead.add(ilist[i].index)
            elif (uses[ilist[i].index] == 1 and 
                  ilist[i+1].__class__ is builtin.TLoad and 
                  ilist[i].index == ilist[i+1].index):
                dead.add(ilist[i].index)
        i += 1
    i = 0
    while i < len(ilist):
        if ilist[i].__class__ is builtin.TStore and ilist[i].index in dead:
            if ilist[i].index not in uses:
                if ilist[i].tipe.size > gtypes.p.size:
                    ilist[i] = builtin.Drop()
                    ilist = ilist[:i+1] + [ builtin.Drop() ] + ilist[i+1:]
                else:
                    ilist[i] = builtin.Drop()
                i += 1
            else:
                ilist = ilist[:i] + ilist[i+1:]
        elif (ilist[i].__class__ is builtin.TLoad or 
              ilist[i].__class__ is builtin.Name) and ilist[i].index in dead:
            ilist = ilist[:i] + ilist[i+1:]
        else:
            i += 1
    # Convert n #0 DROP_N to n * DROP (n < 4).
    i = 0
    while i < len(ilist)-2:
        if (ilist[i].__class__ is builtin.Number and ilist[i].value < 4
            and ilist[i+1].__class__ is builtin.Immediate and ilist[i+1].value == 0
            and ilist[i+2].__class__ is builtin.DropN):
            n = ilist[i].value
            ilist = ilist[:i] + [ builtin.Drop() ] * n + ilist[i+3:]
            i += n
        else:
            i += 1
    # Remove N DROP 
    i = 0
    while i < len(ilist)-1:
        if (ilist[i].__class__ is builtin.Number and 
            ilist[i+1].__class__ is  builtin.Drop):
            ilist = ilist[:i] + ilist[i+2:]
        else:
            i += 1
    # Remove any trailing LINE or FILE
    while ilist and (ilist[-1].__class__ is builtin.Line or
                      ilist[-1].__class__ is builtin.File):
        ilist = ilist[:-1]
    return ilist

class CompoundInstruction(builtin.Instruction):
                        
    def __init__(self, name, opcode, qualifiers, instructions):
        self.name = name
        self.opcode = opcode
        self.qualifiers = qualifiers
        for p in qualifiers:
            if p not in common.legal_qualifiers:
                raise common.UnlocatedException("Unrecognised qualifier '%s'"%p)
        self.flow_graph = flow_graph.FlowGraph(instructions)
        top = end = 0
        if_stack = []
        args = 0

    def process(self, mode):
        try:
            mode.compound(self.name, self.qualifiers, self.flow_graph)
        except common.UnlocatedException as ex:
            raise common.UnlocatedException(
                "%s in compound instruction %s" % (ex.msg, self.name))

    def top_level(self, mode):
        try:
            mode.top_level(self.name, self.qualifiers, self.flow_graph)
        except common.UnlocatedException, ex:
            raise common.UnlocatedException(
                "%s in compound instruction %s" % (ex.msg, self.name))

    def write_instructions(self, out):
        no_line = True
        for bb in self.flow_graph:
            for i in bb:
                if no_line:
                    no_line = False
                elif i.__class__ is builtin.File :
                    out << '\n'
                    no_line = True
                elif i.__class__ is builtin.Line :
                    out << '\n'
                elif i.__class__ is builtin.Comment :
                    no_line = True
                out << i.name << ' '
        
    def write(self, out):
        out << self.name 
        if self.opcode is not None:
            out << '=' << self.opcode
        if self.qualifiers:
            out << '['
            out << ' '.join(self.qualifiers)
            out << ']'
        out << ':\n'
        self.write_instructions(out)
        out << ';\n\n'
        
    def rename(self, renames):
        for bb in self.flow_graph:
            for i in bb:
                if i.__class__ is builtin.Address:
                    i.rename(renames)
        
    def lcc_post(self):
        insts = []
        for bb in self.flow_graph:
            insts += bb.instructions
        if self.name.startswith('gvmt_lcc_'):
            self.name = self.name[9:]
        insts = [ builtin.Comment('lcc-postprocessed') ] + _lcc_post(insts)
        self.flow_graph = flow_graph.FlowGraph(insts)
        
    def may_gc(self):
        return self.flow_graph.may_gc()

    def terminates_block(self):
        return self.flow_graph.ends_block()

