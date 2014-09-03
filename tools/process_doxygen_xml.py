#! /usr/bin/python
from xml.dom import minidom
from sys import argv

helptext = \
"""This script processes the XML generated by "make doc" and produces summary information
on symbols that Mir intends to make public.

To use:

1. Go to your build folder and run "make doc"
2. Create summary by running "../tools/process_doxygen_xml.py doc/xml/*.xml > raw-output"
3. For each symbol map (e.g. mirserver)
3.1. cat raw-output | grep "^mirserver public" | sed "s/mirserver public: /    /" | sort > mirserver-public-symbols
3.2. vi -d mirserver-public-symbols ../src/server/symbols.map"""

debug = False

def get_text(node):
    rc = []
    for node in node.childNodes:
        if node.nodeType == node.TEXT_NODE:
            rc.append(node.data)
        elif node.nodeType == node.ELEMENT_NODE:
            rc.append(get_text(node))
    return ''.join(rc)

def get_text_for_element(parent, tagname):
    rc = []
    nodes = parent.getElementsByTagName(tagname);
    for node in nodes : rc.append(get_text(node))
    return ''.join(rc)

def get_file_location(node):
    for node in node.childNodes:
        if node.nodeType == node.ELEMENT_NODE and node.tagName == 'location':
            return node.attributes['file'].value
    if debug: print 'no location in:', node
    return None
    
def has_element(node, tagname):
    for node in node.childNodes:
        if node.nodeType == node.ELEMENT_NODE and node.tagName in tagname:
            return True
    return False

def print_attribs(node, attribs):
    for attrib in attribs : print ' ', attrib, '=', node.attributes[attrib].value

def concat_text_from_tags(parent, tagnames):
    rc = []
    for tag in tagnames : rc.append(get_text_for_element(parent, tag))
    return ''.join(rc)
    
def print_location(node):
    print ' ', 'location', '=', get_file_location(node)

def get_attribs(node):
    kind = node.attributes['kind'].value
    static = node.attributes['static'].value
    prot =  node.attributes['prot'].value
    return (kind, static, prot)

# Special cases for publishing anyway:
publish_special_cases = {
    # Although private this is called by a template wrapper function that instantiates
    # in client code
    'mir::SharedLibrary::load_symbol*',
}

component_map = {}

def report(component, publish, symbol):
    symbol = symbol.replace('~', '?')

    if symbol in publish_special_cases: publish = True

    symbols = component_map.get(component, {'public' : set(), 'private' : set()})
    if publish: symbols['public'].add(symbol)
    else:       symbols['private'].add(symbol)
    component_map[component] = symbols
    if not debug: return
    if publish: print '  PUBLISH in {}: {}'.format(component, symbol)
    else      : print 'NOPUBLISH in {}: {}'.format(component, symbol)

def print_report():
    format = '{} {}: {};'
    for component, symbols in component_map.iteritems():
        print 'COMPONENT:', component
        for key in symbols.keys():
            for symbol in symbols[key]: print format.format(component, key, symbol)
        print

def print_debug_info(node, attributes):
    if not debug: return
    print
    print_attribs(node, attributes)
    print_location(node)

def find_physical_component(location_file):
    path_elements = location_file.split('/')
    found = False
    for element in path_elements:
        if found: return element
        found = element in ['include', 'src']
    if debug: print 'no component in:', location_file
    return None
    
def mapped_physical_component(location_file):
    location = find_physical_component(location_file)
    if location == 'shared': location = 'common'
    return 'mir' + location

def parse_member_def(context_name, node, is_class):
    library = mapped_physical_component(get_file_location(node))
    (kind, static, prot) = get_attribs(node)
    
    if kind in ['enum', 'typedef']: return
    if has_element(node, ['templateparamlist']): return
    if kind in ['function'] and node.attributes['inline'].value == 'yes': return
    
    name = concat_text_from_tags(node, ['name'])
    if name in ['__attribute__']:
        if debug: print '  ignoring doxygen mis-parsing:', concat_text_from_tags(node, ['argsstring'])
        return

    if name.startswith('operator'): name = 'operator'
    if not context_name == None: symbol = context_name + '::' + name
    else: symbol = name

    file_location = get_file_location(node)
    publish = '/include/' in file_location \
              or 'build/src' in file_location \
              or 'src/shared/input/android/' in file_location

    is_function = kind == 'function'
    if publish: publish = kind != 'define'
    if publish and is_class: publish = is_function or static == 'yes'
    if publish and prot == 'private':
        if is_function: publish = node.attributes['virt'].value == 'virtual'
        else: publish =  False
    
    if is_function: print_debug_info(node, ['kind', 'prot', 'static', 'virt'])
    else: print_debug_info(node, ['kind', 'prot', 'static'])
    if debug: print '  is_class:', is_class
    report(library, publish, symbol+'*')
    if is_function and node.attributes['virt'].value == 'virtual': report(library, publish, 'non-virtual?thunk?to?'+symbol+'*')

def parse_compound_defs(xmldoc):
    compounddefs = xmldoc.getElementsByTagName('compounddef') 
    for node in compounddefs:
        kind = node.attributes['kind'].value

        if kind in ['page', 'file', 'example', 'union']: continue

        if kind in ['group']: 
            for member in node.getElementsByTagName('memberdef') : 
                parse_member_def(None, member, False)
            continue

        if kind in ['namespace']: 
            symbol = concat_text_from_tags(node, ['compoundname'])
            for member in node.getElementsByTagName('memberdef') : 
                parse_member_def(symbol, member, False)
            continue
        
        file = get_file_location(node)
        if debug: print '  from file:', file 
        if '/examples/' in file or '/test/' in file or '[generated]' in file or '[STL]' in file:
            continue

        if has_element(node, ['templateparamlist']): continue

        library = mapped_physical_component(file)
        symbol = concat_text_from_tags(node, ['compoundname'])
        
        file_location = get_file_location(node)
        publish = '/include/' in file_location \
                  or 'build/src' in file_location \
                  or 'src/shared/input/android/' in file_location

        if publish: 
            if kind in ['class', 'struct']:
                prot =  node.attributes['prot'].value
                publish = prot != 'private'
                print_debug_info(node, ['kind', 'prot'])
                report(library, publish, 'vtable?for?' + symbol)
                report(library, publish, 'typeinfo?for?' + symbol)

        if publish: 
            for member in node.getElementsByTagName('memberdef') : 
                parse_member_def(symbol, member, kind in ['class', 'struct'])

if __name__ == "__main__":
    if len(argv) == 1 or '-h' in argv or '--help' in argv:
        print helptext
        exit()

    for arg in argv[1:]:
        try:
            if debug: print 'Processing:', arg
            xmldoc = minidom.parse(arg)
            parse_compound_defs(xmldoc)
        except Exception as error:
            print 'Error:', arg, error

    print_report()
