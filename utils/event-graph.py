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
import sys
import json


def fnSinkNode(parent, source):
    node = {'data': { 'parent': parent['data']['parent'], 'comments': ''}, 'classes': 'callbackfn'}

    if 'call_classmethod' in source:
        node['data']['kind'] = 'ClassMethod'
        node['data']['name'] = source['call_classmethod']['name']
        node['data']['file'] = source['call_classmethod']['method_location']['filename']
        node['data']['line'] = source['call_classmethod']['method_location']['line']
        node['data']['comments'] = source['call_classmethod']['comments']
    elif 'call_function' in source:
        node['data']['kind'] = 'FreeFunction'
        node['data']['name'] = source['call_function']['name']
        node['data']['file'] = source['call_function']['function_location']['filename']
        node['data']['line'] = source['call_function']['function_location']['line']
        node['data']['comments'] = source['call_function']['comments']
    elif 'call_lambda' in source:
        node['data']['kind'] = 'lambda'
        node['data']['name'] = source['call_lambda']['lambda_type']
        node['data']['file'] = source['call_lambda']['call_location']['filename']
        node['data']['line'] = source['call_lambda']['call_location']['line']
        node['data']['lambda_params'] = {'arguments': source['call_lambda']['params'], 'captures': source['call_lambda']['captures']}
    else:
        print('WTF', parent, source)

    node['classes'] += ' ' + node['data']['kind']
    node['data']['id'] = 'sink_' + node['data']['kind'] + '_' + node['data']['name'] + '_' + node['data']['file'] + ':' + str(node['data']['line'])
    return node

