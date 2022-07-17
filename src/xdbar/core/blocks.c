#include "blocks.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define FreeTags(tag_ptr) while ((tag_ptr = pop(tag_ptr)))

static inline Tag *mkcopy(Tag *);
static inline Tag *push(const char *, TagModifierMask, Tag *);
static inline Tag *pop(Tag *);
static inline int parsetag(const char *, TagName *, TagModifierMask *, char *,
                           bool *);
static inline void createblk(Block *, Tag *[NullTagName], const char *, int);

static const char *const TagNameRepr[NullTagName] = {
    [Fn] = "Fn",     [Fg] = "Fg",       [Bg] = "Bg",
    [Box] = "Box",   [BtnL] = "BtnL",   [BtnM] = "BtnM",
    [BtnR] = "BtnR", [ScrlU] = "ScrlU", [ScrlD] = "ScrlD"};

static const char *const TagModifierRepr[NullModifier] = {
    [Shift] = "Shift", [Ctrl] = "Ctrl",    [Super] = "Super",
    [Alt] = "Alt",     [Left] = "Left",    [Right] = "Right",
    [Top] = "Top",     [Bottom] = "Bottom"};

static inline Tag *mkcopy(Tag *root)
{
  if (root == NULL)
    return NULL;
  Tag *tag = (Tag *)malloc(sizeof(Tag));
  strcpy(tag->val, root->val);
  tag->tmod_mask = root->tmod_mask;
  tag->previous  = mkcopy(root->previous);
  return tag;
}

static inline Tag *push(const char *val, TagModifierMask tmod_mask,
                        Tag *previous)
{
  Tag *tag = (Tag *)malloc(sizeof(Tag));
  strcpy(tag->val, val);
  tag->tmod_mask = tmod_mask;
  tag->previous  = previous;
  return tag;
}

static inline Tag *pop(Tag *stale)
{
  if (stale == NULL)
    return NULL;
  Tag *tag = stale->previous;
  free(stale);
  return tag;
}

static inline int parsetag(const char *text, TagName *tag_name,
                           TagModifierMask *tmod_mask, char *val, bool *closing)
{
  *tag_name = NullTagName, tmod_mask = 0x0, *closing = false;
  memset(val, 0, sizeof(*val));
  int cursor = 0, nstart = strlen(TAG_START), nend = strlen(TAG_END),
      bufcursor = 0;

  if (memcmp(text + cursor, TAG_START, nstart) != 0)
    return 0;
  cursor += nstart;
  *closing = text[cursor] == '/' && cursor++;

  for (TagName t = 0; t < NullTagName; ++t) {
    const char *name_repr = TagNameRepr[t];
    if (memcmp(text + cursor, name_repr, strlen(name_repr)) == 0) {
      cursor += strlen(name_repr);
      *tag_name = t;
      break;
    }
  }
  if (*tag_name == NullTagName)
    return 0;

  if (!*closing) {
    if (text[cursor] == ':' && cursor++) {
      const TagModifier *mods = ValidTagModifiers[*tag_name];
      do {
        int e = 0;
        for (; mods[e] != NullModifier; ++e) {
          const char *tmod_repr = TagModifierRepr[mods[e]];
          if (memcmp(text + cursor, tmod_repr, strlen(tmod_repr)) == 0) {
            cursor += strlen(tmod_repr);
            *tmod_mask |= (1 << mods[e]);
            break;
          }
        }
        if (mods[e] == NullModifier)
          return 0;
      } while (text[cursor] == '|' && cursor++);
    }
    if (text[cursor++] != '=')
      return 0;
    while (text[cursor] != TAG_END[0])
      val[bufcursor++] = text[cursor++];
  }

  if (memcmp(text + cursor, TAG_END, nend) != 0)
    return 0;

  return cursor + nend - 1;
}

static inline void createblk(Block *blk, Tag *tags[NullTagName],
                             const char *text, int ntext)
{
  blk->ntext = ntext;
  memcpy(blk->text, text, ntext);

  for (int i = 0; i < NullTagName; ++i)
    blk->tags[i] = mkcopy(tags[i]);
}

int blks_create(const char *name, Block *blks)
{
  int len = strlen(name), nblks = 0, nbuf = 0, cursor;
  bool tagclose;
  char buf[len], val[len];
  TagName tag_name;
  TagModifierMask tmod_mask;

  enum { Previous, Current };
  Tag *tags[2][NullTagName];
  for (TagName name = 0; name != NullTagName; ++name)
    tags[Previous][name] = tags[Current][name] = NULL;

  for (cursor = 0; cursor < len; ++cursor) {
    if (name[cursor] == TAG_START[0]) {
      int size = parsetag(name + cursor, &tag_name, &tmod_mask, val, &tagclose);
      // check for out of place closing tag (closing tag without corresponding
      // opening tag).
      bool invalid = tagclose && tags[Current][tag_name] == NULL;
      // tag parse success check.
      if (size > 0 && tag_name != NullTagName && !invalid) {
        tags[Current][tag_name] =
            tagclose ? pop(tags[Current][tag_name])
                     : push(val, tmod_mask, tags[Current][tag_name]);
        if (nbuf)
          createblk(&blks[nblks++], tags[Previous], buf, nbuf);

        cursor += size;
        FreeTags(tags[Previous][tag_name]);
        tags[Previous][tag_name] = mkcopy(tags[Current][tag_name]);
        memset(buf, nbuf = 0, sizeof(buf));
        continue;
      }
    }
    buf[nbuf++] = name[cursor];
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
