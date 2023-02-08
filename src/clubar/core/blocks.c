#include "blocks.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define FreeTag(tag_ptr) while ((tag_ptr = tag_pop(tag_ptr)))

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

#define REPR(token) [token] = #token
static const char *const TagNameRepr[NullTagName] = {
    REPR(Fn),   REPR(Fg),   REPR(Bg),    REPR(Box),   REPR(BtnL),
    REPR(BtnM), REPR(BtnR), REPR(ScrlU), REPR(ScrlD),
};
static const char *const TagModifierRepr[NullTagModifier] = {
    REPR(Shift), REPR(Ctrl),  REPR(Super), REPR(Alt),
    REPR(Left),  REPR(Right), REPR(Top),   REPR(Bottom),
};
#undef REPR

static inline Tag *tag_new(Tag *, const char *, TagModifierMask);
static inline Tag *tag_clone(const Tag *);
static inline Tag *tag_pop(Tag *);
static inline TagName parse_tagname(Parser *);
static inline TagModifierMask parse_tagmodifier(Parser *, TagName);
static inline bool parse_tag(Parser *, TagToken *);
static inline void createblk(Block *, Tag *const[NullTagName], const char *,
                             int);

static inline Tag *tag_new(Tag *previous, const char *val,
                           TagModifierMask tmod_mask)
{
  Tag *tag = (Tag *)malloc(sizeof(Tag));
  strcpy(tag->val, val);
  tag->tmod_mask = tmod_mask;
  tag->previous  = previous;
  return tag;
}

static inline Tag *tag_clone(const Tag *root)
{
  if (!root)
    return NULL;
  return tag_new(tag_clone(root->previous), root->val, root->tmod_mask);
}

static inline Tag *tag_pop(Tag *stale)
{
  if (stale == NULL)
    return NULL;
  Tag *tag = stale->previous;
  free(stale);
  return tag;
}

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
  if (!(expr))                                                                 \
    return false;

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

  for (int c = parser.cursor; p_peek(&parser); c = parser.cursor) {
    bool parse_success = parse_tag(&parser, &token),
         invalid_close = token.closing && tags[token.tag_name] == NULL;
    if (parse_success && !invalid_close) {
      if (nbuf) // only create a block, if some text exits.
        createblk(&blks[nblks++], tags, buf, nbuf);
      nbuf = 0;
      tags[token.tag_name] =
          token.closing
              ? tag_pop(tags[token.tag_name])
              : tag_new(tags[token.tag_name], token.val, token.tmod_mask);
    } else {
      p_rollback_to(&parser, c);
      buf[nbuf++] = p_next(&parser);
    }
  }
  if (nbuf)
    createblk(&blks[nblks++], tags, buf, nbuf);
  for (TagName name = 0; name < NullTagName; ++name)
    FreeTag(tags[name]);
  return nblks;
}

void blks_free(Block *blks, int nblks)
{
  for (int b = 0; b < nblks; ++b)
    for (TagName tag_name = 0; tag_name < NullTagName; ++tag_name)
      FreeTag(blks[b].tags[tag_name]);
}
