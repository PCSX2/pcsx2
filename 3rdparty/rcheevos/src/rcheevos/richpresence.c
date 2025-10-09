#include "rc_internal.h"

#include "../rc_compat.h"

#include <ctype.h>

/* special formats only used by rc_richpresence_display_part_t.display_type. must not overlap other RC_FORMAT values */
enum {
  RC_FORMAT_STRING = 101,
  RC_FORMAT_LOOKUP = 102,
  RC_FORMAT_UNKNOWN_MACRO = 103,
  RC_FORMAT_ASCIICHAR = 104,
  RC_FORMAT_UNICODECHAR = 105
};

static void rc_alloc_helper_variable_memref_value(rc_richpresence_display_part_t* part, const char* memaddr, int memaddr_len, rc_parse_state_t* parse) {
  rc_preparse_state_t preparse;
  const char* test_memaddr = memaddr;
  rc_condset_t* condset;
  rc_value_t* value;
  int32_t size;

  part->value.type = RC_OPERAND_NONE;

  /* if the expression can be represented as just a memory reference, do so */
  rc_init_preparse_state(&preparse);
  preparse.parse.existing_memrefs = parse->memrefs;
  value = RC_ALLOC(rc_value_t, &preparse.parse);
  rc_parse_value_internal(value, &test_memaddr, &preparse.parse);

  size = preparse.parse.offset;
  if (size < 0) {
    parse->offset = size;
    rc_destroy_preparse_state(&preparse);
    return;
  }

  /* ensure new needed memrefs are allocated in the primary buffer */
  rc_preparse_copy_memrefs(parse, &preparse.memrefs);

  /* parse the value into the scratch buffer so we can look at it */
  rc_reset_parse_state(&preparse.parse, rc_buffer_alloc(&preparse.parse.scratch.buffer, (size_t)size));
  preparse.parse.memrefs = parse->memrefs;
  preparse.parse.existing_memrefs = parse->existing_memrefs;
  value = RC_ALLOC(rc_value_t, &preparse.parse);
  test_memaddr = memaddr;
  rc_parse_value_internal(value, &test_memaddr, &preparse.parse);

  condset = value->conditions;
  if (condset && !condset->next) {
    /* single value - if it's a single Measured clause (including any AddSource/AddAddress helpers), we can
     * simplify to a memref. If there are supporting clauses like MeasuredIf or ResetIf, we can't */
    if (condset->num_measured_conditions == 1 &&
        !condset->num_pause_conditions && !condset->num_reset_conditions &&
        !condset->num_other_conditions && !condset->num_hittarget_conditions) {
      rc_condition_t* condition = condset->conditions;
      for (; condition; condition = condition->next) {
        if (condition->type == RC_CONDITION_MEASURED && condition->required_hits == 0) {
          memcpy(&part->value, &condition->operand1, sizeof(condition->operand1));
          break;
        }
      }
    }
  }

  rc_destroy_preparse_state(&preparse);

  /* could not express value with just a memory reference, create a helper variable */
  if (part->value.type == RC_OPERAND_NONE) {
    value = rc_alloc_variable(memaddr, memaddr_len, parse);
    if (value) {
      part->value.value.memref = (rc_memref_t*)&value->value;
      part->value.type = RC_OPERAND_ADDRESS;
      part->value.size = RC_MEMSIZE_32_BITS;
      part->value.memref_access_type = RC_OPERAND_ADDRESS;
    }
  }
}

