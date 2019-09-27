from clang.cindex import *
from multiprocessing import Process, Queue, cpu_count
import sys, subprocess, json, time
import re, os
from tqdm import tqdm
from colorama import init, deinit, Fore, Back, Style
from pathlib import Path

init()

project_root = '/'
exclude_files = ['/projects/ECS/EventGui.cpp']
subscribe_calls = Queue()
publish_calls = Queue()
call_calls = Queue()
register_calls = Queue()

max_processes = max(2, cpu_count()*2)

completed_processes = Queue()
sources_to_scan = []

# check if the cursor's type is a template
def is_template(node):
    return hasattr(node, 'type') and node.get_num_template_arguments() != -1

potential_parent = None
def visit_node(node, subscribe_calls, publish_calls, register_calls, call_calls):
    global potential_parent
    if node is None:
        return

    if node.spelling is None or node.kind is None:
        return

    if node.kind != CursorKind.TRANSLATION_UNIT:
        if (node.location.file is None):
            return

        if project_root not in str(node.location.file):
            return

    if node.kind in [CursorKind.FUNCTION_DECL, node.kind == CursorKind.CONSTRUCTOR, CursorKind.CXX_METHOD]:
        potential_parent = node
    if node.kind == CursorKind.CALL_EXPR:
        if node.spelling == "subscribe":
            sem = node.get_definition().semantic_parent
            if sem and sem.type.spelling.startswith('ES::EventSystem'):
                subscribe_calls.append({'call': node, 'parent': potential_parent})

        elif node.spelling == "publish":
            sem = node.get_definition().semantic_parent
            if sem and sem.type.spelling.startswith('ES::EventSystem'):
                publish_calls.append({'call': node, 'parent': potential_parent})

        elif node.spelling == "call":
            sem = node.get_definition().semantic_parent
            if sem and sem.type.spelling.startswith('ES::SubscriberCollection'):
                call_calls.append({'call': node, 'parent': potential_parent})

        elif node.spelling == "registerEvent":
            sem = node.get_definition().semantic_parent
            if sem and sem.type.spelling.startswith('ES::EventSystem'):
                register_calls.append({'call': node, 'parent': potential_parent})

def fully_qualified_name(c):
    if c is None:
        return ''
    elif c.kind == CursorKind.TRANSLATION_UNIT:
        return ''
    else:
        res = fully_qualified(c.semantic_parent)
        if res != '':
            return res + '::' + c.spelling
    return c.spelling

def recurse_children(parent, depth):
    if depth > 20:
        print('='*depth, ' BAILING')
        return
    print('-'*depth, str(parent.spelling), str(parent.kind), str(parent.type.get_canonical().spelling), str(parent.get_num_template_arguments()))
    if parent.kind == CursorKind.MEMBER_REF_EXPR:
        refd = parent.referenced
        print('+'*depth, str(refd.spelling), str(refd.kind), str(refd.type.spelling), str(refd.get_num_template_arguments()))
        recurse_children(refd, depth+1)
    elif parent.kind == CursorKind.TEMPLATE_REF:
        refd = parent.get_definition()
        print('*'*depth, str(refd.spelling), str(refd.kind), str(refd.type.spelling), str(refd.get_num_template_arguments()))
        recurse_children(refd, depth+1)
    elif parent.kind == CursorKind.TEMPLATE_TYPE_PARAMETER:
        for a in parent.get_children():
            print('~'*depth+1, str(a.spelling), str(a.kind), str(a.type.spelling))
#    elif parent.kind == CursorKind.TYPE_ALIAS_TEMPLATE_DECL:
#        refd = parent.get_definition();
#        print('~'*depth, str(refd.spelling), str(refd.kind), str(refd.type.spelling), str(refd.get_num_template_arguments()))
#        recurse_children(refd, depth+1)
    for child in parent.get_children():
        recurse_children(child, depth+1)
    

def extractLocation(location):
    return {
        'filename' : location.file.name,
        'line': location.line,
        'column': location.column,
        'int_data': location.int_data,
        'offset': location.offset
#        'from_offset': location.from_offset,
#        'from_position': location.from_position,
#        'ptr_data': location.ptr_data
    }

