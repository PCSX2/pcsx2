#ifndef C4_YML_DETAIL_PRINT_HPP_
#define C4_YML_DETAIL_PRINT_HPP_

#include "c4/yml/tree.hpp"
#include "c4/yml/node.hpp"

#ifdef RYML_DBG
#define _c4dbg_tree(...) print_tree(__VA_ARGS__)
#define _c4dbg_node(...) print_tree(__VA_ARGS__)
#else
#define _c4dbg_tree(...)
#define _c4dbg_node(...)
#endif

namespace c4 {
namespace yml {

C4_SUPPRESS_WARNING_GCC_CLANG_WITH_PUSH("-Wold-style-cast")
C4_SUPPRESS_WARNING_GCC("-Wuseless-cast")

inline const char* _container_style_code(Tree const& p, id_type node)
{
    if(p.is_container(node))
    {
        if(p._p(node)->m_type & (FLOW_SL|FLOW_ML))
        {
            return "[FLOW]";
        }
        if(p._p(node)->m_type & (BLOCK))
        {
            return "[BLCK]";
        }
    }
    return "";
}
inline char _scalar_code(NodeType masked)
{
    if(masked & (KEY_LITERAL|VAL_LITERAL))
        return '|';
    if(masked & (KEY_FOLDED|VAL_FOLDED))
        return '>';
    if(masked & (KEY_SQUO|VAL_SQUO))
        return '\'';
    if(masked & (KEY_DQUO|VAL_DQUO))
        return '"';
    if(masked & (KEY_PLAIN|VAL_PLAIN))
        return '~';
    return '@';
}
inline char _scalar_code_key(NodeType t)
{
    return _scalar_code(t & KEY_STYLE);
}
inline char _scalar_code_val(NodeType t)
{
    return _scalar_code(t & VAL_STYLE);
}
inline char _scalar_code_key(Tree const& p, id_type node)
{
    return _scalar_code_key(p._p(node)->m_type);
}
inline char _scalar_code_val(Tree const& p, id_type node)
{
    return _scalar_code_key(p._p(node)->m_type);
}
inline id_type print_node(Tree const& p, id_type node, int level, id_type count, bool print_children)
{
    printf("[%zu]%*s[%zu] %p", (size_t)count, (2*level), "", (size_t)node, (void const*)p.get(node));
    if(p.is_root(node))
    {
        printf(" [ROOT]");
    }
    char typebuf[128];
    csubstr typestr = p.type(node).type_str(typebuf);
    RYML_CHECK(typestr.str);
    printf(" %.*s", (int)typestr.len, typestr.str);
    if(p.has_key(node))
    {
        if(p.has_key_anchor(node))
        {
            csubstr ka = p.key_anchor(node);
            printf(" &%.*s", (int)ka.len, ka.str);
        }
        if(p.has_key_tag(node))
        {
            csubstr kt = p.key_tag(node);
            printf(" <%.*s>", (int)kt.len, kt.str);
        }
        const char code = _scalar_code_key(p, node);
        csubstr k  = p.key(node);
        printf(" %c%.*s%c :", code, (int)k.len, k.str, code);
    }
    if(p.has_val_anchor(node))
    {
        csubstr a = p.val_anchor(node);
        printf(" &%.*s'", (int)a.len, a.str);
    }
    if(p.has_val_tag(node))
    {
        csubstr vt = p.val_tag(node);
        printf(" <%.*s>", (int)vt.len, vt.str);
    }
    if(p.has_val(node))
    {
        const char code = _scalar_code_val(p, node);
        csubstr v  = p.val(node);
        printf(" %c%.*s%c", code, (int)v.len, v.str, code);
    }
    printf("  (%zu sibs)", (size_t)p.num_siblings(node));

    ++count;

    if(!p.is_container(node))
    {
        printf("\n");
    }
    else
    {
        printf(" (%zu children)\n", (size_t)p.num_children(node));
        if(print_children)
        {
            for(id_type i = p.first_child(node); i != NONE; i = p.next_sibling(i))
            {
                count = print_node(p, i, level+1, count, print_children);
            }
        }
    }

    return count;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

inline void print_node(ConstNodeRef const& p, int level=0)
{
    print_node(*p.tree(), p.id(), level, 0, true);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

inline id_type print_tree(const char *message, Tree const& p, id_type node=NONE)
{
    printf("--------------------------------------\n");
    if(message != nullptr)
        printf("%s:\n", message);
    id_type ret = 0;
    if(!p.empty())
    {
        if(node == NONE)
            node = p.root_id();
        ret = print_node(p, node, 0, 0, true);
    }
    printf("#nodes=%zu vs #printed=%zu\n", (size_t)p.size(), (size_t)ret);
    printf("--------------------------------------\n");
    return ret;
}

inline id_type print_tree(Tree const& p, id_type node=NONE)
{
    return print_tree(nullptr, p, node);
}

inline void print_tree(ConstNodeRef const& p, int level)
{
    print_node(p, level);
    for(ConstNodeRef ch : p.children())
    {
        print_tree(ch, level+1);
    }
}

C4_SUPPRESS_WARNING_GCC_CLANG_POP

} /* namespace yml */
} /* namespace c4 */


#endif /* C4_YML_DETAIL_PRINT_HPP_ */