static const char* rc_parse_line(const char* line, const char** end, rc_parse_state_t* parse) {
  const char* nextline;
  const char* endline;

  /* get a single line */
  nextline = line;
  while (*nextline && *nextline != '\n')
    ++nextline;

  /* if a trailing comment marker (//) exists, the line stops there */
  endline = line;
  while (endline < nextline && (endline[0] != '/' || endline[1] != '/' || (endline > line && endline[-1] == '\\')))
    ++endline;

  if (endline == nextline) {
    /* trailing whitespace on a line without a comment marker may be significant, just remove the line ending */
    if (endline > line && endline[-1] == '\r')
      --endline;
  } else {
    /* remove trailing whitespace before the comment marker */
    while (endline > line && isspace((int)((unsigned char*)endline)[-1]))
      --endline;
  }

  /* point end at the first character to ignore, it makes subtraction for length easier */
  *end = endline;

  /* tally the line */
  ++parse->lines_read;

  /* skip the newline character so we're pointing at the next line */
  if (*nextline == '\n')
    ++nextline;

  return nextline;
}

typedef struct rc_richpresence_builtin_macro_t {
  const char* name;
  size_t name_len;
  uint8_t display_type;
} rc_richpresence_builtin_macro_t;

static rc_richpresence_display_t* rc_parse_richpresence_display_internal(const char* line, const char* endline, rc_parse_state_t* parse, rc_richpresence_lookup_t* first_lookup) {
  rc_richpresence_display_t* self;
  rc_richpresence_display_part_t* part;
  rc_richpresence_display_part_t** next;
  rc_richpresence_lookup_t* lookup;
  const char* ptr;
  const char* in;
  char* out;

  if (endline - line < 1) {
    parse->offset = RC_MISSING_DISPLAY_STRING;
    return 0;
  }

  {
    self = RC_ALLOC(rc_richpresence_display_t, parse);
    memset(self, 0, sizeof(rc_richpresence_display_t));
    next = &self->display;
  }

  /* break the string up on macros: text @macro() moretext */
  do {
    ptr = line;
    while (ptr < endline) {
      if (*ptr == '@' && (ptr == line || ptr[-1] != '\\')) /* ignore escaped @s */
        break;

      ++ptr;
    }

    if (ptr > line) {
      part = RC_ALLOC(rc_richpresence_display_part_t, parse);
      memset(part, 0, sizeof(rc_richpresence_display_part_t));
      *next = part;
      next = &part->next;

      /* handle string part */
      part->display_type = RC_FORMAT_STRING;
      part->text = rc_alloc_str(parse, line, (int)(ptr - line));
      if (part->text) {
        /* remove backslashes used for escaping */
        in = part->text;
        while (*in && *in != '\\')
          ++in;

        if (*in == '\\') {
          out = (char*)in++;
          while (*in) {
            *out++ = *in++;
            if (*in == '\\')
              ++in;
          }
          *out = '\0';
        }
      }
    }

    if (*ptr == '@') {
      /* handle macro part */
      size_t macro_len;

      line = ++ptr;
      while (ptr < endline && *ptr != '(')
        ++ptr;

      if (ptr == endline) {
        parse->offset = RC_MISSING_VALUE;
        return 0;
      }

      macro_len = ptr - line;

      part = RC_ALLOC(rc_richpresence_display_part_t, parse);
      memset(part, 0, sizeof(rc_richpresence_display_part_t));
      *next = part;
      next = &part->next;

      part->display_type = RC_FORMAT_UNKNOWN_MACRO;

      /* find the lookup and hook it up */
      lookup = first_lookup;
      while (lookup) {
        if (strncmp(lookup->name, line, macro_len) == 0 && lookup->name[macro_len] == '\0') {
          part->text = lookup->name;
          part->lookup = lookup;
          part->display_type = lookup->format;
          break;
        }

        lookup = lookup->next;
      }

      if (!lookup) {
        static const rc_richpresence_builtin_macro_t builtin_macros[] = {
          {"Number", 6, RC_FORMAT_VALUE},
          {"Score", 5, RC_FORMAT_SCORE},
          {"Centiseconds", 12, RC_FORMAT_CENTISECS},
          {"Seconds", 7, RC_FORMAT_SECONDS},
          {"Minutes", 7, RC_FORMAT_MINUTES},
          {"SecondsAsMinutes", 16, RC_FORMAT_SECONDS_AS_MINUTES},
          {"ASCIIChar", 9, RC_FORMAT_ASCIICHAR},
          {"UnicodeChar", 11, RC_FORMAT_UNICODECHAR},
          {"Float1", 6, RC_FORMAT_FLOAT1},
          {"Float2", 6, RC_FORMAT_FLOAT2},
          {"Float3", 6, RC_FORMAT_FLOAT3},
          {"Float4", 6, RC_FORMAT_FLOAT4},
          {"Float5", 6, RC_FORMAT_FLOAT5},
          {"Float6", 6, RC_FORMAT_FLOAT6},
          {"Fixed1", 6, RC_FORMAT_FIXED1},
          {"Fixed2", 6, RC_FORMAT_FIXED2},
          {"Fixed3", 6, RC_FORMAT_FIXED3},
          {"Unsigned", 8, RC_FORMAT_UNSIGNED_VALUE},
          {"Unformatted", 11, RC_FORMAT_UNFORMATTED}
        };
        size_t i;

        for (i = 0; i < sizeof(builtin_macros) / sizeof(builtin_macros[0]); ++i) {
          if (macro_len == builtin_macros[i].name_len &&
              memcmp(builtin_macros[i].name, line, builtin_macros[i].name_len) == 0) {
            part->text = builtin_macros[i].name;
            part->lookup = NULL;
            part->display_type = builtin_macros[i].display_type;
            break;
          }
        }
      }

      /* find the closing parenthesis */
      in = line;
      line = ++ptr;
      while (ptr < endline && *ptr != ')')
        ++ptr;

      if (*ptr != ')') {
        /* non-terminated macro, dump the macro and the remaining portion of the line */
        --in; /* already skipped over @ */
        part->display_type = RC_FORMAT_STRING;
        part->text = rc_alloc_str(parse, in, (int)(ptr - in));
      }
      else if (part->display_type != RC_FORMAT_UNKNOWN_MACRO) {
        rc_alloc_helper_variable_memref_value(part, line, (int)(ptr - line), parse);
        if (parse->offset < 0)
          return 0;

        ++ptr;
      }
      else {
        /* assert: the allocated string is going to be smaller than the memory used for the parameter of the macro */
        ++ptr;
        part->text = rc_alloc_str(parse, in, (int)(ptr - in));
      }
    }

    line = ptr;
  } while (line < endline);

  *next = 0;

  return self;
}

