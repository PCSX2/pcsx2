#ifndef _C4_YML_PARSE_ENGINE_HPP_
#define _C4_YML_PARSE_ENGINE_HPP_

#ifndef _C4_YML_PARSER_STATE_HPP_
#include "c4/yml/parser_state.hpp"
#endif


#if defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable: 4251/*needs to have dll-interface to be used by clients of struct*/)
#endif

// NOLINTBEGIN(hicpp-signed-bitwise)

namespace c4 {
namespace yml {

/** @addtogroup doc_parse
 * @{ */

/** @defgroup doc_event_handlers Event Handlers
 *
 * @brief rapidyaml implements its parsing logic with a two-level
 * model, where a @ref ParseEngine object reads through the YAML
 * source, and dispatches events to an EventHandler bound to the @ref
 * ParseEngine. Because @ref ParseEngine is templated on the event
 * handler, the binding uses static polymorphism, without any virtual
 * functions. The actual handler object can be changed at run time,
 * (but of course needs to be the type of the template parameter).
 * This is thus a very efficient architecture, and further enables the
 * user to provide his own custom handler if he wishes to bypass the
 * rapidyaml @ref Tree.
 *
 * There are two handlers implemented in this project:
 *
 * - @ref EventHandlerTree is the handler responsible for creating the
 *   ryml @ref Tree
 *
 * - @ref extra::EventHandlerInts parses YAML into an integer array
     representation of the tree and scalars.
 *
 * - @ref extra::EventHandlerTestSuite is the handler responsible for emitting
 *   standardized [YAML test suite
 *   events](https://github.com/yaml/yaml-test-suite), used (only) in
 *   the CI of this project.
 *
 *
 * ### Event model
 *
 * The event model used by the parse engine and event handlers follows
 * very closely the event model in the [YAML test
 * suite](https://github.com/yaml/yaml-test-suite).
 *
 * Consider for example this YAML,
 * ```yaml
 * {foo: bar,foo2: bar2}
 * ```
 * which would produce these events in the test-suite parlance:
 * ```
 * +STR
 * +DOC
 * +MAP {}
 * =VAL :foo
 * =VAL :bar
 * =VAL :foo2
 * =VAL :bar2
 * -MAP
 * -DOC
 * -STR
 * ```
 *
 * For reference, the @ref ParseEngine object will produce this
 * sequence of calls to its bound EventHandler:
 * ```cpp
 * handler.begin_stream();
 * handler.begin_doc();
 * handler.begin_map_val_flow();
 * handler.set_key_scalar_plain("foo");
 * handler.set_val_scalar_plain("bar");
 * handler.add_sibling();
 * handler.set_key_scalar_plain("foo2");
 * handler.set_val_scalar_plain("bar2");
 * handler.end_map();
 * handler.end_doc();
 * handler.end_stream();
 * ```
 *
 * For many other examples of all areas of YAML and how ryml's parse
 * model corresponds to the YAML standard model, refer to the [unit
 * tests for the parse
 * engine](https://github.com/biojppm/rapidyaml/tree/master/test/test_parse_engine.cpp).
 *
 *
 * ### Special events
 *
 * Most of the parsing events adopted by rapidyaml in its event model
 * are fairly obvious, but there are two less-obvious events requiring
 * some explanation.
 *
 * These events exist to make it easier to parse some special YAML
 * cases. They are called by the parser when a just-handled
 * value/container is actually the first key of a new map:
 *
 *   - `actually_val_is_first_key_of_new_map_flow()` (@ref EventHandlerTree::actually_val_is_first_key_of_new_map_flow() "see implementation in EventHandlerTree" / @ref EventHandlerTestSuite::actually_val_is_first_key_of_new_map_flow() "see implementation in EventHandlerTestSuite")
 *   - `actually_val_is_first_key_of_new_map_block()` (@ref EventHandlerTree::actually_val_is_first_key_of_new_map_block() "see implementation in EventHandlerTree" / @ref EventHandlerTestSuite::actually_val_is_first_key_of_new_map_block() "see implementation in EventHandlerTestSuite")
 *
 * For example, consider an implicit map inside a seq: `[a: b, c:
 * d]` which is parsed as `[{a: b}, {c: d}]`. The standard event
 * sequence for this YAML would be the following:
 * ```cpp
 * handler.begin_seq_val_flow();
 * handler.begin_map_val_flow();
 * handler.set_key_scalar_plain("a");
 * handler.set_val_scalar_plain("b");
 * handler.end_map();
 * handler.add_sibling();
 * handler.begin_map_val_flow();
 * handler.set_key_scalar_plain("c");
 * handler.set_val_scalar_plain("d");
 * handler.end_map();
 * handler.end_seq();
 * ```
 * The problem with this event sequence is that it forces the
 * parser to delay setting the val scalar (in this case "a" and
 * "c") until it knows whether the scalar is a key or a val. This
 * would require the parser to store the scalar until this
 * time. For instance, in the example above, the parser should
 * delay setting "a" and "c", because they are in fact keys and
 * not vals. Until then, the parser would have to store "a" and
 * "c" in its internal state. The downside is that this complexity
 * cost would apply even if there is no implicit map -- every val
 * in a seq would have to be delayed until one of the
 * disambiguating subsequent tokens `,-]:` is found.
 * By calling this function, the parser can avoid this complexity,
 * by preemptively setting the scalar as a val. Then a call to
 * this function will create the map and rearrange the scalar as
 * key. Now the cost applies only once: when a seqimap starts. So
 * the following (easier and cheaper) event sequence below has the
 * same effect as the event sequence above:
 * ```cpp
 * handler.begin_seq_val_flow();
 * handler.set_val_scalar_plain("notmap");
 * handler.set_val_scalar_plain("a"); // preemptively set "a" as val!
 * handler.actually_as_new_map_key(); // create a map, move the "a" val as the key of the first child of the new map
 * handler.set_val_scalar_plain("b"); // now "a" is a key and "b" the val
 * handler.end_map();
 * handler.set_val_scalar_plain("c"); // "c" also as val!
 * handler.actually_as_block_flow();  // likewise
 * handler.set_val_scalar_plain("d"); // now "c" is a key and "b" the val
 * handler.end_map();
 * handler.end_seq();
 * ```
 * This also applies to container keys (although ryml's tree
 * cannot accomodate these): the parser can preemptively set a
 * container as a val, and call this event to turn that container
 * into a key. For example, consider this yaml:
 * ```yaml
 *   [aa, bb]: [cc, dd]
 * # ^       ^ ^
 * # |       | |
 * # (2)   (1) (3)     <- event sequence
 * ```
 * The standard event sequence for this YAML would be the
 * following:
 * ```cpp
 * handler.begin_map_val_block();       // (1)
 * handler.begin_seq_key_flow();        // (2)
 * handler.set_val_scalar_plain("aa");
 * handler.add_sibling();
 * handler.set_val_scalar_plain("bb");
 * handler.end_seq();
 * handler.begin_seq_val_flow();        // (3)
 * handler.set_val_scalar_plain("cc");
 * handler.add_sibling();
 * handler.set_val_scalar_plain("dd");
 * handler.end_seq();
 * handler.end_map();
 * ```
 * The problem with the sequence above is that, reading from
 * left-to-right, the parser can only detect the proper calls at
 * (1) and (2) once it reaches (1) in the YAML source. So, the
 * parser would have to buffer the entire event sequence starting
 * from the beginning until it reaches (1). Using this function,
 * the parser can do instead:
 * ```cpp
 * handler.begin_seq_val_flow();        // (2) -- preemptively as val!
 * handler.set_val_scalar_plain("aa");
 * handler.add_sibling();
 * handler.set_val_scalar_plain("bb");
 * handler.end_seq();
 * handler.actually_as_new_map_key();   // (1) -- adjust when finding that the prev val was actually a key.
 * handler.begin_seq_val_flow();        // (3) -- go on as before
 * handler.set_val_scalar_plain("cc");
 * handler.add_sibling();
 * handler.set_val_scalar_plain("dd");
 * handler.end_seq();
 * handler.end_map();
 * ```
 */

class Tree;
class NodeRef;
class ConstNodeRef;


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** Options to give to the parser to control its behavior. */
struct RYML_EXPORT ParserOptions
{
private:

