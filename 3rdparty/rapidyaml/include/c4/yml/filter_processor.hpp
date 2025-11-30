#ifndef _C4_YML_FILTER_PROCESSOR_HPP_
#define _C4_YML_FILTER_PROCESSOR_HPP_

#include "c4/yml/common.hpp"

#ifdef RYML_DBG
#include "c4/charconv.hpp"
#include "c4/yml/detail/dbgprint.hpp"
#endif

namespace c4 {
namespace yml {

/** @defgroup doc_filter_processors Scalar filter processors
 *
 * These are internal classes used by @ref ParseEngine to parse the
 * scalars; normally there is no reason for a user to be manually
 * using these classes.
 *
 * @ingroup doc_parse */
/** @{ */

//-----------------------------------------------------------------------------

/** Filters an input string into a different output string */
struct FilterProcessorSrcDst
{
    csubstr src;
    substr dst;
    size_t rpos; ///< read position
    size_t wpos; ///< write position

    C4_ALWAYS_INLINE FilterProcessorSrcDst(csubstr src_, substr dst_) noexcept
        : src(src_)
        , dst(dst_)
        , rpos(0)
        , wpos(0)
    {
        RYML_ASSERT(!dst.overlaps(src));
    }

    C4_ALWAYS_INLINE void setwpos(size_t wpos_) noexcept { wpos = wpos_; }
    C4_ALWAYS_INLINE void setpos(size_t rpos_, size_t wpos_) noexcept { rpos = rpos_; wpos = wpos_; }
    C4_ALWAYS_INLINE void set_at_end() noexcept { skip(src.len - rpos); }

    C4_ALWAYS_INLINE bool has_more_chars() const noexcept { return rpos < src.len; }
    C4_ALWAYS_INLINE bool has_more_chars(size_t maxpos) const noexcept { RYML_ASSERT(maxpos <= src.len); return rpos < maxpos; }

    C4_ALWAYS_INLINE csubstr rem() const noexcept { return src.sub(rpos); }
    C4_ALWAYS_INLINE csubstr sofar() const noexcept { return csubstr(dst.str, wpos <= dst.len ? wpos : dst.len); }
    C4_ALWAYS_INLINE FilterResult result() const noexcept
    {
        FilterResult ret;
        ret.str.str = wpos <= dst.len ? dst.str : nullptr;
        ret.str.len = wpos;
        return ret;
    }

    C4_ALWAYS_INLINE char curr() const noexcept { RYML_ASSERT(rpos < src.len); return src[rpos]; }
    C4_ALWAYS_INLINE char next() const noexcept { return rpos+1 < src.len ? src[rpos+1] : '\0'; }
    C4_ALWAYS_INLINE bool skipped_chars() const noexcept { return wpos != rpos; }

    C4_ALWAYS_INLINE void skip() noexcept { ++rpos; }
    C4_ALWAYS_INLINE void skip(size_t num) noexcept { rpos += num; }

    C4_ALWAYS_INLINE void set_at(size_t pos, char c) noexcept // NOLINT(readability-make-member-function-const)
    {
        RYML_ASSERT(pos < wpos);
        dst.str[pos] = c;
    }
    C4_ALWAYS_INLINE void set(char c) noexcept
    {
        if(wpos < dst.len)
            dst.str[wpos] = c;
        ++wpos;
    }
    C4_ALWAYS_INLINE void set(char c, size_t num) noexcept
    {
        RYML_ASSERT(num > 0);
        if(wpos + num <= dst.len)
            memset(dst.str + wpos, c, num);
        wpos += num;
    }

    C4_ALWAYS_INLINE void copy() noexcept
    {
        RYML_ASSERT(rpos < src.len);
        if(wpos < dst.len)
            dst.str[wpos] = src.str[rpos];
        ++wpos;
        ++rpos;
    }
    C4_ALWAYS_INLINE void copy(size_t num) noexcept
    {
        RYML_ASSERT(num);
        RYML_ASSERT(rpos+num <= src.len);
        if(wpos + num <= dst.len)
            memcpy(dst.str + wpos, src.str + rpos, num);
        wpos += num;
        rpos += num;
    }