def extractSubscriberCollectionTemplateArgs(subscriberStr):
    subscriber_rx = re.compile( 'ES::SubscriberCollection<(.*)> \*' )
    m = re.match(subscriber_rx, subscriberStr)
    if m is not None:
        signature = m.group(1)
        args = []
        
        bracket_level = 0
        start = 0
        end = 0

        for i in range(len(signature)):
            if signature[i] == '<':
                bracket_level = bracket_level + 1
            elif signature[i] == '>':
                bracket_level = bracket_level - 1
            
            if signature[i] == ',' and bracket_level == 0:
                args.append(signature[start:end].strip())
                start = i + 1
                end = start
            else:
                end = end + 1
        
        if start < len(signature) - 1:
            args.append(signature[start:end].strip())

        return args
      
    return [subscriberStr]

def get_call_type(call, ctype):
    out_obj = {}

    if ctype == 'subscribe':
        first_arg = True
        for c in call.get_arguments():

            if first_arg:
                first_arg = False
                toks = []
                for tok in c.get_tokens():
                    toks.append(str(tok.spelling))
                out_obj['event_name'] = ''.join(toks)
            if c.kind == CursorKind.CALL_EXPR:
                for p in c.get_children():
                    for ch in p.get_children():
                        if ch.kind == CursorKind.LAMBDA_EXPR:
                            out_obj['call_lambda'] = {'lambda_type' : str(ch.type.spelling), 'captures' : [], 'params' : [], 'call_location': extractLocation(ch.location)}
                            for cch in ch.get_children():
                                if cch.kind == CursorKind.VARIABLE_REF: # captured variable
                                    out_obj['call_lambda']['captures'].append({'name': str(cch.spelling), 'type': str(cch.type.get_canonical().spelling)})
                                elif cch.kind == CursorKind.PARM_DECL: # lambda fn parameter
                                    out_obj['call_lambda']['params'].append({'name': str(cch.spelling), 'type': str(cch.type.get_canonical().spelling)})
                        else:
                            out_obj['call_unknown'] = extractLocation(ch.location)
            elif c.kind == CursorKind.UNARY_OPERATOR:
                for ch in c.get_children():
                    if ch.kind == CursorKind.DECL_REF_EXPR:
                        out_obj['event_type'] = str(ch.type.get_canonical().spelling)
                        d = ch.get_definition()
                        if d is None:
                            d = ch.referenced

                        if d is not None:
                            fndef_comment = d.raw_comment if d.raw_comment is not None else ''
                            if d.kind == CursorKind.CXX_METHOD:
                                par = d.semantic_parent
                                if par is not None:
                                    out_obj['call_classmethod'] = {
                                        'name' : str(par.type.get_canonical().spelling) + '::' + d.spelling,
                                        'call_location' : extractLocation(c.location),
                                        'method_location' : extractLocation(d.location),
                                        'comments' : fndef_comment
                                    }
                                else:
                                    out_obj['call_classmethod'] = {
                                        'name' : '::' + d.spelling,
                                        'call_location' : extractLocation(c.location),
                                        'method_location' : extractLocation(d.location),
                                        'comments' : fndef_comment
                                    }
                            elif d.kind == CursorKind.FUNCTION_DECL:
                                out_obj['call_function'] = {
                                    'name' : str(d.spelling),
                                    'call_location' : extractLocation(c.location),
                                    'function_location' : extractLocation(d.location),
                                    'comments' : fndef_comment
                                }
                            else:
                                out_obj['call_unknown'] = extractLocation(ch.location)

    elif ctype == "publish":
        out_obj['published_params'] = []
        out_obj['call_location'] = extractLocation(call.location)

        first_arg = True
        for c in call.get_arguments():
            if first_arg:
                first_arg = False
                toks = []
                for tok in c.get_tokens():
                    toks.append(str(tok.spelling))
                out_obj['event_name'] = ''.join(toks)
            else:
                toks = []
                for tok in c.get_tokens():
                    toks.append(tok.spelling)
                out_obj['published_params'].append({'literal' : ''.join(toks), 'type': str(c.type.get_canonical().spelling)})

    elif ctype == "call":
        out_obj['published_params'] = []
        out_obj['call_location'] = extractLocation(call.location)

        for c in call.get_arguments():
            toks = []
            for tok in c.get_tokens():
                toks.append(tok.spelling)

        recurse_children(call, 1)
        
    elif ctype == "registerEvent":
        out_obj['registered_params'] = []
        out_obj['call_location'] = extractLocation(call.location)

        for c in call.get_arguments():
            toks = []
            for tok in c.get_tokens():
                toks.append(str(tok.spelling))
            out_obj['eventName'] = ''.join(toks) # should only be ONE parameter which is the event's name

        eventArgs = extractSubscriberCollectionTemplateArgs(call.type.get_canonical().spelling)
        for e in eventArgs:
            out_obj['registered_params'].append(e)
        
    return out_obj