    typedef enum : uint32_t {
        SCALAR_FILTERING = (1u << 0u),
        LOCATIONS = (1u << 1u),
        DEFAULTS = SCALAR_FILTERING,
    } Flags_e;

    uint32_t flags = DEFAULTS;

public:

    ParserOptions() = default;

public:

    /** @name source location tracking */
    /** @{ */

    /** enable/disable source location tracking */
    ParserOptions& locations(bool enabled) noexcept
    {
        if(enabled)
            flags |= LOCATIONS;
        else
            flags &= ~LOCATIONS;
        return *this;
    }
    /** query source location tracking status */
    C4_ALWAYS_INLINE bool locations() const noexcept { return (flags & LOCATIONS); }

    /** @} */

public:

    /** @name scalar filtering status (experimental; disable at your discretion) */
    /** @{ */

    /** enable/disable scalar filtering while parsing */
    ParserOptions& scalar_filtering(bool enabled) noexcept
    {
        if(enabled)
            flags |= SCALAR_FILTERING;
        else
            flags &= ~SCALAR_FILTERING;
        return *this;
    }
    /** query scalar filtering status */
    C4_ALWAYS_INLINE bool scalar_filtering() const noexcept { return (flags & SCALAR_FILTERING); }

    /** @} */
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** This is the main driver of parsing logic: it scans the YAML or
 * JSON source for tokens, and emits the appropriate sequence of
 * parsing events to its event handler. The parse engine itself has no
 * special limitations, and *can* accomodate containers as keys; it is the
 * event handler may introduce additional constraints.
 *
 * There are two implemented handlers (see @ref doc_event_handlers,
 * which has important notes about the event model):
 *
 * - @ref EventHandlerTree is the handler responsible for creating the
 *   ryml @ref Tree
 *
 * - @ref extra::EventHandlerTestSuite is a handler responsible for emitting
 *   standardized [YAML test suite
 *   events](https://github.com/yaml/yaml-test-suite), used (only) in
 *   the CI of this project. This is not part of the library and is
 *   not installed.
 *
 * - @ref extra::EventHandlerInts is the handler responsible for
 *   emitting integer-coded events. It is intended for implementing
 *   fully-conformant parsing in other programming languages
 *   (integration is currently under work for
 *   [YamlScript](https://github.com/yaml/yamlscript) and
 *   [go-yaml](https://github.com/yaml/go-yaml/)). It is not part of
 *   the library and is not installed.
 *
 */
template<class EventHandler>
class ParseEngine
{
public:

    using handler_type = EventHandler;

public:

    /** @name construction and assignment */
    /** @{ */

    ParseEngine(EventHandler *evt_handler, ParserOptions opts={});
    ~ParseEngine();

    ParseEngine(ParseEngine &&) noexcept;
    ParseEngine(ParseEngine const&);
    ParseEngine& operator=(ParseEngine &&) noexcept;
    ParseEngine& operator=(ParseEngine const&);

    /** @} */

public:

    /** @name modifiers */
    /** @{ */

    /** Reserve a certain capacity for the parsing stack.
     * This should be larger than the expected depth of the parsed
     * YAML tree.
     *
     * The parsing stack is the only (potential) heap memory used
     * directly by the parser.
     *
     * If the requested capacity is below the default
     * stack size of 16, the memory is used directly in the parser
     * object; otherwise it will be allocated from the heap.
     *
     * @note this reserves memory only for the parser itself; all the
     * allocations for the parsed tree will go through the tree's
     * allocator (when different).
     *
     * @note for maximum efficiency, the tree and the arena can (and
     * should) also be reserved. */
    void reserve_stack(id_type capacity)
    {
        m_evt_handler->m_stack.reserve(capacity);
    }

    /** Reserve a certain capacity for the array used to track node
     * locations in the source buffer. */
    void reserve_locations(size_t num_source_lines)
    {
        _resize_locations(num_source_lines);
    }

    RYML_DEPRECATED("filter arena no longer needed")
    void reserve_filter_arena(size_t) {}

    /** @} */

public:

    /** @name getters */
    /** @{ */

    /** Get the options used to build this parser object. */
    ParserOptions const& options() const { return m_options; }

    /** Get the current callbacks in the parser. */
    Callbacks const& callbacks() const { RYML_ASSERT(m_evt_handler); return m_evt_handler->m_stack.m_callbacks; }

    /** Get the name of the latest file parsed by this object. */
    csubstr filename() const { return m_file; }

    /** Get the latest YAML buffer parsed by this object. */
    csubstr source() const { return m_buf; }

    /** Get the encoding of the latest YAML buffer parsed by this object.
     * If no encoding was specified, UTF8 is assumed as per the YAML standard. */
    Encoding_e encoding() const { return m_encoding != NOBOM ? m_encoding : UTF8; }

    id_type stack_capacity() const { RYML_ASSERT(m_evt_handler); return m_evt_handler->m_stack.capacity(); }
    size_t locations_capacity() const { return m_newline_offsets_capacity; }

    RYML_DEPRECATED("filter arena no longer needed")
    size_t filter_arena_capacity() const { return 0u; }

    /** @} */

public:

    /** @name parse methods */
    /** @{ */

    /** parse YAML in place, emitting events to the current handler */
    void parse_in_place_ev(csubstr filename, substr src);

    /** parse JSON in place, emitting events to the current handler */
    void parse_json_in_place_ev(csubstr filename, substr src);

    /** @} */

public:

    // deprecated parse methods

    /** @cond dev */
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding function in parse.hpp.") typename std::enable_if<U::is_wtree, void>::type parse_in_place(csubstr filename, substr yaml, Tree *t, size_t node_id);
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding function in parse.hpp.") typename std::enable_if<U::is_wtree, void>::type parse_in_place(                  substr yaml, Tree *t, size_t node_id);
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding function in parse.hpp.") typename std::enable_if<U::is_wtree, void>::type parse_in_place(csubstr filename, substr yaml, Tree *t                );
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding function in parse.hpp.") typename std::enable_if<U::is_wtree, void>::type parse_in_place(                  substr yaml, Tree *t                );
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding function in parse.hpp.") typename std::enable_if<U::is_wtree, void>::type parse_in_place(csubstr filename, substr yaml, NodeRef node           );
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding function in parse.hpp.") typename std::enable_if<U::is_wtree, void>::type parse_in_place(                  substr yaml, NodeRef node           );
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding function in parse.hpp.") typename std::enable_if<U::is_wtree, Tree>::type parse_in_place(csubstr filename, substr yaml                         );
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding function in parse.hpp.") typename std::enable_if<U::is_wtree, Tree>::type parse_in_place(                  substr yaml                         );
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding function in parse.hpp.") typename std::enable_if<U::is_wtree, void>::type parse_in_arena(csubstr filename, csubstr yaml, Tree *t, size_t node_id);
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding function in parse.hpp.") typename std::enable_if<U::is_wtree, void>::type parse_in_arena(                  csubstr yaml, Tree *t, size_t node_id);
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding function in parse.hpp.") typename std::enable_if<U::is_wtree, void>::type parse_in_arena(csubstr filename, csubstr yaml, Tree *t                );
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding function in parse.hpp.") typename std::enable_if<U::is_wtree, void>::type parse_in_arena(                  csubstr yaml, Tree *t                );
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding function in parse.hpp.") typename std::enable_if<U::is_wtree, void>::type parse_in_arena(csubstr filename, csubstr yaml, NodeRef node           );
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding function in parse.hpp.") typename std::enable_if<U::is_wtree, void>::type parse_in_arena(                  csubstr yaml, NodeRef node           );
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding function in parse.hpp.") typename std::enable_if<U::is_wtree, Tree>::type parse_in_arena(csubstr filename, csubstr yaml                         );
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding function in parse.hpp.") typename std::enable_if<U::is_wtree, Tree>::type parse_in_arena(                  csubstr yaml                         );
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding csubstr version in parse.hpp.") typename std::enable_if<U::is_wtree, void>::type parse_in_arena(csubstr filename, substr yaml, Tree *t, size_t node_id);
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding csubstr version in parse.hpp.") typename std::enable_if<U::is_wtree, void>::type parse_in_arena(                  substr yaml, Tree *t, size_t node_id);
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding csubstr version in parse.hpp.") typename std::enable_if<U::is_wtree, void>::type parse_in_arena(csubstr filename, substr yaml, Tree *t                );
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding csubstr version in parse.hpp.") typename std::enable_if<U::is_wtree, void>::type parse_in_arena(                  substr yaml, Tree *t                );
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding csubstr version in parse.hpp.") typename std::enable_if<U::is_wtree, void>::type parse_in_arena(csubstr filename, substr yaml, NodeRef node           );
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding csubstr version in parse.hpp.") typename std::enable_if<U::is_wtree, void>::type parse_in_arena(                  substr yaml, NodeRef node           );
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding csubstr version in parse.hpp.") typename std::enable_if<U::is_wtree, Tree>::type parse_in_arena(csubstr filename, substr yaml                         );
    template<class U=EventHandler> RYML_DEPRECATED("removed, deliberately undefined. use the freestanding csubstr version in parse.hpp.") typename std::enable_if<U::is_wtree, Tree>::type parse_in_arena(                  substr yaml                         );
    /** @endcond */

public:

    /** @name locations */
    /** @{ */

    /** Get the string starting at a particular location, to the end
     * of the parsed source buffer. */
    csubstr location_contents(Location const& loc) const;

    /** Given a pointer to a buffer position, get the location.
     * @param[in] val must be pointing to somewhere in the source
     * buffer that was last parsed by this object. */
    Location val_location(const char *val) const;

    /** @} */

public:

    /** @cond dev */
    template<class U>
    RYML_DEPRECATED("moved to Tree::location(Parser const&). deliberately undefined here.")
    auto location(Tree const&, id_type node) const -> typename std::enable_if<U::is_wtree, Location>::type;

    template<class U>
    RYML_DEPRECATED("moved to ConstNodeRef::location(Parser const&), deliberately undefined here.")
    auto location(ConstNodeRef const&) const -> typename std::enable_if<U::is_wtree, Location>::type;
    /** @endcond */

public:

    /** @name scalar filtering */
    /** @{*/

    /** filter a plain scalar */
    FilterResult filter_scalar_plain(csubstr scalar, substr dst, size_t indentation);
    /** filter a plain scalar in place */
    FilterResult filter_scalar_plain_in_place(substr scalar, size_t cap, size_t indentation);

    /** filter a single-quoted scalar */
    FilterResult filter_scalar_squoted(csubstr scalar, substr dst);
    /** filter a single-quoted scalar in place */
    FilterResult filter_scalar_squoted_in_place(substr scalar, size_t cap);

    /** filter a double-quoted scalar */
    FilterResult filter_scalar_dquoted(csubstr scalar, substr dst);
    /** filter a double-quoted scalar in place */
    FilterResultExtending filter_scalar_dquoted_in_place(substr scalar, size_t cap);

    /** filter a block-literal scalar */
    FilterResult filter_scalar_block_literal(csubstr scalar, substr dst, size_t indentation, BlockChomp_e chomp);
    /** filter a block-literal scalar in place */
    FilterResult filter_scalar_block_literal_in_place(substr scalar, size_t cap, size_t indentation, BlockChomp_e chomp);

    /** filter a block-folded scalar */
    FilterResult filter_scalar_block_folded(csubstr scalar, substr dst, size_t indentation, BlockChomp_e chomp);
    /** filter a block-folded scalar in place */
    FilterResult filter_scalar_block_folded_in_place(substr scalar, size_t cap, size_t indentation, BlockChomp_e chomp);

    /** @} */

private:

    struct ScannedScalar
    {
        substr scalar;
        bool needs_filter;
    };

    struct ScannedBlock
    {
        substr scalar;
        size_t indentation;
        BlockChomp_e chomp;
    };

    bool    _is_doc_begin(csubstr s);
    bool    _is_doc_end(csubstr s);

    bool    _scan_scalar_plain_blck(ScannedScalar *C4_RESTRICT sc, size_t indentation);
    bool    _scan_scalar_plain_seq_flow(ScannedScalar *C4_RESTRICT sc);
    bool    _scan_scalar_plain_seq_blck(ScannedScalar *C4_RESTRICT sc);
    bool    _scan_scalar_plain_map_flow(ScannedScalar *C4_RESTRICT sc);
    bool    _scan_scalar_plain_map_blck(ScannedScalar *C4_RESTRICT sc);
    bool    _scan_scalar_map_json(ScannedScalar *C4_RESTRICT sc);
    bool    _scan_scalar_seq_json(ScannedScalar *C4_RESTRICT sc);
    bool    _scan_scalar_plain_unk(ScannedScalar *C4_RESTRICT sc);
    bool    _is_valid_start_scalar_plain_flow(csubstr s);

    ScannedScalar _scan_scalar_squot();
    ScannedScalar _scan_scalar_dquot();

    void    _scan_block(ScannedBlock *C4_RESTRICT sb, size_t indref);

    csubstr _scan_anchor();
    csubstr _scan_ref_seq();
    csubstr _scan_ref_map();
    csubstr _scan_tag();

public: // exposed for testing

    /** @cond dev */
    csubstr _filter_scalar_plain(substr s, size_t indentation);
    csubstr _filter_scalar_squot(substr s);
    csubstr _filter_scalar_dquot(substr s);
    csubstr _filter_scalar_literal(substr s, size_t indentation, BlockChomp_e chomp);
    csubstr _filter_scalar_folded(substr s, size_t indentation, BlockChomp_e chomp);
    csubstr _move_scalar_left_and_add_newline(substr s);

    csubstr _maybe_filter_key_scalar_plain(ScannedScalar const& sc, size_t indendation);
    csubstr _maybe_filter_val_scalar_plain(ScannedScalar const& sc, size_t indendation);
    csubstr _maybe_filter_key_scalar_squot(ScannedScalar const& sc);
    csubstr _maybe_filter_val_scalar_squot(ScannedScalar const& sc);
    csubstr _maybe_filter_key_scalar_dquot(ScannedScalar const& sc);
    csubstr _maybe_filter_val_scalar_dquot(ScannedScalar const& sc);
    csubstr _maybe_filter_key_scalar_literal(ScannedBlock const& sb);
    csubstr _maybe_filter_val_scalar_literal(ScannedBlock const& sb);
    csubstr _maybe_filter_key_scalar_folded(ScannedBlock const& sb);
    csubstr _maybe_filter_val_scalar_folded(ScannedBlock const& sb);
    /** @endcond */

private:

    void  _handle_map_block();
    void  _handle_seq_block();
    void  _handle_map_flow();
    void  _handle_seq_flow();
    void  _handle_seq_imap();
    void  _handle_map_json();
    void  _handle_seq_json();

    void  _handle_unk();
    void  _handle_unk_json();
    void  _handle_usty();

    void  _handle_flow_skip_whitespace();

    void  _end_map_blck();
    void  _end_seq_blck();
    void  _end2_map();
    void  _end2_seq();

    void  _begin2_doc();
    void  _begin2_doc_expl();
    void  _end2_doc();
    void  _end2_doc_expl();

    void  _maybe_begin_doc();
    void  _maybe_end_doc();

    void  _start_doc_suddenly();
    void  _end_doc_suddenly();
    void  _end_doc_suddenly__pop();
    void  _end_stream();

    void  _set_indentation(size_t indentation);
    void  _save_indentation();
    void  _handle_indentation_pop_from_block_seq();
    void  _handle_indentation_pop_from_block_map();
    void  _handle_indentation_pop(ParserState const* dst);

    void _maybe_skip_comment();
    void _skip_comment();
    void _maybe_skip_whitespace_tokens();
    void _maybe_skipchars(char c);
    #ifdef RYML_NO_COVERAGE__TO_BE_DELETED
    void _maybe_skipchars_up_to(char c, size_t max_to_skip);
    #endif
    template<size_t N>
    void _skipchars(const char (&chars)[N]);
    bool _maybe_scan_following_colon() noexcept;
    bool _maybe_scan_following_comma() noexcept;

public:

    /** @cond dev */
    template<class FilterProcessor> auto _filter_plain(FilterProcessor &C4_RESTRICT proc, size_t indentation) -> decltype(proc.result());
    template<class FilterProcessor> auto _filter_squoted(FilterProcessor &C4_RESTRICT proc) -> decltype(proc.result());
    template<class FilterProcessor> auto _filter_dquoted(FilterProcessor &C4_RESTRICT proc) -> decltype(proc.result());
    template<class FilterProcessor> auto _filter_block_literal(FilterProcessor &C4_RESTRICT proc, size_t indentation, BlockChomp_e chomp) -> decltype(proc.result());
    template<class FilterProcessor> auto _filter_block_folded(FilterProcessor &C4_RESTRICT proc, size_t indentation, BlockChomp_e chomp) -> decltype(proc.result());
    /** @endcond */

public:

    /** @cond dev */
    template<class FilterProcessor> void   _filter_nl_plain(FilterProcessor &C4_RESTRICT proc, size_t indentation);
    template<class FilterProcessor> void   _filter_nl_squoted(FilterProcessor &C4_RESTRICT proc);
    template<class FilterProcessor> void   _filter_nl_dquoted(FilterProcessor &C4_RESTRICT proc);

    template<class FilterProcessor> bool   _filter_ws_handle_to_first_non_space(FilterProcessor &C4_RESTRICT proc);
    template<class FilterProcessor> void   _filter_ws_copy_trailing(FilterProcessor &C4_RESTRICT proc);
    template<class FilterProcessor> void   _filter_ws_skip_trailing(FilterProcessor &C4_RESTRICT proc);

    template<class FilterProcessor> void   _filter_dquoted_backslash(FilterProcessor &C4_RESTRICT proc);

    template<class FilterProcessor> void   _filter_chomp(FilterProcessor &C4_RESTRICT proc, BlockChomp_e chomp, size_t indentation);
    template<class FilterProcessor> size_t _handle_all_whitespace(FilterProcessor &C4_RESTRICT proc, BlockChomp_e chomp);
    template<class FilterProcessor> size_t _extend_to_chomp(FilterProcessor &C4_RESTRICT proc, size_t contents_len);
    template<class FilterProcessor> void   _filter_block_indentation(FilterProcessor &C4_RESTRICT proc, size_t indentation);
    template<class FilterProcessor> void   _filter_block_folded_newlines(FilterProcessor &C4_RESTRICT proc, size_t indentation, size_t len);
    template<class FilterProcessor> size_t _filter_block_folded_newlines_compress(FilterProcessor &C4_RESTRICT proc, size_t num_newl, size_t wpos_at_first_newl);
    template<class FilterProcessor> void   _filter_block_folded_newlines_leading(FilterProcessor &C4_RESTRICT proc, size_t indentation, size_t len);
    template<class FilterProcessor> void   _filter_block_folded_indented_block(FilterProcessor &C4_RESTRICT proc, size_t indentation, size_t len, size_t curr_indentation) noexcept;

    /** @endcond */

private:

    void _line_progressed(size_t ahead);
    void _line_ended();
    void _line_ended_undo();

    bool  _finished_file() const;
    bool  _finished_line() const;

    void   _scan_line();
    substr _peek_next_line(size_t pos=npos) const;

    bool _at_line_begin() const
    {
        return m_evt_handler->m_curr->line_contents.rem.begin() == m_evt_handler->m_curr->line_contents.full.begin();
    }

    void _relocate_arena(csubstr prev_arena, substr next_arena);
    static void _s_relocate_arena(void*, csubstr prev_arena, substr next_arena);

private:

    C4_ALWAYS_INLINE bool has_all(ParserFlag_t f) const noexcept { return (m_evt_handler->m_curr->flags & f) == f; }
    C4_ALWAYS_INLINE bool has_any(ParserFlag_t f) const noexcept { return (m_evt_handler->m_curr->flags & f) != 0; }
    C4_ALWAYS_INLINE bool has_none(ParserFlag_t f) const noexcept { return (m_evt_handler->m_curr->flags & f) == 0; }
    static C4_ALWAYS_INLINE bool has_all(ParserFlag_t f, ParserState const* C4_RESTRICT s) noexcept { return (s->flags & f) == f; }
    static C4_ALWAYS_INLINE bool has_any(ParserFlag_t f, ParserState const* C4_RESTRICT s) noexcept { return (s->flags & f) != 0; }
    static C4_ALWAYS_INLINE bool has_none(ParserFlag_t f, ParserState const* C4_RESTRICT s) noexcept { return (s->flags & f) == 0; }

    #ifndef RYML_DBG
    C4_ALWAYS_INLINE static void add_flags(ParserFlag_t on, ParserState *C4_RESTRICT s) noexcept { s->flags |= on; }
    C4_ALWAYS_INLINE static void addrem_flags(ParserFlag_t on, ParserFlag_t off, ParserState *C4_RESTRICT s) noexcept { s->flags &= ~off; s->flags |= on; }
    C4_ALWAYS_INLINE static void rem_flags(ParserFlag_t off, ParserState *C4_RESTRICT s) noexcept { s->flags &= ~off; }
    C4_ALWAYS_INLINE void add_flags(ParserFlag_t on) noexcept { m_evt_handler->m_curr->flags |= on; }
    C4_ALWAYS_INLINE void addrem_flags(ParserFlag_t on, ParserFlag_t off) noexcept { m_evt_handler->m_curr->flags &= ~off; m_evt_handler->m_curr->flags |= on; }
    C4_ALWAYS_INLINE void rem_flags(ParserFlag_t off) noexcept { m_evt_handler->m_curr->flags &= ~off; }
    #else
    static void add_flags(ParserFlag_t on, ParserState *C4_RESTRICT s);
    static void addrem_flags(ParserFlag_t on, ParserFlag_t off, ParserState *C4_RESTRICT s);
    static void rem_flags(ParserFlag_t off, ParserState *C4_RESTRICT s);
    C4_ALWAYS_INLINE void add_flags(ParserFlag_t on) noexcept { add_flags(on, m_evt_handler->m_curr); }
    C4_ALWAYS_INLINE void addrem_flags(ParserFlag_t on, ParserFlag_t off) noexcept { addrem_flags(on, off, m_evt_handler->m_curr); }
    C4_ALWAYS_INLINE void rem_flags(ParserFlag_t off) noexcept { rem_flags(off, m_evt_handler->m_curr); }
    #endif

private:

    void _prepare_locations();
    void _resize_locations(size_t sz);
    bool _locations_dirty() const;

private:

    void _reset();
    void _free();
    void _clr();

    #ifdef RYML_DBG
    template<class ...Args> void _dbg(csubstr fmt, Args const& C4_RESTRICT ...args) const;
    #endif
    template<class ...Args> void _err(csubstr fmt, Args const& C4_RESTRICT ...args) const;
    template<class ...Args> void _errloc(csubstr fmt, Location const& loc, Args const& C4_RESTRICT ...args) const;

    template<class DumpFn>  void _fmt_msg(DumpFn &&dumpfn) const;

private:

    /** store pending tag or anchor/ref annotations */
    struct Annotation
    {
        struct Entry
        {
            csubstr str;
            size_t indentation;
            size_t line;
        };
        Entry annotations[2];
        size_t num_entries;
    };

    void _handle_colon();
    void _add_annotation(Annotation *C4_RESTRICT dst, csubstr str, size_t indentation, size_t line);
    void _clear_annotations(Annotation *C4_RESTRICT dst);
    bool _has_pending_annotations() const { return m_pending_tags.num_entries || m_pending_anchors.num_entries; }
    #ifdef RYML_NO_COVERAGE__TO_BE_DELETED
    bool _handle_indentation_from_annotations();
    #endif
    bool _annotations_require_key_container() const;
    void _handle_annotations_before_blck_key_scalar();
    void _handle_annotations_before_blck_val_scalar();
    void _handle_annotations_before_start_mapblck(size_t current_line);
    void _handle_annotations_before_start_mapblck_as_key();
    void _handle_annotations_and_indentation_after_start_mapblck(size_t key_indentation, size_t key_line);
    size_t _select_indentation_from_annotations(size_t val_indentation, size_t val_line);
    void _handle_directive(csubstr rem);
    bool _handle_bom();
    void _handle_bom(Encoding_e enc);

    void _check_tag(csubstr tag);

private:

    ParserOptions m_options;

    csubstr m_file;
    substr  m_buf;

public:

    /** @cond dev */
    EventHandler *C4_RESTRICT m_evt_handler; // NOLINT
    /** @endcond */

private:

    Annotation m_pending_anchors;
    Annotation m_pending_tags;

    bool m_was_inside_qmrk;
    bool m_doc_empty = true;
    size_t m_prev_colon = npos;

    Encoding_e m_encoding = UTF8;

private:

    size_t *m_newline_offsets;
    size_t  m_newline_offsets_size;
    size_t  m_newline_offsets_capacity;
    csubstr m_newline_offsets_buf;

};


/** Quickly inspect the source to estimate the number of nodes the
 * resulting tree is likely have. If a tree is empty before
 * parsing, considerable time will be spent growing it, so calling
 * this to reserve the tree size prior to parsing is likely to
 * result in a time gain. We encourage using this method before
 * parsing, but as always measure its impact in performance to
 * obtain a good trade-off.
 *
 * @note since this method is meant for optimizing performance, it
 * is approximate. The result may be actually smaller than the
 * resulting number of nodes, notably if the YAML uses implicit
 * maps as flow seq members as in `[these: are, individual:
 * maps]`. */
RYML_EXPORT id_type estimate_tree_capacity(csubstr src); // NOLINT(readability-redundant-declaration)

/** @} */

} // namespace yml
} // namespace c4

// NOLINTEND(hicpp-signed-bitwise)

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif

#endif /* _C4_YML_PARSE_ENGINE_HPP_ */
