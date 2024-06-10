# To make this file known to Qt Creator using:
# Tools > Options > Debugger > Locals & Expressions > Extra Debugging Helpers
# Any contents here will be picked up by GDB, LLDB, and CDB based
# debugging in Qt Creator automatically.


# Example to display a simple type
# template<typename U, typename V> struct MapNode
# {
#     U key;
#     V data;
# }
#
# def qdump__MapNode(d, value):
#    d.putValue("This is the value column contents")
#    d.putExpandable()
#    if d.isExpanded():
#        with Children(d):
#            # Compact simple case.
#            d.putSubItem("key", value["key"])
#            # Same effect, with more customization possibilities.
#            with SubItem(d, "data")
#                d.putItem("data", value["data"])

# Check http://doc.qt.io/qtcreator/creator-debugging-helpers.html
# for more details or look at qttypes.py, stdtypes.py, boosttypes.py
# for more complex examples.

# to try parsing:
# env PYTHONPATH=/usr/share/qtcreator/debugger/ python src/ryml-gdbtypes.py


import dumper
#from dumper import Dumper, Value, Children, SubItem
#from dumper import SubItem, Children
from dumper import *

import sys
import os


# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------

# QtCreator makes it really hard to figure out problems in this code.
# So here are some debugging utilities.


# FIXME. this decorator is not working; find out why.
def dbglog(func):
    """a decorator that logs entry and exit of functions"""
    if not _DBG:
        return func
    def func_wrapper(*args, **kwargs):
        _dbg_enter(func.__name__)
        ret = func(*args, **kwargs)
        _dbg_exit(func.__name__)
        return ret
    return func_wrapper


_DBG = False
_dbg_log = None
_dbg_stack = 0
def _dbg(*args, **kwargs):
    global _dbg_log, _dbg_stack
    if not _DBG:
        return
    if _dbg_log is None:
        filename = os.path.join(os.path.dirname(__file__), "dbg.txt")
        _dbg_log = open(filename, "w")
    kwargs['file'] = _dbg_log
    kwargs['flush'] = True
    print("  " * _dbg_stack, *args, **kwargs)


def _dbg_enter(name):
    global _dbg_stack
    _dbg(name, "- enter")
    _dbg_stack += 1


def _dbg_exit(name):
    global _dbg_stack
    _dbg_stack -= 1
    _dbg(name, "- exit!")



# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------


NPOS = 18446744073709551615
MAX_SUBSTR_LEN_DISPLAY = 80
MAX_SUBSTR_LEN_EXPAND = 1000


def get_str_value(d, value, limit=0):
    # adapted from dumper.py::Dumper::putCharArrayValue()
    m_str = value["str"].pointer()
    m_len = value["len"].integer()
    if m_len == NPOS:
        _dbg("getstr... 1", m_len)
        m_str = "!!!!!<npos>!!!!!"
        m_len = len(m_str)
        return m_str, m_len
    if limit == 0:
        limit = d.displayStringLimit
    elided, shown = d.computeLimit(m_len, limit)
    mem = bytes(d.readRawMemory(m_str, shown))
    mem = mem.decode('utf8')
    return mem, m_len


def __display_csubstr(d, value, limit=0):
    m_str, m_len = get_str_value(d, value)
    safe_len = min(m_len, MAX_SUBSTR_LEN_DISPLAY)
    disp = m_str[0:safe_len]
    # ensure the string escapes characters like \n\r\t etc
    disp = disp.encode('unicode_escape').decode('utf8')
    # WATCHOUT. quotes in the string will make qtcreator hang!!!
    disp = disp.replace('"', '\\"')
    disp = disp.replace('\'', '\\')
    if m_len <= MAX_SUBSTR_LEN_DISPLAY:
        d.putValue(f"[{m_len}] '{disp}'")
    else:
        d.putValue(f"[{m_len}] '{disp}'...")
    return m_str, m_len


def qdump__c4__csubstr(d, value):
    m_str, m_len = __display_csubstr(d, value)
    d.putExpandable()
    if d.isExpanded():
        with Children(d):
            safe_len = min(m_len, MAX_SUBSTR_LEN_EXPAND)
            for i in range(safe_len):
                ct = d.createType('char')
                d.putSubItem(safe_len, d.createValue(value["str"].pointer() + i, ct))
            d.putSubItem("len", value["len"])
            d.putPtrItem("str", value["str"].pointer())


def qdump__c4__substr(d, value):
    qdump__c4__csubstr(d, value)


def qdump__c4__basic_substring(d, value):
    qdump__c4__csubstr(d, value)


