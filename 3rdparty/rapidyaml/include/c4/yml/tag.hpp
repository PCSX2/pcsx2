#ifndef _C4_YML_TAG_HPP_
#define _C4_YML_TAG_HPP_

#include <c4/yml/common.hpp>

namespace c4 {
namespace yml {

class Tree;

/** @addtogroup doc_tag_utils
 *
 * @{
 */


#ifndef RYML_MAX_TAG_DIRECTIVES
/** the maximum number of tag directives in a Tree */
#define RYML_MAX_TAG_DIRECTIVES 4
#endif

/** the integral type necessary to cover all the bits marking node tags */
using tag_bits = uint16_t;

/** a bit mask for marking tags for types */
typedef enum : tag_bits {
    TAG_NONE      =  0,
    // container types
    TAG_MAP       =  1, /**< !!map   Unordered set of key: value pairs without duplicates. @see https://yaml.org/type/map.html */
    TAG_OMAP      =  2, /**< !!omap  Ordered sequence of key: value pairs without duplicates. @see https://yaml.org/type/omap.html */
    TAG_PAIRS     =  3, /**< !!pairs Ordered sequence of key: value pairs allowing duplicates. @see https://yaml.org/type/pairs.html */
    TAG_SET       =  4, /**< !!set   Unordered set of non-equal values. @see https://yaml.org/type/set.html */
    TAG_SEQ       =  5, /**< !!seq   Sequence of arbitrary values. @see https://yaml.org/type/seq.html */
    // scalar types
    TAG_BINARY    =  6, /**< !!binary A sequence of zero or more octets (8 bit values). @see https://yaml.org/type/binary.html */
    TAG_BOOL      =  7, /**< !!bool   Mathematical Booleans. @see https://yaml.org/type/bool.html */
    TAG_FLOAT     =  8, /**< !!float  Floating-point approximation to real numbers. https://yaml.org/type/float.html */
    TAG_INT       =  9, /**< !!float  Mathematical integers. https://yaml.org/type/int.html */
    TAG_MERGE     = 10, /**< !!merge  Specify one or more mapping to be merged with the current one. https://yaml.org/type/merge.html */
    TAG_NULL      = 11, /**< !!null   Devoid of value. https://yaml.org/type/null.html */
    TAG_STR       = 12, /**< !!str    A sequence of zero or more Unicode characters. https://yaml.org/type/str.html */
    TAG_TIMESTAMP = 13, /**< !!timestamp A point in time https://yaml.org/type/timestamp.html */
    TAG_VALUE     = 14, /**< !!value  Specify the default value of a mapping https://yaml.org/type/value.html */
    TAG_YAML      = 15, /**< !!yaml   Specify the default value of a mapping https://yaml.org/type/yaml.html */
} YamlTag_e;

RYML_EXPORT YamlTag_e to_tag(csubstr tag);
RYML_EXPORT csubstr from_tag(YamlTag_e tag);
RYML_EXPORT csubstr from_tag_long(YamlTag_e tag);
RYML_EXPORT csubstr normalize_tag(csubstr tag);
RYML_EXPORT csubstr normalize_tag_long(csubstr tag);
RYML_EXPORT csubstr normalize_tag_long(csubstr tag, substr output);

RYML_EXPORT bool is_custom_tag(csubstr tag);


struct RYML_EXPORT TagDirective
{
    /** Eg <pre>!e!</pre> in <pre>%TAG !e! tag:example.com,2000:app/</pre> */
    csubstr handle;
    /** Eg <pre>tag:example.com,2000:app/</pre> in <pre>%TAG !e! tag:example.com,2000:app/</pre> */
    csubstr prefix;
    /** The next node to which this tag directive applies */
    id_type next_node_id;

    bool create_from_str(csubstr directive_); ///< leaves next_node_id unfilled
    size_t transform(csubstr tag, substr output, Callbacks const& callbacks, bool with_brackets=true) const;
};

struct RYML_EXPORT TagDirectiveRange
{
    TagDirective const* C4_RESTRICT b;
    TagDirective const* C4_RESTRICT e;
    C4_ALWAYS_INLINE TagDirective const* begin() const noexcept { return b; }
    C4_ALWAYS_INLINE TagDirective const* end() const noexcept { return e; }
};

/** @} */

} // namespace yml
} // namespace c4

#endif /* _C4_YML_TAG_HPP_ */
