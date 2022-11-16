#include "blocks.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define FreeTags(tag_ptr) while ((tag_ptr = tag_pop(tag_ptr)))

typedef struct Parser {
  const char *buffer;
  int cursor;
} Parser;
#define Parser(text) (Parser){.buffer = text, .cursor = 0};

#define p_peek(p)         ((p)->buffer[(p)->cursor])
#define p_buffer(p)       ((p)->buffer + (p)->cursor)
#define p_advance(p, inc) ((p)->cursor += inc)
#define p_consume(p, ch)  (p_peek(p) == ch && p_advance(p, 1) > 0)
#define p_consume_string(p, str, len)                                          \
  (memcmp(str, p_buffer(p), (len)) == 0 && p_advance(p, (len)) > 0)

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

static inline Tag *tag_clone(Tag *);
static inline Tag *tag_push(Tag *, const char *, TagModifierMask);
static inline Tag *tag_pop(Tag *);
static inline TagName parse_tagname(Parser *);
static inline TagModifierMask parse_tagmodifier(Parser *, TagName);
static inline int parse(const char *, TagName *, TagModifierMask *, char *,
                        bool *);
static inline void createblk(Block *, Tag *[NullTagName], const char *, int);

static inline Tag *tag_clone(Tag *root)
{
  if (root == NULL)
    return NULL;
  Tag *tag = (Tag *)malloc(sizeof(Tag));
  strcpy(tag->val, root->val);
  tag->tmod_mask = root->tmod_mask;
  tag->previous  = tag_clone(root->previous);
  return tag;
}

static inline Tag *tag_push(Tag *previous, const char *val,
                            TagModifierMask tmod_mask)
{
  Tag *tag = (Tag *)malloc(sizeof(Tag));
  strcpy(tag->val, val);
  tag->tmod_mask = tmod_mask;
  tag->previous  = previous;
  return tag;
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

int parse(const char *text, TagName *tag_name, TagModifierMask *tmod_mask,
          char *val, bool *closing)
{ // clang-format off
#define TRY(expr) if (!(expr)) return 0; // clang-format on
  Parser parser = Parser(text);
  *tag_name = NullTagName, *tmod_mask = 0x0, *closing = false;
  // 'sizeof' counts null termination.
  static const size_t ntag_start = sizeof(TagStart) - 1,
                      ntag_end   = sizeof(TagEnd) - 1;

  // parse tag start.
  TRY(p_consume_string(&parser, TagStart, ntag_start));
  *closing = p_consume(&parser, '/');

  // parse tag name.
  TRY((*tag_name = parse_tagname(&parser)) != NullTagName);

  if (!*closing) {
    if (p_consume(&parser, ':')) {
      // parse tag modifiers.
      int mod = NullTagModifier;
      do {
        TRY((mod = parse_tagmodifier(&parser, *tag_name)) != NullTagModifier);
        *tmod_mask |= (1 << mod);
      } while (p_consume(&parser, '|'));
    }
    TRY(p_consume(&parser, '='));
    // parse tag value.
    for (int cursor = 0; !(p_peek(&parser) == TagEnd[0]);)
      val[cursor++] = parser.buffer[parser.cursor++];
  }

  // parse tag end.
  TRY(p_consume_string(&parser, TagEnd, ntag_end));
  return parser.cursor - 1;
#undef TRY
}

static inline void createblk(Block *blk, Tag *tags[NullTagName],
                             const char *text, int ntext)
{
  blk->ntext = ntext;
  memcpy(blk->text, text, ntext);
  for (int i = 0; i < NullTagName; ++i)
    blk->tags[i] = tag_clone(tags[i]);
}

int blks_create(Block *blks, const char *text)
{
  int len = strlen(text), nblks = 0, nbuf = 0, cursor;
  bool tagclose;
  char buf[BLK_BUFFER_SIZE], val[BLK_BUFFER_SIZE];
  TagName tag_name;
  TagModifierMask tmod_mask;

  enum { Previous, Current };
  Tag *tags[2][NullTagName];
  for (TagName name = 0; name != NullTagName; ++name)
    tags[Previous][name] = tags[Current][name] = NULL;

  for (cursor = 0; cursor < len; ++cursor) {
    if (text[cursor] == TagStart[0]) {
      memset(val, 0, sizeof(val));
      int size = parse(text + cursor, &tag_name, &tmod_mask, val, &tagclose);
      // check for out of place closing tag.
      bool invalid = tagclose && tags[Current][tag_name] == NULL;
      // tag parse success check.
      if (size > 0 && tag_name != NullTagName && !invalid) {
        tags[Current][tag_name] =
            tagclose ? tag_pop(tags[Current][tag_name])
                     : tag_push(tags[Current][tag_name], val, tmod_mask);
        // only create block if there is some text in it.
        if (nbuf)
          createblk(&blks[nblks++], tags[Previous], buf, nbuf);
        // move 'cursor' forward, parsed text 'size'.
        cursor += size;
        FreeTags(tags[Previous][tag_name]);
        tags[Previous][tag_name] = tag_clone(tags[Current][tag_name]);
        memset(buf, nbuf = 0, sizeof(buf));
        continue;
      }
    }
    buf[nbuf++] = text[cursor];
  }
  if (nbuf)
    createblk(&blks[nblks++], tags[Previous], buf, nbuf);

  for (TagName name = 0; name < NullTagName; ++name) {
    FreeTags(tags[Previous][name]);
    FreeTags(tags[Current][name]);
  }
  return nblks;
}

void blks_free(Block *blks, int nblks)
{
  for (int b = 0; b < nblks; ++b)
    for (TagName tag_name = 0; tag_name < NullTagName; ++tag_name)
      FreeTags(blks[b].tags[tag_name]);
}
