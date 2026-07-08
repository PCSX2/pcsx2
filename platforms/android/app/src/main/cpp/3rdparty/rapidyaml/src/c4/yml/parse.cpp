#include "c4/yml/parse.hpp"

#ifndef _C4_YML_NODE_HPP_
#include "c4/yml/node.hpp"
#endif
#ifndef _C4_YML_PARSE_ENGINE_HPP_
#include "c4/yml/parse_engine.hpp"
#endif
#ifndef _C4_YML_PARSE_ENGINE_DEF_HPP_
#include "c4/yml/parse_engine.def.hpp"
#endif
#ifndef _C4_YML_EVENT_HANDLER_TREE_HPP_
#include "c4/yml/event_handler_tree.hpp"
#endif


//-----------------------------------------------------------------------------

namespace c4 {
namespace yml {

// instantiate the parser class
template class ParseEngine<EventHandlerTree>;

namespace {
inline void _reset_tree_handler(Parser *parser, Tree *t, id_type node_id)
{
    RYML_ASSERT(parser);
    RYML_ASSERT(t);
    if(!parser->m_evt_handler)
        _RYML_CB_ERR(t->m_callbacks, "event handler is not set");
    parser->m_evt_handler->reset(t, node_id);
    RYML_ASSERT(parser->m_evt_handler->m_tree == t);
}
} // namespace

void parse_in_place(Parser *parser, csubstr filename, substr yaml, Tree *t, id_type node_id)
{
    _reset_tree_handler(parser, t, node_id);
    parser->parse_in_place_ev(filename, yaml);
}

void parse_json_in_place(Parser *parser, csubstr filename, substr json, Tree *t, id_type node_id)
{
    _reset_tree_handler(parser, t, node_id);
    parser->parse_json_in_place_ev(filename, json);
}


// this is vertically aligned to highlight the parameter differences.
void parse_in_place(Parser *parser,                   substr yaml, Tree *t, id_type node_id) { parse_in_place(parser, {}, yaml, t, node_id); }
void parse_in_place(Parser *parser, csubstr filename, substr yaml, Tree *t                ) { RYML_CHECK(t); parse_in_place(parser, filename, yaml, t, t->root_id()); }
void parse_in_place(Parser *parser,                   substr yaml, Tree *t                ) { RYML_CHECK(t); parse_in_place(parser, {}      , yaml, t, t->root_id()); }
void parse_in_place(Parser *parser, csubstr filename, substr yaml, NodeRef node           ) { RYML_CHECK(!node.invalid()); parse_in_place(parser, filename, yaml, node.tree(), node.id()); }
void parse_in_place(Parser *parser,                   substr yaml, NodeRef node           ) { RYML_CHECK(!node.invalid()); parse_in_place(parser, {}      , yaml, node.tree(), node.id()); }
Tree parse_in_place(Parser *parser, csubstr filename, substr yaml                         ) { RYML_CHECK(parser); RYML_CHECK(parser->m_evt_handler); Tree tree(parser->callbacks()); parse_in_place(parser, filename, yaml, &tree, tree.root_id()); return tree; }
Tree parse_in_place(Parser *parser,                   substr yaml                         ) { RYML_CHECK(parser); RYML_CHECK(parser->m_evt_handler); Tree tree(parser->callbacks()); parse_in_place(parser, {}      , yaml, &tree, tree.root_id()); return tree; }

// this is vertically aligned to highlight the parameter differences.
void parse_in_place(csubstr filename, substr yaml, Tree *t, id_type node_id) { RYML_CHECK(t); Parser::handler_type event_handler(t->callbacks()); Parser parser(&event_handler); parse_in_place(&parser, filename, yaml, t, node_id); }
void parse_in_place(                  substr yaml, Tree *t, id_type node_id) { RYML_CHECK(t); Parser::handler_type event_handler(t->callbacks()); Parser parser(&event_handler); parse_in_place(&parser, {}      , yaml, t, node_id); }
void parse_in_place(csubstr filename, substr yaml, Tree *t                 ) { RYML_CHECK(t); Parser::handler_type event_handler(t->callbacks()); Parser parser(&event_handler); parse_in_place(&parser, filename, yaml, t, t->root_id()); }
void parse_in_place(                  substr yaml, Tree *t                 ) { RYML_CHECK(t); Parser::handler_type event_handler(t->callbacks()); Parser parser(&event_handler); parse_in_place(&parser, {}      , yaml, t, t->root_id()); }
void parse_in_place(csubstr filename, substr yaml, NodeRef node            ) { RYML_CHECK(!node.invalid()); Parser::handler_type event_handler(node.tree()->callbacks()); Parser parser(&event_handler); parse_in_place(&parser, filename, yaml, node.tree(), node.id()); }
void parse_in_place(                  substr yaml, NodeRef node            ) { RYML_CHECK(!node.invalid()); Parser::handler_type event_handler(node.tree()->callbacks()); Parser parser(&event_handler); parse_in_place(&parser, {}      , yaml, node.tree(), node.id()); }
Tree parse_in_place(csubstr filename, substr yaml                          ) { Parser::handler_type event_handler; Parser parser(&event_handler); Tree tree(parser.callbacks()); parse_in_place(&parser, filename, yaml, &tree, tree.root_id()); return tree; }
Tree parse_in_place(                  substr yaml                          ) { Parser::handler_type event_handler; Parser parser(&event_handler); Tree tree(parser.callbacks()); parse_in_place(&parser, {}      , yaml, &tree, tree.root_id()); return tree; }


// this is vertically aligned to highlight the parameter differences.
void parse_json_in_place(Parser *parser,                   substr json, Tree *t, id_type node_id) { parse_json_in_place(parser, {}, json, t, node_id); }
void parse_json_in_place(Parser *parser, csubstr filename, substr json, Tree *t                 ) { RYML_CHECK(t); parse_json_in_place(parser, filename, json, t, t->root_id()); }
void parse_json_in_place(Parser *parser,                   substr json, Tree *t                 ) { RYML_CHECK(t); parse_json_in_place(parser, {}      , json, t, t->root_id()); }
void parse_json_in_place(Parser *parser, csubstr filename, substr json, NodeRef node            ) { RYML_CHECK(!node.invalid()); parse_json_in_place(parser, filename, json, node.tree(), node.id()); }
void parse_json_in_place(Parser *parser,                   substr json, NodeRef node            ) { RYML_CHECK(!node.invalid()); parse_json_in_place(parser, {}      , json, node.tree(), node.id()); }
Tree parse_json_in_place(Parser *parser, csubstr filename, substr json                          ) { RYML_CHECK(parser); RYML_CHECK(parser->m_evt_handler); Tree tree(parser->callbacks()); parse_json_in_place(parser, filename, json, &tree, tree.root_id()); return tree; }
Tree parse_json_in_place(Parser *parser,                   substr json                          ) { RYML_CHECK(parser); RYML_CHECK(parser->m_evt_handler); Tree tree(parser->callbacks()); parse_json_in_place(parser, {}      , json, &tree, tree.root_id()); return tree; }

// this is vertically aligned to highlight the parameter differences.
void parse_json_in_place(csubstr filename, substr json, Tree *t, id_type node_id) { RYML_CHECK(t); Parser::handler_type event_handler(t->callbacks()); Parser parser(&event_handler); parse_json_in_place(&parser, filename, json, t, node_id); }
void parse_json_in_place(                  substr json, Tree *t, id_type node_id) { RYML_CHECK(t); Parser::handler_type event_handler(t->callbacks()); Parser parser(&event_handler); parse_json_in_place(&parser, {}      , json, t, node_id); }
void parse_json_in_place(csubstr filename, substr json, Tree *t                 ) { RYML_CHECK(t); Parser::handler_type event_handler(t->callbacks()); Parser parser(&event_handler); parse_json_in_place(&parser, filename, json, t, t->root_id()); }
void parse_json_in_place(                  substr json, Tree *t                 ) { RYML_CHECK(t); Parser::handler_type event_handler(t->callbacks()); Parser parser(&event_handler); parse_json_in_place(&parser, {}      , json, t, t->root_id()); }
void parse_json_in_place(csubstr filename, substr json, NodeRef node            ) { RYML_CHECK(!node.invalid()); Parser::handler_type event_handler(node.tree()->callbacks()); Parser parser(&event_handler); parse_json_in_place(&parser, filename, json, node.tree(), node.id()); }
void parse_json_in_place(                  substr json, NodeRef node            ) { RYML_CHECK(!node.invalid()); Parser::handler_type event_handler(node.tree()->callbacks()); Parser parser(&event_handler); parse_json_in_place(&parser, {}      , json, node.tree(), node.id()); }
Tree parse_json_in_place(csubstr filename, substr json                          ) { Parser::handler_type event_handler; Parser parser(&event_handler); Tree tree(parser.callbacks()); parse_json_in_place(&parser, filename, json, &tree, tree.root_id()); return tree; }
Tree parse_json_in_place(                  substr json                          ) { Parser::handler_type event_handler; Parser parser(&event_handler); Tree tree(parser.callbacks()); parse_json_in_place(&parser, {}      , json, &tree, tree.root_id()); return tree; }


// this is vertically aligned to highlight the parameter differences.
void parse_in_arena(Parser *parser, csubstr filename, csubstr yaml, Tree *t, id_type node_id) { RYML_CHECK(t); substr src = t->copy_to_arena(yaml); parse_in_place(parser, filename, src, t, node_id); }
void parse_in_arena(Parser *parser,                   csubstr yaml, Tree *t, id_type node_id) { RYML_CHECK(t); substr src = t->copy_to_arena(yaml); parse_in_place(parser, {}      , src, t, node_id); }
void parse_in_arena(Parser *parser, csubstr filename, csubstr yaml, Tree *t                 ) { RYML_CHECK(t); substr src = t->copy_to_arena(yaml); parse_in_place(parser, filename, src, t, t->root_id()); }
void parse_in_arena(Parser *parser,                   csubstr yaml, Tree *t                 ) { RYML_CHECK(t); substr src = t->copy_to_arena(yaml); parse_in_place(parser, {}      , src, t, t->root_id()); }
void parse_in_arena(Parser *parser, csubstr filename, csubstr yaml, NodeRef node            ) { RYML_CHECK(!node.invalid()); substr src = node.tree()->copy_to_arena(yaml); parse_in_place(parser, filename, src, node.tree(), node.id()); }
void parse_in_arena(Parser *parser,                   csubstr yaml, NodeRef node            ) { RYML_CHECK(!node.invalid()); substr src = node.tree()->copy_to_arena(yaml); parse_in_place(parser, {}      , src, node.tree(), node.id()); }
Tree parse_in_arena(Parser *parser, csubstr filename, csubstr yaml                          ) { RYML_CHECK(parser); RYML_CHECK(parser->m_evt_handler); Tree tree(parser->callbacks()); substr src = tree.copy_to_arena(yaml); parse_in_place(parser, filename, src, &tree, tree.root_id()); return tree; }
Tree parse_in_arena(Parser *parser,                   csubstr yaml                          ) { RYML_CHECK(parser); RYML_CHECK(parser->m_evt_handler); Tree tree(parser->callbacks()); substr src = tree.copy_to_arena(yaml); parse_in_place(parser, {}      , src, &tree, tree.root_id()); return tree; }

// this is vertically aligned to highlight the parameter differences.
void parse_in_arena(csubstr filename, csubstr yaml, Tree *t, id_type node_id) { RYML_CHECK(t); Parser::handler_type event_handler(t->callbacks()); Parser parser(&event_handler); substr src = t->copy_to_arena(yaml); parse_in_place(&parser, filename, src, t, node_id); }
void parse_in_arena(                  csubstr yaml, Tree *t, id_type node_id) { RYML_CHECK(t); Parser::handler_type event_handler(t->callbacks()); Parser parser(&event_handler); substr src = t->copy_to_arena(yaml); parse_in_place(&parser, {}      , src, t, node_id); }
void parse_in_arena(csubstr filename, csubstr yaml, Tree *t                 ) { RYML_CHECK(t); Parser::handler_type event_handler(t->callbacks()); Parser parser(&event_handler); substr src = t->copy_to_arena(yaml); parse_in_place(&parser, filename, src, t, t->root_id()); }
void parse_in_arena(                  csubstr yaml, Tree *t                 ) { RYML_CHECK(t); Parser::handler_type event_handler(t->callbacks()); Parser parser(&event_handler); substr src = t->copy_to_arena(yaml); parse_in_place(&parser, {}      , src, t, t->root_id()); }
void parse_in_arena(csubstr filename, csubstr yaml, NodeRef node            ) { RYML_CHECK(!node.invalid()); Parser::handler_type event_handler(node.tree()->callbacks()); Parser parser(&event_handler); substr src = node.tree()->copy_to_arena(yaml); parse_in_place(&parser, filename, src, node.tree(), node.id()); }
void parse_in_arena(                  csubstr yaml, NodeRef node            ) { RYML_CHECK(!node.invalid()); Parser::handler_type event_handler(node.tree()->callbacks()); Parser parser(&event_handler); substr src = node.tree()->copy_to_arena(yaml); parse_in_place(&parser, {}      , src, node.tree(), node.id()); }
Tree parse_in_arena(csubstr filename, csubstr yaml                          ) { Parser::handler_type event_handler; Parser parser(&event_handler); Tree tree(parser.callbacks()); substr src = tree.copy_to_arena(yaml); parse_in_place(&parser, filename, src, &tree, tree.root_id()); return tree; }
Tree parse_in_arena(                  csubstr yaml                          ) { Parser::handler_type event_handler; Parser parser(&event_handler); Tree tree(parser.callbacks()); substr src = tree.copy_to_arena(yaml); parse_in_place(&parser, {}      , src, &tree, tree.root_id()); return tree; }


// this is vertically aligned to highlight the parameter differences.
void parse_json_in_arena(Parser *parser, csubstr filename, csubstr json, Tree *t, id_type node_id) { RYML_CHECK(t); substr src = t->copy_to_arena(json); parse_json_in_place(parser, filename, src, t, node_id); }
void parse_json_in_arena(Parser *parser,                   csubstr json, Tree *t, id_type node_id) { RYML_CHECK(t); substr src = t->copy_to_arena(json); parse_json_in_place(parser, {}      , src, t, node_id); }
void parse_json_in_arena(Parser *parser, csubstr filename, csubstr json, Tree *t                 ) { RYML_CHECK(t); substr src = t->copy_to_arena(json); parse_json_in_place(parser, filename, src, t, t->root_id()); }
void parse_json_in_arena(Parser *parser,                   csubstr json, Tree *t                 ) { RYML_CHECK(t); substr src = t->copy_to_arena(json); parse_json_in_place(parser, {}      , src, t, t->root_id()); }
void parse_json_in_arena(Parser *parser, csubstr filename, csubstr json, NodeRef node            ) { RYML_CHECK(!node.invalid()); substr src = node.tree()->copy_to_arena(json); parse_json_in_place(parser, filename, src, node.tree(), node.id()); }
void parse_json_in_arena(Parser *parser,                   csubstr json, NodeRef node            ) { RYML_CHECK(!node.invalid()); substr src = node.tree()->copy_to_arena(json); parse_json_in_place(parser, {}      , src, node.tree(), node.id()); }
Tree parse_json_in_arena(Parser *parser, csubstr filename, csubstr json                          ) { RYML_CHECK(parser); RYML_CHECK(parser->m_evt_handler); Tree tree(parser->callbacks()); substr src = tree.copy_to_arena(json); parse_json_in_place(parser, filename, src, &tree, tree.root_id()); return tree; }
Tree parse_json_in_arena(Parser *parser,                   csubstr json                          ) { RYML_CHECK(parser); RYML_CHECK(parser->m_evt_handler); Tree tree(parser->callbacks()); substr src = tree.copy_to_arena(json); parse_json_in_place(parser, {}      , src, &tree, tree.root_id()); return tree; }

// this is vertically aligned to highlight the parameter differences.
void parse_json_in_arena(csubstr filename, csubstr json, Tree *t, id_type node_id) { RYML_CHECK(t); Parser::handler_type event_handler(t->callbacks()); Parser parser(&event_handler); substr src = t->copy_to_arena(json); parse_json_in_place(&parser, filename, src, t, node_id); }
void parse_json_in_arena(                  csubstr json, Tree *t, id_type node_id) { RYML_CHECK(t); Parser::handler_type event_handler(t->callbacks()); Parser parser(&event_handler); substr src = t->copy_to_arena(json); parse_json_in_place(&parser, {}      , src, t, node_id); }
void parse_json_in_arena(csubstr filename, csubstr json, Tree *t                 ) { RYML_CHECK(t); Parser::handler_type event_handler(t->callbacks()); Parser parser(&event_handler); substr src = t->copy_to_arena(json); parse_json_in_place(&parser, filename, src, t, t->root_id()); }
void parse_json_in_arena(                  csubstr json, Tree *t                 ) { RYML_CHECK(t); Parser::handler_type event_handler(t->callbacks()); Parser parser(&event_handler); substr src = t->copy_to_arena(json); parse_json_in_place(&parser, {}      , src, t, t->root_id()); }
void parse_json_in_arena(csubstr filename, csubstr json, NodeRef node            ) { RYML_CHECK(!node.invalid()); Parser::handler_type event_handler(node.tree()->callbacks()); Parser parser(&event_handler); substr src = node.tree()->copy_to_arena(json); parse_json_in_place(&parser, filename, src, node.tree(), node.id()); }
void parse_json_in_arena(                  csubstr json, NodeRef node            ) { RYML_CHECK(!node.invalid()); Parser::handler_type event_handler(node.tree()->callbacks()); Parser parser(&event_handler); substr src = node.tree()->copy_to_arena(json); parse_json_in_place(&parser, {}      , src, node.tree(), node.id()); }
Tree parse_json_in_arena(csubstr filename, csubstr json                          ) { Parser::handler_type event_handler; Parser parser(&event_handler); Tree tree(parser.callbacks()); substr src = tree.copy_to_arena(json); parse_json_in_place(&parser, filename, src, &tree, tree.root_id()); return tree; }
Tree parse_json_in_arena(                  csubstr json                          ) { Parser::handler_type event_handler; Parser parser(&event_handler); Tree tree(parser.callbacks()); substr src = tree.copy_to_arena(json); parse_json_in_place(&parser, {}      , src, &tree, tree.root_id()); return tree; }


//-----------------------------------------------------------------------------

RYML_EXPORT id_type estimate_tree_capacity(csubstr src)
{
    id_type num_nodes = 1; // root
    for(size_t i = 0; i < src.len; ++i)
    {
        const char c = src.str[i];
        num_nodes += (c == '\n') || (c == ',') || (c == '[') || (c == '{');
    }
    return num_nodes;
}

} // namespace yml
} // namespace c4