    C4_ALWAYS_INLINE void translate_esc(char c) noexcept
    {
        if(wpos < dst.len)
            dst.str[wpos] = c;
        ++wpos;
        rpos += 2;
    }
    C4_ALWAYS_INLINE void translate_esc_bulk(const char *C4_RESTRICT s, size_t nw, size_t nr) noexcept
    {
        RYML_ASSERT(nw > 0);
        RYML_ASSERT(nr > 0);
        RYML_ASSERT(rpos+nr <= src.len);
        if(wpos+nw <= dst.len)
            memcpy(dst.str + wpos, s, nw);
        wpos += nw;
        rpos += 1 + nr;
    }
    C4_ALWAYS_INLINE void translate_esc_extending(const char *C4_RESTRICT s, size_t nw, size_t nr) noexcept
    {
        translate_esc_bulk(s, nw, nr);
    }
};


//-----------------------------------------------------------------------------
// filter in place

// debugging scaffold
/** @cond dev */
#if defined(RYML_DBG) && 0
#define _c4dbgip(...) _c4dbgpf(__VA_ARGS__)
#else
#define _c4dbgip(...)
#endif
/** @endcond */

/** Filters in place. While the result may be larger than the source,
 * any extending happens only at the end of the string. Consequently,
 * it's impossible for characters to be left unfiltered.
 *
 * @see FilterProcessorInplaceMidExtending */
struct FilterProcessorInplaceEndExtending
{
    substr src;  ///< the subject string
    size_t wcap; ///< write capacity - the capacity of the subject string's buffer
    size_t rpos; ///< read position
    size_t wpos; ///< write position

    C4_ALWAYS_INLINE FilterProcessorInplaceEndExtending(substr src_, size_t wcap_) noexcept
        : src(src_)
        , wcap(wcap_)
        , rpos(0)
        , wpos(0)
    {
        RYML_ASSERT(wcap >= src.len);
    }

    C4_ALWAYS_INLINE void setwpos(size_t wpos_) noexcept { wpos = wpos_; }
    C4_ALWAYS_INLINE void setpos(size_t rpos_, size_t wpos_) noexcept { rpos = rpos_; wpos = wpos_; }
    C4_ALWAYS_INLINE void set_at_end() noexcept { skip(src.len - rpos); }

    C4_ALWAYS_INLINE bool has_more_chars() const noexcept { return rpos < src.len; }
    C4_ALWAYS_INLINE bool has_more_chars(size_t maxpos) const noexcept { RYML_ASSERT(maxpos <= src.len); return rpos < maxpos; }

    C4_ALWAYS_INLINE FilterResult result() const noexcept
    {
        _c4dbgip("inplace: wpos={} wcap={} small={}", wpos, wcap, wpos > rpos);
        FilterResult ret;
        ret.str.str = (wpos <= wcap) ? src.str : nullptr;
        ret.str.len = wpos;
        return ret;
    }
    C4_ALWAYS_INLINE csubstr sofar() const noexcept { return csubstr(src.str, wpos <= wcap ? wpos : wcap); }
    C4_ALWAYS_INLINE csubstr rem() const noexcept { return src.sub(rpos); }

    C4_ALWAYS_INLINE char curr() const noexcept { RYML_ASSERT(rpos < src.len); return src[rpos]; }
    C4_ALWAYS_INLINE char next() const noexcept { return rpos+1 < src.len ? src[rpos+1] : '\0'; }

    C4_ALWAYS_INLINE void skip() noexcept { ++rpos; }
    C4_ALWAYS_INLINE void skip(size_t num) noexcept { rpos += num; }

    void set_at(size_t pos, char c) noexcept
    {
        RYML_ASSERT(pos < wpos);
        const size_t save = wpos;
        wpos = pos;
        set(c);
        wpos = save;
    }
    void set(char c) noexcept
    {
        if(wpos < wcap)  // respect write-capacity
            src.str[wpos] = c;
        ++wpos;
    }
    void set(char c, size_t num) noexcept
    {
        RYML_ASSERT(num);
        if(wpos + num <= wcap)  // respect write-capacity
            memset(src.str + wpos, c, num);
        wpos += num;
    }