def qdump__c4__yml__NodeScalar(d, value):
    alen = value["anchor"]["len"].integer()
    tlen = value["tag"   ]["len"].integer()
    m_str, m_len = get_str_value(d, value["scalar"])
    if alen == 0 and tlen == 0:
        d.putValue(f'\'{m_str}\'')
    elif alen == 0 and tlen > 0:
        d.putValue(f'\'{m_str}\' [Ta]')
    elif alen > 0 and tlen == 0:
        d.putValue(f'\'{m_str}\' [tA]')
    elif alen > 0 and tlen > 0:
        d.putValue(f'\'{m_str}\' [TA]')
    d.putExpandable()
    if d.isExpanded():
        with Children(d):
            d.putSubItem("[scalar]", value["scalar"])
            if tlen > 0:
                d.putSubItem("[tag]", value["tag"])
            if alen > 0:
                d.putSubItem("[anchor or ref]", value["anchor"])


def _format_enum_value(int_value, enum_map):
    str_value = enum_map.get(int_value, None)
    display = f'{int_value}' if str_value is None else f'{str_value} ({int_value})'
    return display


def _format_bitmask_value(int_value, enum_map):
    str_value = enum_map.get(int_value, None)
    if str_value:
        return f'{str_value} ({int_value})'
    else:
        out = ""
        orig = int_value
        # do in reverse to get compound flags first
        for k, v in reversed(enum_map.items()):
            if (k != 0):
                if (int_value & k) == k:
                    if len(out) > 0:
                        out += '|'
                    out += v
                    int_value &= ~k
            else:
                if len(out) == 0 and int_value == 0:
                    return v
        if out == "":
            return f'{int_value}'
        return f"{out} ({orig})"


def _c4bit(*ints):
    ret = 0
    for i in ints:
        ret |= 1 << i
    return ret


node_types = {
    0: "NOTYPE",
    _c4bit(0): "VAL"     ,
    _c4bit(1): "KEY"     ,
    _c4bit(2): "MAP"     ,
    _c4bit(3): "SEQ"     ,
    _c4bit(4): "DOC"     ,
    _c4bit(5,3): "STREAM",
    _c4bit(6): "KEYREF"  ,
    _c4bit(7): "VALREF"  ,
    _c4bit(8): "KEYANCH" ,
    _c4bit(9): "VALANCH" ,
    _c4bit(10): "KEYTAG" ,
    _c4bit(11): "VALTAG" ,
    _c4bit(12): "VALQUO" ,
    _c4bit(13): "KEYQUO" ,
    _c4bit(1,0): "KEYVAL",
    _c4bit(1,3): "KEYSEQ",
    _c4bit(1,2): "KEYMAP",
    _c4bit(4,2): "DOCMAP",
    _c4bit(4,3): "DOCSEQ",
    _c4bit(4,0): "DOCVAL",
    #
    _c4bit(14): "STYLE_FLOW_SL",
    _c4bit(15): "STYLE_FLOW_ML",
    _c4bit(16): "STYLE_BLOCK",
    #
    _c4bit(17): "KEY_LITERAL",
    _c4bit(18): "VAL_LITERAL",
    _c4bit(19): "KEY_FOLDED",
    _c4bit(20): "VAL_FOLDED",
    _c4bit(21): "KEY_SQUO",
    _c4bit(22): "VAL_SQUO",
    _c4bit(23): "KEY_DQUO",
    _c4bit(24): "VAL_DQUO",
    _c4bit(25): "KEY_PLAIN",
    _c4bit(26): "VAL_PLAIN",
}
node_types_rev = {v: k for k, v in node_types.items()}


def _node_type_has_all(node_type_value, type_name):
    exp = node_types_rev[type_name]
    return (node_type_value & exp) == exp


def _node_type_has_any(node_type_value, type_name):
    exp = node_types_rev[type_name]
    return (node_type_value & exp) != 0


def qdump__c4__yml__NodeType_e(d, value):
    v = _format_bitmask_value(value.integer(), node_types)
    d.putValue(v)


def qdump__c4__yml__NodeType(d, value):
    qdump__c4__yml__NodeType_e(d, value["type"])


