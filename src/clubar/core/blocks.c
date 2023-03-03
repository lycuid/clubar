#include "blocks.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define RemoveTag(tag_ptr) while ((tag_ptr = tag_remove(tag_ptr)))

typedef struct TagToken {
  bool closing : 1;
  char val[BLK_BUFFER_SIZE];
  TagName tag_name;
  TagModifierMask tmod_mask;
} TagToken;
#define TOKEN_CLEAR(t)                                                         \
  do {                                                                         \
    memset((t)->val, 0, sizeof((t)->val));                                     \
    (t)->tag_name = NullTagName, (t)->tmod_mask = 0x0, (t)->closing = false;   \
  } while (0)

typedef struct Parser {
  const char *buffer;
  int cursor, size;
} Parser;
#define PARSER(text, len) (Parser){.buffer = text, .size = len, .cursor = 0};

#define p_peek(p)            ((p)->cursor < (p)->size ? (p)->buffer + (p)->cursor : NULL)
#define p_next(p)            ((p)->buffer[(p)->cursor++])
#define p_buffer(p)          ((p)->buffer + (p)->cursor)
#define p_rollback_to(p, c)  ((p)->cursor = c)
#define p_advance(p)         p_advance_by(p, 1)
#define p_advance_by(p, inc) ((p)->cursor += inc)
#define p_consume(p, ch)     (p_peek(p) && *p_peek(p) == ch && p_advance(p) > 0)
#define p_consume_string(p, str, len)                                          \
  (p_peek(p) && memcmp(str, p_buffer(p), (len)) == 0 &&                        \
   p_advance_by(p, (len)) > 0)

static inline TagName parse_tagname(Parser *);
static inline TagModifierMask parse_tagmodifier(Parser *, TagName);
static inline bool parse_tag(Parser *, TagToken *);
static inline void createblk(Block *, Tag *const[NullTagName], const char *,
                             int);

static inline TagName parse_tagname(Parser *parser)
{
  for (TagName tag_name = 0; tag_name < NullTagName; ++tag_name) {
    int len = strlen(TagNameRepr[tag_name]);
    if (p_consume_string(parser, TagNameRepr[tag_name], len))
      return tag_name;
  }
  return NullTagName;
}

static inline TagModifierMask parse_tagmodifier(Parser *parser, TagName name)
{
  const TagModifier *mods = ValidTagModifiers[name];
  for (int e = 0; mods[e] != NullTagModifier; ++e) {
    int len = strlen(TagModifierRepr[mods[e]]);
    if (p_consume_string(parser, TagModifierRepr[mods[e]], len))
      return mods[e];
  }
  return NullTagModifier;
}

static inline bool parse_tag(Parser *p, TagToken *token)
{
#define TRYP(expr)                                                             \
  if (!(expr) && (p_rollback_to(p, cursor), true))                             \
    return false;
  size_t cursor = p->cursor;
  TOKEN_CLEAR(token);
  static const size_t ntag_start = sizeof(TagStart) - 1,
                      ntag_end   = sizeof(TagEnd) - 1;

  TRYP(p_consume_string(p, TagStart, ntag_start));
  token->closing = p_consume(p, '/');
  TRYP((token->tag_name = parse_tagname(p)) != NullTagName);

  if (!token->closing) {
    if (p_consume(p, ':')) {
      int mod = NullTagModifier;
      do {
        TRYP((mod = parse_tagmodifier(p, token->tag_name)) != NullTagModifier);
        token->tmod_mask |= (1 << mod);
      } while (p_consume(p, '|'));
    }
    TRYP(p_consume(p, '='));
    for (int i = 0; *p_peek(p) != TagEnd[0]; ++i)
      token->val[i] = p_next(p);
  }
  return p_consume_string(p, TagEnd, ntag_end);
#undef TRYP
}

static inline void createblk(Block *blk, Tag *const tags[NullTagName],
                             const char *text, int ntext)
{
  memcpy(blk->text, text, ntext);
  if (ntext < BLK_BUFFER_SIZE)
    blk->text[ntext] = 0;
  for (int i = 0; i < NullTagName; ++i)
    blk->tags[i] = tag_clone(tags[i]);
}

int blks_create(Block *blks, const char *line)
{
  Parser parser = PARSER(line, strlen(line));
  int nblks = 0, nbuf = 0;
  char buf[BLK_BUFFER_SIZE];
  TagToken token;
  Tag *tags[NullTagName] = {0};

  while (p_peek(&parser)) {
    bool parse_success = parse_tag(&parser, &token),
         invalid_close = token.closing && tags[token.tag_name] == NULL;
    if (parse_success && !invalid_close) {
      if (nbuf) // only create a block, if some text exits.
        createblk(&blks[nblks++], tags, buf, nbuf);
      nbuf = 0;
      tags[token.tag_name] =
          token.closing
              ? tag_remove(tags[token.tag_name])
              : tag_create(tags[token.tag_name], token.val, token.tmod_mask);
    } else {
      buf[nbuf++] = p_next(&parser);
    }
  }
  if (nbuf) // only create a block, if some text exits.
    createblk(&blks[nblks++], tags, buf, nbuf);
  for (TagName name = 0; name < NullTagName; ++name)
    RemoveTag(tags[name]);
  return nblks;
}

void blks_free(Block *blks, int nblks)
{
  for (int b = 0; b < nblks; ++b)
    for (TagName tag_name = 0; tag_name < NullTagName; ++tag_name)
      RemoveTag(blks[b].tags[tag_name]);
}