    void copy() noexcept
    {
        RYML_ASSERT(wpos <= rpos);
        RYML_ASSERT(rpos < src.len);
        if(wpos < wcap)  // respect write-capacity
            src.str[wpos] = src.str[rpos];
        ++rpos;
        ++wpos;
    }
    void copy(size_t num) noexcept
    {
        RYML_ASSERT(num);
        RYML_ASSERT(rpos+num <= src.len);
        RYML_ASSERT(wpos <= rpos);
        if(wpos + num <= wcap)  // respect write-capacity
        {
            if(wpos + num <= rpos) // there is no overlap
                memcpy(src.str + wpos, src.str + rpos, num);
            else                   // there is overlap
                memmove(src.str + wpos, src.str + rpos, num);
        }
        rpos += num;
        wpos += num;
    }

    void translate_esc(char c) noexcept
    {
        RYML_ASSERT(rpos + 2 <= src.len);
        RYML_ASSERT(wpos <= rpos);
        if(wpos < wcap) // respect write-capacity
            src.str[wpos] = c;
        rpos += 2; // add 1u to account for the escape character
        ++wpos;
    }

    void translate_esc_bulk(const char *C4_RESTRICT s, size_t nw, size_t nr) noexcept
    {
        RYML_ASSERT(nw > 0);
        RYML_ASSERT(nr > 0);
        RYML_ASSERT(nw <= nr + 1u);
        RYML_ASSERT(rpos+nr <= src.len);
        RYML_ASSERT(wpos <= rpos);
        const size_t wpos_next = wpos + nw;
        const size_t rpos_next = rpos + nr + 1u; // add 1u to account for the escape character
        RYML_ASSERT(wpos_next <= rpos_next);
        if(wpos_next <= wcap)
            memcpy(src.str + wpos, s, nw);
        rpos = rpos_next;
        wpos = wpos_next;
    }

    C4_ALWAYS_INLINE void translate_esc_extending(const char *C4_RESTRICT s, size_t nw, size_t nr) noexcept
    {
        translate_esc_bulk(s, nw, nr);
    }
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/** Filters in place. The result may be larger than the source, and
 * extending may happen anywhere. As a result some characters may be
 * left unfiltered when there is no slack in the buffer and the
 * write-position would overlap the read-position. Consequently, it's
 * possible for characters to be left unfiltered. In YAML, this
 * happens only with double-quoted strings, and only with a small
 * number of escape sequences such as `\L` which is substituted by three
 * bytes. These escape sequences cause a call to translate_esc_extending()
 * which is the only entry point to this unfiltered situation.
 *
 * @see FilterProcessorInplaceMidExtending */
struct FilterProcessorInplaceMidExtending
{
    substr src;  ///< the subject string
    size_t wcap; ///< write capacity - the capacity of the subject string's buffer
    size_t rpos; ///< read position
    size_t wpos; ///< write position
    size_t maxcap; ///< the max capacity needed for filtering the string. This may be larger than the final string size.
    bool unfiltered_chars; ///< number of characters that were not added to wpos from lack of capacity

    C4_ALWAYS_INLINE FilterProcessorInplaceMidExtending(substr src_, size_t wcap_) noexcept
        : src(src_)
        , wcap(wcap_)
        , rpos(0)
        , wpos(0)
        , maxcap(src.len)
        , unfiltered_chars(false)
    {
        RYML_ASSERT(wcap >= src.len);
    }

    C4_ALWAYS_INLINE void setwpos(size_t wpos_) noexcept { wpos = wpos_; }
    C4_ALWAYS_INLINE void setpos(size_t rpos_, size_t wpos_) noexcept { rpos = rpos_; wpos = wpos_; }
    C4_ALWAYS_INLINE void set_at_end() noexcept { skip(src.len - rpos); }

    C4_ALWAYS_INLINE bool has_more_chars() const noexcept { return rpos < src.len; }
    C4_ALWAYS_INLINE bool has_more_chars(size_t maxpos) const noexcept { RYML_ASSERT(maxpos <= src.len); return rpos < maxpos; }