static int rc_richpresence_lookup_item_count(rc_richpresence_lookup_item_t* item)
{
  if (item == NULL)
    return 0;

  return (rc_richpresence_lookup_item_count(item->left) + rc_richpresence_lookup_item_count(item->right) + 1);
}

static void rc_rebalance_richpresence_lookup_get_items(rc_richpresence_lookup_item_t* root,
    rc_richpresence_lookup_item_t** items, int* index)
{
  if (root->left != NULL)
    rc_rebalance_richpresence_lookup_get_items(root->left, items, index);

  items[*index] = root;
  ++(*index);

  if (root->right != NULL)
    rc_rebalance_richpresence_lookup_get_items(root->right, items, index);
}

static void rc_rebalance_richpresence_lookup_rebuild(rc_richpresence_lookup_item_t** root,
    rc_richpresence_lookup_item_t** items, int first, int last)
{
  int mid = (first + last) / 2;
  rc_richpresence_lookup_item_t* item = items[mid];
  *root = item;

  if (mid == first)
    item->left = NULL;
  else
    rc_rebalance_richpresence_lookup_rebuild(&item->left, items, first, mid - 1);

  if (mid == last)
    item->right = NULL;
  else
    rc_rebalance_richpresence_lookup_rebuild(&item->right, items, mid + 1, last);
}