def extractComments(call):
    sourceFile = open(call.location.file.name)
    lines = sourceFile.readlines()
    sourceFile.close()
    
    slcmt_rx = re.compile('^//.*')       # // single line comment
    mlcmt_rx = re.compile('^/\*.*\*/')   # /* multiline comment in one line */
    mlcmt_st = re.compile('^/\*')        # /* start of multiline comment
    mlcmt_ed = re.compile('\*/$')        #    end of multiline comment */
    
    comment_lines = []
    curLine = call.location.line - 2 # line-1 is correct line of call
    if mlcmt_rx.match(lines[curLine].strip()): # found a /* complete multiline comment */
        comment_lines.append(lines[curLine].strip())
    elif mlcmt_ed.match(lines[curLine].strip()): # found end of multiline comment
        comment_lines.append(lines[curLine].strip())
        curLine -= 1
        while not mlcmt_st.match(lines[curLine].strip()): # seek backwards until we hit /*
            comment_lines.append(lines[curLine].strip())
            curLine -= 1
        comment_lines.append(lines[curLine].strip()) # add last line (with comment start)

    elif slcmt_rx.match(lines[curLine].strip()): # found single line comment
        while slcmt_rx.match(lines[curLine].strip()): # seek backwards until we hit a line without //
            comment_lines.append(lines[curLine].strip())
            curLine -= 1
    
    return '\n'.join(comment_lines[::-1])

def get_call_details(call_list, fn_name):
    call_objs = []
    for entry in call_list:
        call_type_obj = get_call_type(entry['call'], fn_name)
        if call_type_obj:
            call_type_obj['ref_location'] = extractLocation(entry['call'].location)
            call_type_obj['comments'] = extractComments(entry['call'])
            call_objs.append(call_type_obj)

    return call_objs

def queue_to_list(queue):
    out = []
    while not queue.empty():
        out.append(queue.get())
    return out

def parse_file(arglist):
    local_subscribe_calls = []
    local_publish_calls = []
    local_register_calls = []
    local_call_calls = []

    tu = index.parse(None, arglist)
    for c in tu.cursor.walk_preorder():
        visit_node(c, local_subscribe_calls, local_publish_calls, local_register_calls, local_call_calls)

    sub_details = get_call_details(local_subscribe_calls, 'subscribe')
    pub_details = get_call_details(local_publish_calls, 'publish')
    reg_details = get_call_details(local_register_calls, 'registerEvent')
    #cal_details = get_call_details(local_call_calls, 'call')

    for c in sub_details:
        subscribe_calls.put(c)
    for c in pub_details:
        publish_calls.put(c)
    for c in reg_details:
        register_calls.put(c)

    subscribe_calls.close()
    publish_calls.close()
    register_calls.close()
    completed_processes.put(tu.spelling)

    completed_processes.close()