if __name__ == "__main__":
    inFile = open(sys.argv[1])
    data_in = json.load(inFile)

    eventNames = []
    event_objects = []
    subscribers = []
    publishers = []
    registers = []

    sub_sig_rx = re.compile('void \((.*)\)')
    nsubs = 0
    for sub in data_in['subscribers']:
        nsubs = nsubs + 1
        ob_data = {'data': {}}
        ob_data['data']['kind'] = 'subscriber'
        ob_data['classes'] = 'subscriber'

        if 'event_name' not in sub:
            print(nsubs, "No Event Name! The following entry will be discarded:\n", sub)
            continue
        ob_data['data']['eventName'] = sub['event_name']
        ob_data['data']['comments'] = sub['comments']

        if 'event_type' in sub:
            m = re.match(sub_sig_rx, sub['event_type'])
            if m is not None:
                ob_data['data']['signature'] = m.group(1) #sub['event_type']
            else:
                print('could not match:', sub['event_type'])
        
        if 'call_classmethod' in sub:
            ob_data['data']['sourceFile'] = sub['call_classmethod']['call_location']['filename']
            ob_data['data']['sourceLine'] = sub['call_classmethod']['call_location']['line']
            ob_data['data']['sourceCol'] = sub['call_classmethod']['call_location']['column']
            ob_data['data']['sourceFn'] = sub['call_classmethod']['name']
        elif 'call_function' in sub:
            ob_data['data']['sourceFile'] = sub['call_function']['call_location']['filename']
            ob_data['data']['sourceLine'] = sub['call_function']['call_location']['line']
            ob_data['data']['sourceCol'] = sub['call_function']['call_location']['column']
            ob_data['data']['sourceFn'] = sub['call_function']['name']
        elif 'call_lambda' in sub:
            ob_data['data']['sourceFile'] = sub['call_lambda']['call_location']['filename']
            ob_data['data']['sourceLine'] = sub['call_lambda']['call_location']['line']
            ob_data['data']['sourceCol'] = sub['call_lambda']['call_location']['column']
            ob_data['data']['sourceFn'] = sub['call_lambda']['lambda_type']

            ob_data['data']['signature'] = ''
            toks = []
            for p in sub['call_lambda']['params']:
                toks.append(p['type'])
            ob_data['data']['signature'] = ', '.join(toks)
        else:
            ob_data['errors'] = {'msg' : 'Could not find function or method call for this subscriber?', 'data': sub}
            print('Could not find callback type:\n\t', nsubs, sub)
            continue
        
        if 'sourceFile' in ob_data['data']:
            ob_data['data']['id'] = ob_data['data']['sourceFile'] + ': ' + str(ob_data['data']['sourceLine']) + '::' + str(ob_data['data']['sourceCol'])
        else:
            ob_data['data']['id'] = sub['ref_location']['filename'] + ': ' + str(sub['ref_location']['line']) + '::' + str(sub['ref_location']['column'])
        ob_data['data']['parent'] = ob_data['data']['eventName']
            
        if 'signature' not in ob_data['data'] or ob_data['data']['signature'] == "":
            ob_data['data']['signature'] = "(void)"

        if ob_data['data']['eventName'] not in eventNames:
            eventNames.append(ob_data['data']['eventName'])
            event_objects.append({'data': {'id': ob_data['data']['eventName']}, 'classes': 'group'})
        subscribers.append({'id': ob_data['data']['id'], 'eventName': ob_data['data']['eventName'], 'signature': ob_data['data']['signature']})
        event_objects.append(ob_data)
        sink = fnSinkNode(ob_data, sub)
        event_objects.append(sink)
        event_objects.append({'data': {'id': ob_data['data']['id'] + '__' + sink['data']['id'], 'source': ob_data['data']['id'], 'target': sink['data']['id'], 'eventName': ob_data['data']['eventName']}, 'classes': 'callbackfncall'})
    
    for pub in data_in['publishers']:
        ob_data = {'data': {}}
        ob_data['data']['kind'] = 'publisher'
        ob_data['classes'] = 'publisher'
        ob_data['data']['eventName'] = pub['event_name']
        ob_data['data']['comments'] = pub['comments']

        evt_params = []
        for p in pub['published_params']:
            evt_params.append(p['type'])
        ob_data['data']['signature'] = ', '.join(evt_params)

        ob_data['data']['sourceFile'] = pub['call_location']['filename']
        ob_data['data']['sourceLine'] = pub['call_location']['line']
        ob_data['data']['sourceCol'] = pub['call_location']['column']
    
        ob_data['data']['id'] = ob_data['data']['sourceFile'] + ': ' + str(ob_data['data']['sourceLine']) + '::' + str(ob_data['data']['sourceCol'])
        ob_data['data']['parent'] = ob_data['data']['eventName']
            
        if ob_data['data']['signature'] is None or ob_data['data']['signature'] == "":
            ob_data['data']['signature'] = "(void)"
        
        if ob_data['data']['eventName'] not in eventNames:
            eventNames.append(ob_data['data']['eventName'])
            event_objects.append({'data': {'id': ob_data['data']['eventName']}})

        publishers.append({'id': ob_data['data']['id'], 'eventName': ob_data['data']['eventName'], 'signature': ob_data['data']['signature']})
        event_objects.append(ob_data)
    
    for reg in data_in['registrars']:
        ob_data = {'data': {}}
        ob_data['data']['kind'] = 'registrar'
        ob_data['classes'] = 'registrar'
        ob_data['data']['eventName'] = reg['eventName']
        ob_data['data']['comments'] = reg['comments']

        ob_data['data']['signature'] = ','.join(reg['registered_params'])

        ob_data['data']['sourceFile'] = reg['call_location']['filename']
        ob_data['data']['sourceLine'] = reg['call_location']['line']
        ob_data['data']['sourceCol'] = reg['call_location']['column']
    
        ob_data['data']['id'] = ob_data['data']['sourceFile'] + ': ' + str(ob_data['data']['sourceLine']) + '::' + str(ob_data['data']['sourceCol'])
        ob_data['data']['parent'] = ob_data['data']['eventName']
            
        if ob_data['data']['signature'] is None or ob_data['data']['signature'] == "":
            ob_data['data']['signature'] = "(void)"
        
        if ob_data['data']['eventName'] not in eventNames:
            eventNames.append(ob_data['data']['eventName'])
            event_objects.append({'data': {'id': ob_data['data']['eventName']}})
                
        registers.append({'id': ob_data['data']['id'], 'eventName': ob_data['data']['eventName'], 'signature': ob_data['data']['signature']})
        event_objects.append(ob_data)

    # search for connections between publishers and subscribers
    for pub in publishers:
        for sub in subscribers:
            if pub['eventName'] == sub['eventName']:
                edge = {'data': {'id': pub['id'] + '__' + sub['id'], 'source': pub['id'], 'target': sub['id'], 'eventName': pub['eventName']}, 'classes': ''}
                if pub['signature'] != sub['signature']:
                    edge['classes'] += 'signaturemismatch '
                event_objects.append(edge)
    
    # search for connections between registrations and others
    for reg in registers:
        for sub in subscribers:
            if reg['eventName'] == sub['eventName']:
                edge = {'data': {'id': reg['id'] + '__' + sub['id'], 'source': reg['id'], 'target': sub['id'], 'eventName': reg['eventName']}, 'classes': 'registration '}
                if reg['signature'] != sub['signature']:
                    edge['classes'] += 'signaturemismatch '
                event_objects.append(edge)
        for pub in publishers:
            if pub['eventName'] == reg['eventName']:
                edge = {'data': {'id': reg['id'] + '__' + pub['id'], 'source': reg['id'], 'target': pub['id'], 'eventName': reg['eventName']}, 'classes': 'registration '}
                if reg['signature'] != pub['signature']:
                    edge['classes'] += 'signaturemismatch '
                event_objects.append(edge)
    
    if len(sys.argv) > 2:
        with open(sys.argv[2], 'w') as outFile:
            outFile.write(json.dumps(event_objects, indent=2))
    else:
        print(json.dumps(event_objects, indent=2))