def qdump__c4__yml__NodeData(d, value):
    d.putValue("wtf")
    ty = _format_bitmask_value(value.integer(), node_types)
    t = value["m_type"]["type"].integer()
    k = value["m_key"]["scalar"]
    v = value["m_val"]["scalar"]
    sk, lk = get_str_value(d, k)
    sv, lv = get_str_value(d, v)
    if _node_type_has_all(t, "KEYVAL"):
        d.putValue(f"'{sk}': '{sv}'    {ty}")
    elif _node_type_has_any(t, "KEY"):
        d.putValue(f"'{sk}':    {ty}")
    elif _node_type_has_any(t, "VAL"):
        d.putValue(f"'{sv}'    {ty}")
    else:
        d.putValue(f"{ty}")
    d.putExpandable()
    if d.isExpanded():
        with Children(d):
            d.putSubItem("m_type", value["m_type"])
            # key
            if _node_type_has_any(t, "KEY"):
                d.putSubItem("m_key", value["m_key"])
            if _node_type_has_any(t, "KEYREF"):
                with SubItem(d, "m_key.ref"):
                    s_, _ = get_str_value(d, value["m_key"]["anchor"])
                    d.putValue(f"'{s_}'")
            if _node_type_has_any(t, "KEYANCH"):
                with SubItem(d, "m_key.anchor"):
                    s_, _ = get_str_value(d, value["m_key"]["anchor"])
                    d.putValue(f"'{s_}'")
            if _node_type_has_any(t, "KEYTAG"):
                with SubItem(d, "m_key.tag"):
                    s_, _ = get_str_value(d, value["m_key"]["tag"])
                    d.putValue(f"'{s_}'")
            # val
            if _node_type_has_any(t, "VAL"):
                d.putSubItem("m_val", value["m_val"])
            if _node_type_has_any(t, "VALREF"):
                with SubItem(d, "m_val.ref"):
                    s_, _ = get_str_value(d, value["m_val"]["anchor"])
                    d.putValue(f"'{s_}'")
            if _node_type_has_any(t, "VALANCH"):
                with SubItem(d, "m_val.anchor"):
                    s_, _ = get_str_value(d, value["m_val"]["anchor"])
                    d.putValue(f"'{s_}'")
            if _node_type_has_any(t, "VALTAG"):
                with SubItem(d, "m_val.tag"):
                    s_, _ = get_str_value(d, value["m_val"]["tag"])
                    d.putValue(f"'{s_}'")
            # hierarchy
            _dump_node_index(d, "m_parent", value)
            _dump_node_index(d, "m_first_child", value)
            _dump_node_index(d, "m_last_child", value)
            _dump_node_index(d, "m_next_sibling", value)
            _dump_node_index(d, "m_prev_sibling", value)


def _dump_node_index(d, name, value):
    if int(value[name].integer()) == NPOS:
        pass
        #with SubItem(d, name):
        #    d.putValue("-")
    else:
        d.putSubItem(name, value[name])


# c4::yml::Tree
def qdump__c4__yml__Tree(d, value):
    m_size = value["m_size"].integer()
    m_cap = value["m_cap"].integer()
    d.putExpandable()
    if d.isExpanded():
        #d.putArrayData(value["m_buf"], m_size, value["m_buf"].dereference())
        with Children(d):
            with SubItem(d, f"[nodes]"):
                d.putItemCount(m_size)
                d.putArrayData(value["m_buf"].pointer(), m_size, value["m_buf"].type.dereference())
            d.putPtrItem("m_buf", value["m_buf"].pointer())
            d.putIntItem("m_size", value["m_size"])
            d.putIntItem("m_cap (capacity)", value["m_cap"])
            d.putIntItem("[slack]", m_cap - m_size)
            d.putIntItem("m_free_head", value["m_free_head"])
            d.putIntItem("m_free_tail", value["m_free_tail"])
            d.putSubItem("m_arena", value["m_arena"])


def qdump__c4__yml__detail__stack(d, value):
    T = value.type[0]
    N = value.type[0]
    m_size = value["m_size"].integer()
    m_capacity = value["m_capacity"].integer()
    d.putItemCount(m_size)
    if d.isExpanded():
        with Children(d):
            with SubItem(d, f"[nodes]"):
                d.putItemCount(m_size)
                d.putArrayData(value["m_stack"].pointer(), m_size, T)
            d.putIntItem("m_size", value["m_size"])
            d.putIntItem("m_capacity", value["m_capacity"])
            #d.putIntItem("[small capacity]", N)
            d.putIntItem("[is large]", value["m_buf"].address() == value["m_stack"].pointer())
            d.putPtrItem("m_stack", value["m_stack"].pointer())
            d.putPtrItem("m_buf", value["m_buf"].address())


def qdump__c4__yml__detail__ReferenceResolver__refdata(d, value):
    node = value["node"].integer()
    ty = _format_bitmask_value(value["type"].integer(), node_types)
    d.putValue(f'{node}   {ty}')
    d.putExpandable()
    if d.isExpanded():
        with Children(d):
            d.putSubItem("type", value["type"])
            d.putSubItem("node", value["node"])
            _dump_node_index(d, "prev_anchor", value)
            _dump_node_index(d, "target", value)
            _dump_node_index(d, "parent_ref", value)
            _dump_node_index(d, "parent_ref_sibling", value)
