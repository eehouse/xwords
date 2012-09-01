#!/usr/bin/python

import re, sys, os, getopt

# g_nodes = []
# g_comments = {}
# g_strings = {}
# g_empties = {}

p_string = '^ *<string name="(.*?)">.*?</string>'
p_comment ='^ *<!--.*?-->'
p_empty = '^$'

pat = re.compile( p_string
                  + '|'
                  + p_comment
                  + '|'
                  + p_empty
                  , re.DOTALL | re.MULTILINE )
pat_string = re.compile( p_string, re.DOTALL | re.MULTILINE )
pat_comment = re.compile( p_comment, re.DOTALL | re.MULTILINE )
pat_empty = re.compile( p_empty, re.DOTALL | re.MULTILINE )

def usage(msg=''):
    print
    if not '' == msg: print 'Error:', msg
    print "usage:", sys.argv[0], "[-l lang]* [-g]"
    print '    [-l lang]* # include in list of langs compared'
    print '    [-g] # generate dummy nodes where missing'
    sys.exit(0)

def load_strings(lang=''):
    nodes = []
    comments = {}
    strings = {}
    empties = {}
    wd = os.path.dirname(sys.argv[0])
    values = 'values'
    if not '' == lang: values += '-' + lang
    path = wd + '/../XWords4/res/' + values + '/strings.xml'
    file = open( path, 'r')
    for match in pat.finditer( file.read() ):
        index = len(nodes)
        elem = match.group(0)
        if pat_string.match(elem):
            strings[match.group(1)] = index
        elif pat_comment.match(elem):
            comments[index] = index
        elif pat_empty.match(elem):
            empties[index] = index
        nodes.append( elem )
    return nodes, strings, empties, comments

def main():
    langs = []
    generate = False
    pairs, rest = getopt.getopt(sys.argv[1:], "l:g")
    for option, value in pairs:
        if option == '-l': langs.append(value)
        elif option == '-g': generate = True
        else: usage()

    if generate and 1 != len(langs):
        usage('-g requires exactly one -l lang')

    en_nodes, en_strings, en_empties, en_comments = load_strings();

    for lang in langs:
        l_nodes, l_strings, l_empties, l_comments = load_strings(lang)

        # print those keys in lang but not in English
        for str in l_strings.keys():
            if not str in en_strings:
                print str, 'not in the English xml file'

        # then those in English not in lang
        # for str in en_strings.keys():
        #     if not str in l_strings:
        #         print str, 'in the English but not in', lang, 'xml file'

        # generate a new lang file with a node for every one in
        # English.  Use the localized one if present, otherwise
        # substitute in the English, adding a comment
        if generate:
            for indx in range(len(en_nodes)):
                if indx in en_strings.values(): # it's a string
                    id = [siid for siid in en_strings.keys() if en_strings[siid] == indx][0]
                    if id in l_strings.keys(): 
                        print l_nodes[l_strings[id]]
                    else: 
                        print '    <!--XLATE-ME-->'
                        print en_nodes[indx]

    # for id in en_strings.keys():
    #     index = en_strings[id]
    #     if not index - 1 in en_comments:
    #         print en_nodes[index], 'is missing a comment'

    # for node in en_nodes:
    #     print node

##############################################################################
if __name__ == '__main__':
    main()

