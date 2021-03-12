# -*- coding: utf-8 -*-
#
#  Copyright (c) 2017, Honda Research Institute Europe GmbH.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#
#  1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#
#  2. Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
#
#  3. Neither the name of the copyright holder nor the names of its
#     contributors may be used to endorse or promote products derived from
#     this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY EXPRESS OR
#  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
#  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
#  IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY DIRECT, INDIRECT,
#  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
#  OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
#  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
#  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
#  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
import re
import sys, os
import json
import subprocess
from colorama import init, deinit, Fore, Back, Style

def get_signature(fn, kind):
    sig = None
    if kind == 'subscriber':
        if 'event_type' in fn:
            m = re.match('void \((.*)\)', fn['event_type'])
            if m is not None:
                sig = m.group(1)
            else:
                print('ERROR: Could not parse event type:', fn['event_type'], 'from call: ', fn)
                sig = 'ERROR'
        elif 'call_lambda' in fn:
            toks = []
            for p in fn['call_lambda']['params']:
                toks.append(p['type'])
            sig = ', '.join(toks)
        else:
            print('ERROR: Subscriber signature could be found!\n\t', fn)
            sig = 'ERROR'
    elif kind == 'publisher':
        params = []
        for p in fn['published_params']:
            params.append(p['type'])
        sig = ', '.join(params)
    elif kind == 'registrar':
        sig = ', '.join(fn['registered_params'])
    else:
        print('ERROR: Unknown call type:', kind, '\n\t', fn)
    
    if sig is None or sig == "":
        sig = '(void)'
    
    return sig

def get_call_line(fn, kind):
    if kind == 'subscriber':
        if 'call_classmethod' in fn:
            return fn['call_classmethod']['call_location']['filename'] + ':' + Style.BRIGHT + str(fn['call_classmethod']['call_location']['line']) + Style.NORMAL
        elif 'call_function' in fn:
            return fn['call_function']['call_location']['filename'] + ':' + Style.BRIGHT + str(fn['call_function']['call_location']['line']) + Style.NORMAL
        elif 'call_lambda' in fn:
            return fn['call_lambda']['call_location']['filename'] + ':' + Style.BRIGHT + str(fn['call_lambda']['call_location']['line']) + Style.NORMAL
        else:
            return fn['ref_location']['filename'] + ':' + Style.BRIGHT + str(fn['ref_location']['line']) + Style.NORMAL
    elif 'call_location' in fn:
        return fn['call_location']['filename'] + ':' + Style.BRIGHT + str(fn['call_location']['line']) + Style.NORMAL
    else:
        return fn['ref_location']['filename'] + ':' + Style.BRIGHT + str(fn['ref_location']['line']) + Style.NORMAL

def get_line_from_file(filename, line, blameFrom=None):
    if blameFrom is None:
        with open(filename, 'r') as f:
            lines = f.readlines()
            return lines[line-1]    # libclang produces line numbers 1-off from our index here
    else:
        cmd = 'cd ' + blameFrom + ';git blame --date=relative -L ' + str(line) + ',' + str(line) + ' ' + filename
        res = re.sub(r"\)(\s+\S)", lambda x: ')' + Fore.LIGHTYELLOW_EX + x.group(1), re.sub(r"\((\S+)", lambda x: '(' + Style.BRIGHT + x.group(1) + Style.NORMAL, re.sub(r"ago(\s*[0-9]*)\)", 'ago)', os.popen(cmd).read()))) + Fore.RESET
        return str(res)