    C4_ALWAYS_INLINE FilterResultExtending result() const noexcept
    {
        _c4dbgip("inplace: wpos={} wcap={} unfiltered={} maxcap={}", this->wpos, this->wcap, this->unfiltered_chars, this->maxcap);
        FilterResultExtending ret;
        ret.str.str = (wpos <= wcap && !unfiltered_chars) ? src.str : nullptr;
        ret.str.len = wpos;
        ret.reqlen = maxcap;
        return ret;
    }
    C4_ALWAYS_INLINE csubstr sofar() const noexcept { return csubstr(src.str, wpos <= wcap ? wpos : wcap); }
    C4_ALWAYS_INLINE csubstr rem() const noexcept { return src.sub(rpos); }

    C4_ALWAYS_INLINE char curr() const noexcept { RYML_ASSERT(rpos < src.len); return src[rpos]; }
    C4_ALWAYS_INLINE char next() const noexcept { return rpos+1 < src.len ? src[rpos+1] : '\0'; }

    C4_ALWAYS_INLINE void skip() noexcept { ++rpos; }
    C4_ALWAYS_INLINE void skip(size_t num) noexcept { rpos += num; }

    void set_at(size_t pos, char c) noexcept
    {
        RYML_ASSERT(pos < wpos);
        const size_t save = wpos;
        wpos = pos;
        set(c);
        wpos = save;
    }
    void set(char c) noexcept
    {
        if(wpos < wcap)  // respect write-capacity
        {
            if((wpos <= rpos) && !unfiltered_chars)
                src.str[wpos] = c;
        }
        else
        {
            _c4dbgip("inplace: add unwritten {}->{}   maxcap={}->{}!", unfiltered_chars, true, maxcap, (wpos+1u > maxcap ? wpos+1u : maxcap));
            unfiltered_chars = true;
        }
        ++wpos;
        maxcap = wpos > maxcap ? wpos : maxcap;
    }
    void set(char c, size_t num) noexcept
    {
        RYML_ASSERT(num);
        if(wpos + num <= wcap)  // respect write-capacity
        {
            if((wpos <= rpos) && !unfiltered_chars)
                memset(src.str + wpos, c, num);
        }
        else
        {
            _c4dbgip("inplace: add unwritten {}->{}   maxcap={}->{}!", unfiltered_chars, true, maxcap, (wpos+num > maxcap ? wpos+num : maxcap));
            unfiltered_chars = true;
        }
        wpos += num;
        maxcap = wpos > maxcap ? wpos : maxcap;
    }

    void copy() noexcept
    {
        RYML_ASSERT(rpos < src.len);
        if(wpos < wcap)  // respect write-capacity
        {
            if((wpos < rpos) && !unfiltered_chars)  // write only if wpos is behind rpos
                src.str[wpos] = src.str[rpos];
        }
        else
        {
            _c4dbgip("inplace: add unwritten {}->{} (wpos={}!=rpos={})={}  (wpos={}<wcap={})   maxcap={}->{}!", unfiltered_chars, true, wpos, rpos, wpos!=rpos, wpos, wcap, wpos<wcap, maxcap, (wpos+1u > maxcap ? wpos+1u : maxcap));
            unfiltered_chars = true;
        }
        ++rpos;
        ++wpos;
        maxcap = wpos > maxcap ? wpos : maxcap;
    }
    void copy(size_t num) noexcept
    {
        RYML_ASSERT(num);
        RYML_ASSERT(rpos+num <= src.len);
        if(wpos + num <= wcap)  // respect write-capacity
        {
            if((wpos < rpos) && !unfiltered_chars)  // write only if wpos is behind rpos
            {
                if(wpos + num <= rpos) // there is no overlap
                    memcpy(src.str + wpos, src.str + rpos, num);
                else                   // there is overlap
                    memmove(src.str + wpos, src.str + rpos, num);
            }
        }
        else
        {
            _c4dbgip("inplace: add unwritten {}->{} (wpos={}!=rpos={})={}  (wpos={}<wcap={})  maxcap={}->{}!", unfiltered_chars, true, wpos, rpos, wpos!=rpos, wpos, wcap, wpos<wcap);
            unfiltered_chars = true;
        }
        rpos += num;
        wpos += num;
        maxcap = wpos > maxcap ? wpos : maxcap;
    }

