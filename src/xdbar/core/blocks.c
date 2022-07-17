#include "blocks.h"
#include <stdlib.h>
#include <string.h>

#define FreeTags(tag_ptr) while ((tag_ptr = pop(tag_ptr)))

static inline Tag *mkcopy(Tag *);
static inline Tag *push(const char *, TagModifierMask, Tag *);
static inline Tag *pop(Tag *);
static inline int parsetag(const char *, TagKey *, TagModifierMask *, char *,
                           int *);
static inline void createblk(Block *, Tag *[NullKey], const char *, int);

static const char *const TagKeyRepr[NullKey] = {
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

static inline int parsetag(const char *text, TagKey *tkey,
                           TagModifierMask *tmod_mask, char *val, int *closing)
{
  int ptr = 0, nstart = strlen(TAG_START), nend = strlen(TAG_END), bufptr = 0;

  if (memcmp(text + ptr, TAG_START, nstart) != 0)
    return 0;
  ptr += nstart;
  *closing = text[ptr] == '/' && ptr++;

  // parse tag key.
  for (TagKey t = 0; t < NullKey; ++t) {
    const char *key_repr = TagKeyRepr[t];
    if (memcmp(text + ptr, key_repr, strlen(key_repr)) == 0) {
      ptr += strlen(key_repr);
      *tkey = t;
      break;
    }
  }
  if (*tkey == NullKey)
    return 0;

  if (!*closing) {
    // parsing tag modifier.
    if (text[ptr] == ':' && ptr++) {
      // atleast one modifier should be parsed after a colon (':').
      const TagModifier *mods = ValidTagModifiers[*tkey];
      do {
        int e = 0;
        for (; mods[e] != NullModifier; ++e) {
          const char *tmod_repr = TagModifierRepr[mods[e]];
          if (memcmp(text + ptr, tmod_repr, strlen(tmod_repr)) == 0) {
            ptr += strlen(tmod_repr);
            *tmod_mask |= (1 << mods[e]);
            break;
          }
        }
        if (mods[e] == NullModifier)
          return 0;
        // keep parsing modifiers as long as it ends with pipe ('|').
      } while (text[ptr] == '|' && ptr++);
    }

    if (text[ptr++] != '=')
      return 0;
    // parse tag value.
    while (text[ptr] != TAG_END[0])
      val[bufptr++] = text[ptr++];
  }

  if (memcmp(text + ptr, TAG_END, nend) != 0)
    return 0;

  return ptr + nend - 1;
}

static inline void createblk(Block *blk, Tag *tags[NullKey], const char *text,
                             int ntext)
{
  blk->ntext = ntext;
  memcpy(blk->text, text, ntext);

  for (int i = 0; i < NullKey; ++i)
    blk->tags[i] = mkcopy(tags[i]);
}

int blks_create(const char *name, Block *blks)
{
#define Cur 0
#define New 1
  int len = strlen(name), nblks = 0, nbuf = 0, ptr, tagclose;
  char buf[len], val[len];
  TagKey tkey;
  TagModifierMask tmod_mask;

  Tag *tagstate[2][NullKey];
  for (TagKey tag = 0; tag < NullKey; ++tag)
    tagstate[Cur][tag] = tagstate[New][tag] = NULL;

  for (ptr = 0; ptr < len; ++ptr) {
    if (name[ptr] == TAG_START[0]) {
      tkey = NullKey, tmod_mask = 0x0, tagclose = 0;
      memset(val, 0, sizeof(val));
      int size = parsetag(name + ptr, &tkey, &tmod_mask, val, &tagclose);
      // check for 'out of place' closing tag (closing tag without
      // corresponding opening tag).
      int oop = tagclose && tagstate[New][tkey] == NULL;
      // tag parse success check.
      if (size > 0 && tkey != NullKey && !oop) {
        tagstate[New][tkey] = tagclose
                                  ? pop(tagstate[New][tkey])
                                  : push(val, tmod_mask, tagstate[New][tkey]);
        // only creating a new block, if text is not empty as blocks with
        // empty text doesn't get rendered and that block becomes essentially
        // useless (very tiny memory/speed optimization).
        if (nbuf)
          createblk(&blks[nblks++], tagstate[Cur], buf, nbuf);

        ptr += size;
        // State Update: copy 'New' tagstate to 'Cur' tagstate.
        FreeTags(tagstate[Cur][tkey]);
        tagstate[Cur][tkey] = mkcopy(tagstate[New][tkey]);
        // reset temp value buffer and continue loop, once state is updated.
        memset(buf, nbuf = 0, sizeof(buf));
        continue;
      }
    }
    buf[nbuf++] = name[ptr];
  }

  if (nbuf)
    createblk(&blks[nblks++], tagstate[Cur], buf, nbuf);

  for (int i = 0; i < NullKey; ++i) {
    FreeTags(tagstate[Cur][i]);
    FreeTags(tagstate[New][i]);
  }
#undef Cur
#undef New
  return nblks;
}

void blks_free(Block *blks, int nblks)
{
  for (int b = 0; b < nblks; ++b)
    for (int i = 0; i < NullKey; ++i)
      FreeTags(blks[b].tags[i]);
}
