#ifndef C4_UTF_HPP_
#define C4_UTF_HPP_

#include "c4/language.hpp"
#include "c4/substr_fwd.hpp"
#include <stddef.h>
#include <stdint.h>

/** @file utf.hpp utilities for UTF and Byte Order Mark */

namespace c4 {

/** @defgroup doc_utf UTF utilities
 * @{ */


/** skip the Byte Order Mark, or get the full string if there is Byte Order Mark.
 * @see Implements the Byte Order Marks as described in https://en.wikipedia.org/wiki/Byte_order_mark#Byte-order_marks_by_encoding */
C4CORE_EXPORT substr skip_bom(substr s);
/** skip the Byte Order Mark, or get the full string if there is Byte Order Mark
 * @see Implements the Byte Order Marks as described in https://en.wikipedia.org/wiki/Byte_order_mark#Byte-order_marks_by_encoding */
C4CORE_EXPORT csubstr skip_bom(csubstr s);


/** get the Byte Order Mark, or an empty string if there is no Byte Order Mark
 * @see Implements the Byte Order Marks as described in https://en.wikipedia.org/wiki/Byte_order_mark#Byte-order_marks_by_encoding */
C4CORE_EXPORT substr get_bom(substr s);
/** get the Byte Order Mark, or an empty string if there is no Byte Order Mark
 * @see Implements the Byte Order Marks as described in https://en.wikipedia.org/wiki/Byte_order_mark#Byte-order_marks_by_encoding */
C4CORE_EXPORT csubstr get_bom(csubstr s);


/** return the position of the first character not belonging to the
 * Byte Order Mark, or 0 if there is no Byte Order Mark.
 * @see Implements the Byte Order Marks as described in https://en.wikipedia.org/wiki/Byte_order_mark#Byte-order_marks_by_encoding */
C4CORE_EXPORT size_t first_non_bom(csubstr s);


/** decode the given @p code_point, writing into the output string in
 * @p out.
 *
 * @param out the output string. must have at least 4 bytes (this is
 * asserted), and must not have a null string.
 *
 * @param code_point: must have length in ]0,8], and must not begin
 * with any of `U+`,`\\x`,`\\u,`\\U`,`0` (asserted)
 *
 * @return the part of @p out that was written, which will always be
 * at most 4 bytes.
 */
C4CORE_EXPORT substr decode_code_point(substr out, csubstr code_point);

/** decode the given @p code point, writing into the output string @p
 * buf, of size @p buflen
 *
 * @param buf the output string. must have at least 4 bytes (this is
 * asserted), and must not be null
 *
 * @param buflen the length of the output string. must be at least 4
 *
 * @param code: the code point must have length in ]0,8], and must not begin
 * with any of `U+`,`\\x`,`\\u,`\\U`,`0` (asserted)
 *
 * @return the number of written characters, which will always be
 * at most 4 bytes.
 */
C4CORE_EXPORT size_t decode_code_point(uint8_t *C4_RESTRICT buf, size_t buflen, uint32_t code);

/** @} */

} // namespace c4

#endif // C4_UTF_HPP_