    void translate_esc(char c) noexcept
    {
        RYML_ASSERT(rpos + 2 <= src.len);
        if(wpos < wcap) // respect write-capacity
        {
            if((wpos <= rpos) && !unfiltered_chars)
                src.str[wpos] = c;
        }
        else
        {
            _c4dbgip("inplace: add unfiltered {}->{}  maxcap={}->{}!", unfiltered_chars, true, maxcap, (wpos+1u > maxcap ? wpos+1u : maxcap));
            unfiltered_chars = true;
        }
        rpos += 2;
        ++wpos;
        maxcap = wpos > maxcap ? wpos : maxcap;
    }

    C4_NO_INLINE void translate_esc_bulk(const char *C4_RESTRICT s, size_t nw, size_t nr) noexcept
    {
        RYML_ASSERT(nw > 0);
        RYML_ASSERT(nr > 0);
        RYML_ASSERT(nr+1u >= nw);
        const size_t wpos_next = wpos + nw;
        const size_t rpos_next = rpos + nr + 1u; // add 1u to account for the escape character
        if(wpos_next <= wcap)  // respect write-capacity
        {
            if((wpos <= rpos) && !unfiltered_chars)  // write only if wpos is behind rpos
                memcpy(src.str + wpos, s, nw);
        }
        else
        {
            _c4dbgip("inplace: add unwritten {}->{} (wpos={}!=rpos={})={}  (wpos={}<wcap={})  maxcap={}->{}!", unfiltered_chars, true, wpos, rpos, wpos!=rpos, wpos, wcap, wpos<wcap);
            unfiltered_chars = true;
        }
        rpos = rpos_next;
        wpos = wpos_next;
        maxcap = wpos > maxcap ? wpos : maxcap;
    }

    C4_NO_INLINE void translate_esc_extending(const char *C4_RESTRICT s, size_t nw, size_t nr) noexcept
    {
        RYML_ASSERT(nw > 0);
        RYML_ASSERT(nr > 0);
        RYML_ASSERT(rpos+nr <= src.len);
        const size_t wpos_next = wpos + nw;
        const size_t rpos_next = rpos + nr + 1u; // add 1u to account for the escape character
        if(wpos_next <= rpos_next) // read and write do not overlap. just do a vanilla copy.
        {
            if((wpos_next <= wcap) && !unfiltered_chars)
                memcpy(src.str + wpos, s, nw);
            rpos = rpos_next;
            wpos = wpos_next;
            maxcap = wpos > maxcap ? wpos : maxcap;
        }
        else // there is overlap. move the (to-be-read) string to the right.
        {
            const size_t excess = wpos_next - rpos_next;
            RYML_ASSERT(wpos_next > rpos_next);
            if(src.len + excess <= wcap) // ensure we do not go past the end
            {
                RYML_ASSERT(rpos+nr+excess <= src.len);
                if(wpos_next <= wcap)
                {
                    if(!unfiltered_chars)
                    {
                        memmove(src.str + wpos_next, src.str + rpos_next, src.len - rpos_next);
                        memcpy(src.str + wpos, s, nw);
                    }
                    rpos = wpos_next; // wpos, not rpos
                }
                else
                {
                    rpos = rpos_next;
                    //const size_t unw = nw > (nr + 1u) ? nw - (nr + 1u) : 0;
                    _c4dbgip("inplace: add unfiltered {}->{}   maxcap={}->{}!", unfiltered_chars, true);
                    unfiltered_chars = true;
                }
                wpos = wpos_next;
                // extend the string up to capacity
                src.len += excess;
                maxcap = wpos > maxcap ? wpos : maxcap;
            }
            else
            {
                //const size_t unw = nw > (nr + 1u) ? nw - (nr + 1u) : 0;
                RYML_ASSERT(rpos_next <= src.len);
                const size_t required_size = wpos_next + (src.len - rpos_next);
                _c4dbgip("inplace: add unfiltered {}->{}   maxcap={}->{}!", unfiltered_chars, true, maxcap, required_size > maxcap ? required_size : maxcap);
                RYML_ASSERT(required_size > wcap);
                unfiltered_chars = true;
                maxcap = required_size > maxcap ? required_size : maxcap;
                wpos = wpos_next;
                rpos = rpos_next;
            }
        }
    }
};

#undef _c4dbgip


/** @} */

} // namespace yml
} // namespace c4

#endif /* _C4_YML_FILTER_PROCESSOR_HPP_ */