static void rc_rebalance_richpresence_lookup(rc_richpresence_lookup_item_t** root, rc_parse_state_t* parse)
{
  rc_richpresence_lookup_item_t** items;
  int index;
  int size;

  /* don't bother rebalancing one or two items */
  int count = rc_richpresence_lookup_item_count(*root);
  if (count < 3)
    return;

  /* allocate space for the flattened list in scratch memory */
  size = count * sizeof(rc_richpresence_lookup_item_t*);
  items = (rc_richpresence_lookup_item_t**)rc_buffer_alloc(&parse->scratch.buffer, size);

  /* if allocation fails, we can still use the unbalanced tree, so just bail out */
  if (items == NULL)
    return;

  /* flatten the list */
  index = 0;
  rc_rebalance_richpresence_lookup_get_items(*root, items, &index);

  /* and rebuild it as a balanced tree */
  rc_rebalance_richpresence_lookup_rebuild(root, items, 0, count - 1);
}

static void rc_insert_richpresence_lookup_item(rc_richpresence_lookup_t* lookup,
    uint32_t first, uint32_t last, const char* label, size_t label_len, rc_parse_state_t* parse)
{
  rc_richpresence_lookup_item_t** next;
  rc_richpresence_lookup_item_t* item;

  next = &lookup->root;
  while ((item = *next) != NULL) {
    if (first > item->last) {
      if (first == item->last + 1 &&
          strncmp(label, item->label, label_len) == 0 && item->label[label_len] == '\0') {
        item->last = last;
        return;
      }

      next = &item->right;
    }
    else if (last < item->first) {
      if (last == item->first - 1 &&
          strncmp(label, item->label, label_len) == 0 && item->label[label_len] == '\0') {
        item->first = first;
        return;
      }

      next = &item->left;
    }
    else {
      parse->offset = RC_DUPLICATED_VALUE;
      return;
    }
  }

  item = RC_ALLOC_SCRATCH(rc_richpresence_lookup_item_t, parse);
  item->first = first;
  item->last = last;
  item->label = rc_alloc_str(parse, label, label_len);
  item->left = item->right = NULL;

  *next = item;
}

static const char* rc_parse_richpresence_lookup(rc_richpresence_lookup_t* lookup, const char* nextline, rc_parse_state_t* parse)
{
  const char* line;
  const char* endline;
  const char* label;
  char* endptr = 0;
  uint32_t first, last;
  int base;

  do
  {
    line = nextline;
    if (line == NULL)
      break;

    nextline = rc_parse_line(line, &endline, parse);

    if (endline - line < 2) {
      /* ignore full line comments inside a lookup */
      if (line[0] == '/' && line[1] == '/')
        continue;

      /* empty line indicates end of lookup */
      if (lookup->root)
        rc_rebalance_richpresence_lookup(&lookup->root, parse);
      break;
    }

    /* "*=XXX" specifies default label if lookup does not provide a mapping for the value */
    if (line[0] == '*' && line[1] == '=') {
      line += 2;
      lookup->default_label = rc_alloc_str(parse, line, (int)(endline - line));
      continue;
    }

    label = line;
    while (label < endline && *label != '=')
      ++label;

    if (label == endline) {
      parse->offset = RC_MISSING_VALUE;
      break;
    }
    ++label;

    do {
      /* get the value for the mapping */
      if (line[0] == '0' && line[1] == 'x') {
        line += 2;
        base = 16;
      } else {
        base = 10;
      }

      first = (unsigned)strtoul(line, &endptr, base);

      /* check for a range */
      if (*endptr != '-') {
        /* no range, just set last to first */
        last = first;
      }
      else {
        /* range, get last value */
        line = endptr + 1;

        if (line[0] == '0' && line[1] == 'x') {
          line += 2;
          base = 16;
        } else {
          base = 10;
        }

        last = (unsigned)strtoul(line, &endptr, base);
      }

      /* ignore spaces after the number - was previously ignored as string was split on equals */
      while (*endptr == ' ')
        ++endptr;

      /* if we've found the equal sign, this is the last item */
      if (*endptr == '=') {
        rc_insert_richpresence_lookup_item(lookup, first, last, label, (int)(endline - label), parse);
        break;
      }

      /* otherwise, if it's not a comma, it's an error */
      if (*endptr != ',') {
        parse->offset = RC_INVALID_CONST_OPERAND;
        break;
      }

      /* insert the current item and continue scanning the next one */
      rc_insert_richpresence_lookup_item(lookup, first, last, label, (int)(endline - label), parse);
      if (parse->offset < 0)
        break;

      line = endptr + 1;
    } while (line < endline);

  } while (parse->offset > 0);

  return nextline;
}