def printUsage(p):
    print("Usage:\n")
    print("\t", p, " compile_command_dir [output_file project_root [subset;of;files;to;parse]]")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("You MUST specify a directory containing compile_commands.json!")
        printUsage(argv[0])
        quit()
    
    clang_lib_path = subprocess.run(["llvm-config", "--libdir"], capture_output=True, text=True, encoding='utf-8').stdout.strip()
    print("Using llvm libdir:\t\t\t", Fore.CYAN, clang_lib_path, Fore.RESET)
    Config.set_library_path(clang_lib_path)

    # ensure LD_LIBRARY_PATH includes the llvm libdir
    ld_libpath = os.environ.get('LD_LIBRARY_PATH', '') # TODO: verify this is correct on Windows?
    if clang_lib_path not in ld_libpath:
        os.environ['LD_LIBRARY_PATH'] = ld_libpath + ":" + clang_lib_path

    if (len(sys.argv) >= 3):
        print("Writing results to file:\t\t", Fore.CYAN, sys.argv[2], Fore.RESET)
    
    if (len(sys.argv) >= 4):
        print("Restricting parsed files to:\t\t", Fore.CYAN, sys.argv[3], Fore.RESET)
        project_root = sys.argv[3]
        for i in range(len(exclude_files)):
            exclude_files[i] = project_root + exclude_files[i] ### TODO: cross-platform paths? os.path.join(project_root, path)

    print("Loading compilation database from:\t", Fore.CYAN, sys.argv[1], Fore.RESET)
    compdb = CompilationDatabase.fromDirectory(sys.argv[1])
    commands = compdb.getAllCompileCommands()
    index = Index.create()

    if len(sys.argv) >= 5:
        sources_to_scan = sys.argv[4].split(';')

    print("Processing files in batches of:\t\t", Fore.CYAN, max_processes, Fore.RESET)
    parse_processes = []
    for cc in commands:
        if len(sources_to_scan) > 0:
            if cc.filename in sources_to_scan and not cc.filename in exclude_files:
                if Path(cc.filename).exists(): # don't process non-existent files
                    arglist = [ar for ar in cc.arguments]
#                    print("Parsing '", cc.filename, "'")
                    p = Process(target=parse_file, args=(arglist,))
                    parse_processes.append(p)
                else:
                    print(Fore.YELLOW + 'WARNING: included file does not exist and will not be parsed:' + Fore.RESET, cc.filename)
        else:
            if not cc.filename in exclude_files:
                if Path(cc.filename).exists(): # don't process non-existent files
                    arglist = [ar for ar in cc.arguments]
#                    print("Parsing '", cc.filename, "'")
                    p = Process(target=parse_file, args=(arglist,))
                    parse_processes.append(p)
                else:
                    print(Fore.YELLOW + 'WARNING: included file does not exist and will not be parsed:' + Fore.RESET, cc.filename)


    last_process_idx = 0
    curr_processes = []
    with tqdm(total=len(parse_processes), desc="Parsing", unit='file',
              bar_format="%s{l_bar}%s{bar}%s| {n_fmt}/{total_fmt}  [{elapsed}, {rate_fmt} {postfix}]"
              % (Style.BRIGHT+Fore.LIGHTGREEN_EX, Style.NORMAL+Fore.LIGHTBLUE_EX, Fore.RESET)) as pbar:
        while len(parse_processes) != completed_processes.qsize():
            while len(curr_processes) < max_processes and last_process_idx < len(parse_processes):
                curr_processes.append(parse_processes[last_process_idx])
                parse_processes[last_process_idx].start()

                last_process_idx += 1

            if completed_processes.qsize() >= last_process_idx:
                curr_processes = []

            time.sleep(0.25)
            pbar.update(completed_processes.qsize() - pbar.n)
            pbar.refresh()

    # drain completed queue to ensure no subprocesses are blocked on it
    while not completed_processes.empty():
        completed_processes.get()

    print(Fore.RESET + "Processing found callsites...")
    # collect all calls
    master_list = {'subscribers': [], 'publishers': [], 'registrars': [], 'directcalls': []}
    while subscribe_calls.qsize() + publish_calls.qsize() + register_calls.qsize() + call_calls.qsize() > 0:
        master_list['subscribers'] += queue_to_list(subscribe_calls)
        master_list['publishers']  += queue_to_list(publish_calls)
        master_list['registrars']  += queue_to_list(register_calls)
        master_list['directcalls'] += queue_to_list(call_calls)
        time.sleep(0.1) # give child processes a chance to add any last-minute entries they may have buffered

    # join all suprocesses
    for p in parse_processes:
        p.join()

    # write out results
    if len(sys.argv) >= 3:
        with open(sys.argv[2], 'w') as outFile:
            outFile.write(json.dumps(master_list, indent=2))
    else:
        print(json.dumps(master_list, indent=2))

    print(Fore.LIGHTGREEN_EX + "Done.")
    deinit()
