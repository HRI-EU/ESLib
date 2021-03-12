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
import json
import sys
import ntpath

# Used to mark the start and end of templatized sections
# The lines on which the opening and closing comments appear will be discarded.
TMPL_BLOCK_OPEN = '/** +-+'
TMPL_BLOCK_CLOSE = '-+- **/'

# Used to mark the start and end of individual template tags.
# Template tags cannot be nested.
TMPL_TAG_OPEN = '{:'
TMPL_TAG_CLOSE = ':}'

def find_template_block( text ):
    try:
        start_idx = text.index(TMPL_BLOCK_OPEN)
        end_idx = text.index(TMPL_BLOCK_CLOSE)

        block = text[start_idx:end_idx].split('\n')
        block = '\n'.join(block[1:-1])
        return (block, start_idx, end_idx+len(TMPL_BLOCK_CLOSE))
    except ValueError:
        return (None, -1, -1)

def find_template_tag( text ):
    try:
        start_idx = text.index(TMPL_TAG_OPEN)
        end_idx = text.index(TMPL_TAG_CLOSE)

        return (text[start_idx+len(TMPL_TAG_OPEN):end_idx].strip(), start_idx, end_idx+len(TMPL_TAG_CLOSE))
    except ValueError:
        return None

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("You MUST specify input and output files!")
        print("Usage:\n\t" + sys.argv[0] + "template_file graph_file output_file")

    with open(sys.argv[1], 'r') as input:
        in_template = input.read()

    with open(sys.argv[2], 'r') as input:
        in_graph = input.read()

    out_file = sys.argv[3]

    # find template block
    template, boundsmin, boundsmax = find_template_block(in_template)
    while template is not None:
        tag = find_template_tag(template)
        while tag is not None:
            param = tag[0]
            if tag[0] == 'graph':
                param = in_graph
            elif tag[0] == 'graph_name':
                param = ntpath.basename(sys.argv[2])
            else:
                print("Unknown Template Parameter: '" + tag[0] + "'")

            template = template[0:tag[1]] + param + template[tag[2]:]
            tag = find_template_tag(template)

        in_template = in_template[0:boundsmin] + template + in_template[boundsmax:]

        # get next template block if there is one
        template, boundsmin, boundsmax = find_template_block(in_template)

    with open(out_file, 'w') as output:
        output.write(in_template)