void rc_parse_richpresence_internal(rc_richpresence_t* self, const char* script, rc_parse_state_t* parse) {
  rc_richpresence_display_t** nextdisplay;
  rc_richpresence_lookup_t* firstlookup = NULL;
  rc_richpresence_lookup_t** nextlookup = &firstlookup;
  rc_richpresence_lookup_t* lookup;
  rc_trigger_t* trigger;
  char format[64];
  const char* display = 0;
  const char* line;
  const char* nextline;
  const char* endline;
  const char* ptr;
  int hasdisplay = 0;
  int display_line = 0;
  int chars;

  self->values = NULL;

  /* special case for empty script to return 1 line read */
  if (!*script) {
     parse->lines_read = 1;
     parse->offset = RC_MISSING_DISPLAY_STRING;
     return;
  }

  /* first pass: process macro initializers */
  line = script;
  while (*line) {
    nextline = rc_parse_line(line, &endline, parse);
    if (strncmp(line, "Lookup:", 7) == 0) {
      line += 7;

      lookup = RC_ALLOC_SCRATCH(rc_richpresence_lookup_t, parse);
      lookup->name = rc_alloc_str(parse, line, (int)(endline - line));
      lookup->format = RC_FORMAT_LOOKUP;
      lookup->root = NULL;
      lookup->default_label = "";
      *nextlookup = lookup;
      nextlookup = &lookup->next;

      nextline = rc_parse_richpresence_lookup(lookup, nextline, parse);
      if (parse->offset < 0)
        return;

    } else if (strncmp(line, "Format:", 7) == 0) {
      line += 7;
      if (endline - line == 11 && memcmp(line, "Unformatted", 11) == 0) {
        /* for backwards compatibility with the comma rollout, we allow old scripts
         * to define an Unformatted type mapped to VALUE, and new versions will ignore
         * the definition and use the built-in macro. skip the next line (FormatType=) */
        line = rc_parse_line(nextline, &endline, parse);
        continue;
      }

      lookup = RC_ALLOC_SCRATCH(rc_richpresence_lookup_t, parse);
      lookup->name = rc_alloc_str(parse, line, (int)(endline - line));
      lookup->root = NULL;
      lookup->default_label = "";
      *nextlookup = lookup;
      nextlookup = &lookup->next;

      line = nextline;
      nextline = rc_parse_line(line, &endline, parse);
      if (parse->buffer && strncmp(line, "FormatType=", 11) == 0) {
        line += 11;

        chars = (int)(endline - line);
        if (chars > 63)
          chars = 63;
        memcpy(format, line, chars);
        format[chars] = '\0';

        lookup->format = (uint8_t)rc_parse_format(format);
      } else {
        lookup->format = RC_FORMAT_VALUE;
      }
    } else if (strncmp(line, "Display:", 8) == 0) {
      display = nextline;
      display_line = parse->lines_read;

      /* scan as long as we find conditional lines or full line comments */
      do {
        line = nextline;
        nextline = rc_parse_line(line, &endline, parse);
      } while (*line == '?' || (line[0] == '/' && line[1] == '/'));
    }

    line = nextline;
  }

  *nextlookup = 0;
  self->first_lookup = firstlookup;

  nextdisplay = &self->first_display;

  /* second pass, process display string*/
  if (display) {
    /* point the parser back at the display strings */
    int lines_read = parse->lines_read;
    parse->lines_read = display_line;
    line = display;

    nextline = rc_parse_line(line, &endline, parse);

    do {
      if (line[0] == '?') {
        /* conditional display: ?trigger?string */
        ptr = ++line;
        while (ptr < endline && *ptr != '?')
          ++ptr;

        if (ptr < endline) {
          *nextdisplay = rc_parse_richpresence_display_internal(ptr + 1, endline, parse, firstlookup);
          if (parse->offset < 0)
            return;

          trigger = &((*nextdisplay)->trigger);
          rc_parse_trigger_internal(trigger, &line, parse);
          if (parse->offset < 0)
            return;

          if (line != ptr) {
            /* incomplete read */
            parse->offset = RC_INVALID_OPERATOR;
            return;
          }

          (*nextdisplay)->has_required_hits = parse->has_required_hits;

          if (parse->buffer)
            nextdisplay = &((*nextdisplay)->next);
        }
      }
      else if (line[0] != '/' || line[1] != '/') {
        break;
      }

      line = nextline;
      nextline = rc_parse_line(line, &endline, parse);
    } while (1);

    /* non-conditional display: string */
    *nextdisplay = rc_parse_richpresence_display_internal(line, endline, parse, firstlookup);
    if (*nextdisplay) {
      hasdisplay = 1;
      nextdisplay = &((*nextdisplay)->next);

      /* restore the parser state */
      parse->lines_read = lines_read;
    }
    else {
      /* this should only happen if the line is blank.
       * expect parse->offset to be RC_MISSING_DISPLAY_STRING and leave parse->lines_read
       * on the current line for error tracking. */
    }
  }

  /* finalize */
  *nextdisplay = 0;
  self->has_memrefs = 0;

  if (!hasdisplay && parse->offset > 0) {
    parse->offset = RC_MISSING_DISPLAY_STRING;
  }
}

