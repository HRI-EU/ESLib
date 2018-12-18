# -*- coding: utf-8 -*-
#
#  Custom package settings
#
#  Copyright (C)
#  Honda Research Institute Europe GmbH
#  Carl-Legien-Str. 30
#  63073 Offenbach/Main
#  Germany
#
#  UNPUBLISHED PROPRIETARY MATERIAL.
#  ALL RIGHTS RESERVED.
#
#
name = "ESLib"

category="Libraries"

sqOptOutRules    = [ 'GEN03', 'GEN04', 'GEN07', 'GEN10', 'C02', 'C06', 'DOC01', 'DOC04' ]

sqComments       = {"GEN03":"Not feasible for C++ template library since readability suffers massively","GEN04":"Software will be open-sourced and will receive a different header","GEN10":"Software will be made accessible for University partners and will be hosted in a public Git repository","C02":"Software entirely written in C++","C06":"Software is template library, inline rule not applicable","DOC01":"Doxygen mainpage exists under doc folder","DOC04":"Should not be public outside HRI-EU"}



# EOF