def get_mismatch_msg(eventName, publishers=None, subscribers=None, registrars=None, blameFrom=None):
    out = '\n' + Style.BRIGHT + Fore.RED + 'Event signature mismatch for: ' + Fore.LIGHTYELLOW_EX + eventName + Fore.RESET + Style.NORMAL

    mismatch_strs = []
    signatures = []
    signature_counts = {}
    mismatches = {}
    if registrars is not None:
        for registrar in registrars:
            sig = get_signature(registrar, 'registrar')
            if sig in mismatches:
                mismatches[sig]['count'] += 1
                mismatches[sig]['registrars'].append(registrar)
            else:
                mismatches[sig] = {'count': 1, 'registrars': [registrar], 'subscribers': [], 'publishers': []}

    if publishers is not None:
        for publisher in publishers:
            sig = get_signature(publisher, 'publisher')
            if sig in mismatches:
                mismatches[sig]['count'] += 1
                mismatches[sig]['publishers'].append(publisher)
            else:
                mismatches[sig] = {'count': 1, 'publishers': [publisher], 'subscribers': [], 'registrars': []}

    if subscribers is not None:
        for subscriber in subscribers:
            sig = get_signature(subscriber, 'subscriber')
            if sig in mismatches:
                mismatches[sig]['count'] += 1
                mismatches[sig]['subscribers'].append(subscriber)
            else:
                mismatches[sig] = {'count': 1, 'subscribers': [subscriber], 'publishers': [], 'registrars': []}

    mismatches_sorted = {k: v for k, v in sorted(mismatches.items(), key=lambda x: x[1]['count'])}
    for sig, eles in mismatches_sorted.items():
        out += '\n\t' + '<' + Fore.MAGENTA + sig + Fore.RESET + '>  ' + str(eles['count']) + ' usage' + ('s' if eles['count'] > 1 else '') + ':'
        for registrar in eles['registrars']:
            out += '\n\t\t' + Style.BRIGHT + 'registerEvent()' + Style.NORMAL + ' called at '
            out += Fore.CYAN + get_call_line(registrar, 'registrar') + Fore.RESET
            # out += ' with parameter types: <' + Fore.MAGENTA + get_signature(registrar, 'registrar') + Fore.RESET + '>'
            if len(registrar['comments']) > 0:
                out += '\n\t\t\t' + Fore.LIGHTGREEN_EX + Style.DIM + '\n\t\t\t'.join(registrar['comments'].split('\n')) + Style.RESET_ALL
            out += '\n\t\t\t' + get_line_from_file(registrar['ref_location']['filename'], registrar['ref_location']['line'], blameFrom).strip()
        for publisher in eles['publishers']:
            out += '\n\t\t' + Style.BRIGHT + 'publish()' + Style.NORMAL + ' called at '
            out += Fore.CYAN + get_call_line(publisher, 'publisher') + Fore.RESET
            # out += ' with parameter types: <' + Fore.MAGENTA + get_signature(publisher, 'publisher') + Fore.RESET + '>'
            if len(publisher['comments']) > 0:
                out += '\n\t\t\t' + Fore.LIGHTGREEN_EX + Style.DIM + '\n\t\t\t'.join(publisher['comments'].split('\n')) + Style.RESET_ALL
            out += '\n\t\t\t' + get_line_from_file(publisher['ref_location']['filename'], publisher['ref_location']['line'], blameFrom).strip()
        for subscriber in eles['subscribers']:
            out += '\n\t\t' + Style.BRIGHT + 'subscribe()' + Style.NORMAL + ' called at '
            out += Fore.CYAN + get_call_line(subscriber, 'subscriber') + Fore.RESET
            # out += ' with parameter types: <' + Fore.MAGENTA + get_signature(subscriber, 'subscriber') + Fore.RESET + '>'
            if len(subscriber['comments']) > 0:
                out += '\n\t\t\t' + Fore.LIGHTGREEN_EX + Style.DIM + '\n\t\t\t'.join(subscriber['comments'].split('\n')) + Style.RESET_ALL
            out += '\n\t\t\t' + get_line_from_file(subscriber['ref_location']['filename'], subscriber['ref_location']['line'], blameFrom).strip()

    out += '\n'
    return out

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("ERROR: You must specify an input file!")
        exit(1)
    
    init()
    print("Processing event data in:", Fore.CYAN, sys.argv[1])
    inFile = open(sys.argv[1])
    data_in = json.load(inFile)
    inFile.close()

    git_dir = None
    if len(sys.argv) >= 3:
        git_dir = sys.argv[2]

    signature_mismatch_found = False

    total_events = []
    checked_events = []

    for pub in data_in['publishers']:
        if pub['event_name'] in total_events:
            continue
        total_events.append(pub['event_name'])
    for sub in data_in['subscribers']:
        if sub['event_name'] in total_events:
            continue
        total_events.append(sub['event_name'])
    for reg in data_in['registrars']:
        if reg['eventName'] in total_events:
            continue
        total_events.append(reg['eventName'])

    for current_event in total_events:
        mismatches = {'subscribers': [], 'publishers': [], 'registrars': []}

        if current_event in checked_events:
            continue
        current_sig = None
        mismatch = False

        for sub in data_in['subscribers']:
            if sub['event_name'] == current_event:
                mismatches['subscribers'].append(sub)
                if current_sig:
                    if get_signature(sub, 'subscriber') != current_sig:
                        mismatch = True
                else:
                    current_sig = get_signature(sub, 'subscriber')
                
        for pub in data_in['publishers']:
            if pub['event_name'] == current_event:
                mismatches['publishers'].append(pub)
                if current_sig:
                    if get_signature(pub, 'publisher') != current_sig:
                        mismatch = True
                else:
                    current_sig = get_signature(pub, 'publisher')

        for reg in data_in['registrars']:
            if reg['eventName'] == current_event:
                mismatches['registrars'].append(reg)
                if current_sig:
                    if get_signature(reg, 'registrar') != current_sig:
                        mismatch = True
                else:
                    current_sig = get_signature(reg, 'registrar')
        
        if mismatch:
            print(get_mismatch_msg(eventName=current_event, publishers=mismatches['publishers'], subscribers=mismatches['subscribers'], registrars=mismatches['registrars'], blameFrom=git_dir))
        signature_mismatch_found = signature_mismatch_found or mismatch

        checked_events.append(current_event)
    
    deinit()
    if signature_mismatch_found:
        print(Style.BRIGHT + Fore.RED + 'Event validation FAILED\n' + Style.RESET_ALL)
        exit(1)
    else:
        print(Style.BRIGHT + Fore.GREEN + 'Event validation PASSED\n' + Style.RESET_ALL)
        exit(0)