int rc_richpresence_size_lines(const char* script, int* lines_read) {
  rc_richpresence_with_memrefs_t* richpresence;
  rc_preparse_state_t preparse;
  rc_init_preparse_state(&preparse);

  richpresence = RC_ALLOC(rc_richpresence_with_memrefs_t, &preparse.parse);
  preparse.parse.variables = &richpresence->richpresence.values;
  rc_parse_richpresence_internal(&richpresence->richpresence, script, &preparse.parse);
  rc_preparse_alloc_memrefs(NULL, &preparse);

  if (lines_read)
    *lines_read = preparse.parse.lines_read;

  rc_destroy_preparse_state(&preparse);
  return preparse.parse.offset;
}

int rc_richpresence_size(const char* script) {
  return rc_richpresence_size_lines(script, NULL);
}

rc_richpresence_t* rc_parse_richpresence(void* buffer, const char* script, void* unused_L, int unused_funcs_idx) {
  rc_richpresence_with_memrefs_t* richpresence;
  rc_preparse_state_t preparse;

  (void)unused_L;
  (void)unused_funcs_idx;

  if (!buffer || !script)
    return NULL;

  rc_init_preparse_state(&preparse);
  richpresence = RC_ALLOC(rc_richpresence_with_memrefs_t, &preparse.parse);
  preparse.parse.variables = &richpresence->richpresence.values;
  rc_parse_richpresence_internal(&richpresence->richpresence, script, &preparse.parse);

  rc_reset_parse_state(&preparse.parse, buffer);
  richpresence = RC_ALLOC(rc_richpresence_with_memrefs_t, &preparse.parse);
  preparse.parse.variables = &richpresence->richpresence.values;
  rc_preparse_alloc_memrefs(&richpresence->memrefs, &preparse);

  rc_parse_richpresence_internal(&richpresence->richpresence, script, &preparse.parse);
  richpresence->richpresence.has_memrefs = 1;

  rc_destroy_preparse_state(&preparse);
  return (preparse.parse.offset >= 0) ? &richpresence->richpresence : NULL;
}

static void rc_update_richpresence_memrefs(rc_richpresence_t* self, rc_peek_t peek, void* ud) {
  if (self->has_memrefs) {
    rc_richpresence_with_memrefs_t* richpresence = (rc_richpresence_with_memrefs_t*)self;
    rc_update_memref_values(&richpresence->memrefs, peek, ud);
  }
}

rc_memrefs_t* rc_richpresence_get_memrefs(rc_richpresence_t* self) {
  if (self->has_memrefs) {
    rc_richpresence_with_memrefs_t* richpresence = (rc_richpresence_with_memrefs_t*)self;
    return &richpresence->memrefs;
  }

  return NULL;
}

void rc_update_richpresence(rc_richpresence_t* richpresence, rc_peek_t peek, void* peek_ud, void* unused_L) {
  (void)unused_L;

  rc_update_richpresence_memrefs(richpresence, peek, peek_ud);
  rc_update_values(richpresence->values, peek, peek_ud);
  rc_update_richpresence_internal(richpresence, peek, peek_ud);
}

void rc_update_richpresence_internal(rc_richpresence_t* richpresence, rc_peek_t peek, void* peek_ud) {
  rc_richpresence_display_t* display;

  for (display = richpresence->first_display; display; display = display->next) {
    if (display->has_required_hits)
      rc_test_trigger(&display->trigger, peek, peek_ud, NULL);
  }
}

static int rc_evaluate_richpresence_display(rc_richpresence_display_part_t* part, char* buffer, size_t buffersize)
{
  rc_richpresence_lookup_item_t* item;
  rc_typed_value_t value;
  char tmp[256];
  char* ptr = buffer;
  const char* text;
  size_t chars;

  *ptr = '\0';
  while (part) {
    switch (part->display_type) {
      case RC_FORMAT_STRING:
        text = part->text;
        chars = strlen(text);
        break;

      case RC_FORMAT_LOOKUP:
        rc_evaluate_operand(&value, &part->value, NULL);
        rc_typed_value_convert(&value, RC_VALUE_TYPE_UNSIGNED);

        text = part->lookup->default_label;
        item = part->lookup->root;
        while (item) {
          if (value.value.u32 > item->last) {
            item = item->right;
          }
          else if (value.value.u32 < item->first) {
            item = item->left;
          }
          else {
            text = item->label;
            break;
          }
        }

        chars = strlen(text);
        break;

      case RC_FORMAT_ASCIICHAR:
        chars = 0;
        text = tmp;
        value.type = RC_VALUE_TYPE_UNSIGNED;

        do {
          rc_evaluate_operand(&value, &part->value, NULL);
          if (value.value.u32 == 0) {
            /* null terminator - skip over remaining character macros */
            while (part->next && part->next->display_type == RC_FORMAT_ASCIICHAR)
              part = part->next;
            break;
          }

          if (value.value.u32 < 32 || value.value.u32 >= 127)
            value.value.u32 = '?';

          tmp[chars++] = (char)value.value.u32;
          if (chars == sizeof(tmp) || !part->next || part->next->display_type != RC_FORMAT_ASCIICHAR)
            break;

          part = part->next;
        } while (1);

        tmp[chars] = '\0';
        break;

      case RC_FORMAT_UNICODECHAR:
        chars = 0;
        text = tmp;
        value.type = RC_VALUE_TYPE_UNSIGNED;

        do {
          rc_evaluate_operand(&value, &part->value, NULL);
          if (value.value.u32 == 0) {
            /* null terminator - skip over remaining character macros */
            while (part->next && part->next->display_type == RC_FORMAT_UNICODECHAR)
              part = part->next;
            break;
          }

          if (value.value.u32 < 32 || value.value.u32 > 65535)
            value.value.u32 = 0xFFFD; /* unicode replacement char */

          if (value.value.u32 < 0x80) {
            tmp[chars++] = (char)value.value.u32;
          }
          else if (value.value.u32 < 0x0800) {
            tmp[chars + 1] = (char)(0x80 | (value.value.u32 & 0x3F)); value.value.u32 >>= 6;
            tmp[chars] = (char)(0xC0 | (value.value.u32 & 0x1F));
            chars += 2;
          }
          else {
            /* surrogate pair not supported, convert to replacement char */
            if (value.value.u32 >= 0xD800 && value.value.u32 < 0xE000)
              value.value.u32 = 0xFFFD;

            tmp[chars + 2] = (char)(0x80 | (value.value.u32 & 0x3F)); value.value.u32 >>= 6;
            tmp[chars + 1] = (char)(0x80 | (value.value.u32 & 0x3F)); value.value.u32 >>= 6;
            tmp[chars] = (char)(0xE0 | (value.value.u32 & 0x1F));
            chars += 3;
          }

          if (chars >= sizeof(tmp) - 3 || !part->next || part->next->display_type != RC_FORMAT_UNICODECHAR)
            break;

          part = part->next;
        } while (1);

        tmp[chars] = '\0';
        break;

      case RC_FORMAT_UNKNOWN_MACRO:
        chars = snprintf(tmp, sizeof(tmp), "[Unknown macro]%s", part->text);
        text = tmp;
        break;

      default:
        rc_evaluate_operand(&value, &part->value, NULL);
        chars = rc_format_typed_value(tmp, sizeof(tmp), &value, part->display_type);
        text = tmp;
        break;
    }

    if (chars > 0 && buffersize > 0) {
      if ((unsigned)chars >= buffersize) {
        /* prevent write past end of buffer */
        memcpy(ptr, text, buffersize - 1);
        ptr[buffersize - 1] = '\0';
        buffersize = 0;
      }
      else {
        memcpy(ptr, text, chars);
        ptr[chars] = '\0';
        buffersize -= (unsigned)chars;
      }
    }

    ptr += chars;
    part = part->next;
  }

  return (int)(ptr - buffer);
}

int rc_get_richpresence_display_string(rc_richpresence_t* richpresence, char* buffer, size_t buffersize, rc_peek_t peek, void* peek_ud, void* unused_L) {
  rc_richpresence_display_t* display;

  for (display = richpresence->first_display; display; display = display->next) {
    /* if we've reached the end of the condition list, process it */
    if (!display->next)
      return rc_evaluate_richpresence_display(display->display, buffer, buffersize);

    /* triggers with required hits will be updated in rc_update_richpresence */
    if (!display->has_required_hits)
      rc_test_trigger(&display->trigger, peek, peek_ud, unused_L);

    /* if we've found a valid condition, process it */
    if (display->trigger.state == RC_TRIGGER_STATE_TRIGGERED)
      return rc_evaluate_richpresence_display(display->display, buffer, buffersize);
  }

  buffer[0] = '\0';
  return 0;
}

int rc_evaluate_richpresence(rc_richpresence_t* richpresence, char* buffer, size_t buffersize, rc_peek_t peek, void* peek_ud, void* unused_L) {
  rc_update_richpresence(richpresence, peek, peek_ud, unused_L);
  return rc_get_richpresence_display_string(richpresence, buffer, buffersize, peek, peek_ud, unused_L);
}

void rc_reset_richpresence_triggers(rc_richpresence_t* self) {
  rc_richpresence_display_t* display;

  for (display = self->first_display; display; display = display->next)
    rc_reset_trigger(&display->trigger);
}

void rc_reset_richpresence(rc_richpresence_t* self) {
  rc_reset_richpresence_triggers(self);
  rc_reset_values(self->values);
}
