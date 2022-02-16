#include "blocks.h"
#include <stdlib.h>
#include <string.h>

#define LOOP(cond) while ((cond))
enum { Cur, New };

Tag *mkcopy(Tag *root) {
  if (root == NULL)
    return NULL;
  Tag *tag = (Tag *)malloc(sizeof(Tag));
  strcpy(tag->val, root->val);
  tag->modifier = root->modifier;
  tag->previous = mkcopy(root->previous);
  return tag;
}

Tag *push(const char *val, TagModifier mod, Tag *previous) {
  Tag *tag = (Tag *)malloc(sizeof(Tag));
  strcpy(tag->val, val);
  tag->modifier = mod;
  tag->previous = previous;
  return tag;
}

Tag *pop(Tag *stale) {
  if (stale == NULL)
    return NULL;
  Tag *tag = stale->previous;
  free(stale);
  return tag;
}

int parsetag(const char *text, TagKey *key, TagModifier *mod, char *val,
             int *closing) {
  int ptr = 0, nstart = strlen(TAG_START), nend = strlen(TAG_END), bufptr = 0;

  if (memcmp(text + ptr, TAG_START, nstart) != 0)
    return 0;
  ptr += nstart;
  *closing = text[ptr] == '/' && ptr++;

  for (TagKey t = 0; t < NullKey; ++t) {
    const char *key_repr = TagKeyRepr[t];
    if (memcmp(text + ptr, key_repr, strlen(key_repr)) == 0) {
      ptr += strlen(key_repr);
      *key = t;
      break;
    }
  }
  if (*key == NullKey)
    return 0;

  if (!*closing) {
    if (text[ptr] == ':' && ptr++) {
      const TagModifier *mods = AllowedTagModifiers[*key];
      int e = 0;
      for (; mods[e] != NullModifier; ++e) {
        const char *ext_repr = TagModifierRepr[mods[e]];
        if (memcmp(text + ptr, ext_repr, strlen(ext_repr)) == 0) {
          ptr += strlen(ext_repr);
          *mod = mods[e];
          break;
        }
      }
      if (mods[e] == NullModifier)
        return 0;
    }

    if (text[ptr++] != '=')
      return 0;
    while (text[ptr] != TAG_END[0])
      val[bufptr++] = text[ptr++];
  }

  if (memcmp(text + ptr, TAG_END, nend) != 0)
    return 0;

  return ptr + nend - 1;
}

void createblk(Block *blk, Tag *tags[NullKey], const char *text, int ntext) {
  blk->ntext = ntext;
  memcpy(blk->text, text, ntext);

  for (int i = 0; i < NullKey; ++i)
    blk->tags[i] = mkcopy(tags[i]);
}

int createblks(const char *name, Block *blks) {
  int len = strlen(name), nblks = 0, nbuf = 0, ptr, tagclose;
  char buf[len], val[len];
  TagKey key;
  TagModifier mod;

  Tag *tagstate[2][NullKey];
  for (TagKey tag = 0; tag < NullKey; ++tag)
    tagstate[Cur][tag] = tagstate[New][tag] = NULL;

  for (ptr = 0; ptr < len; ++ptr) {
    if (name[ptr] == TAG_START[0]) {
      key = NullKey, mod = NullModifier, tagclose = 0;
      memset(val, 0, sizeof(val));
      int size = parsetag(name + ptr, &key, &mod, val, &tagclose);
      // check for 'out of place' closing tag (closing tag without
      // corresponding opening tag).
      int oop = tagclose && tagstate[New][key] == NULL;
      // tag parse success check.
      if (size > 0 && key != NullKey && !oop) {
        tagstate[New][key] = tagclose ? pop(tagstate[New][key])
                                      : push(val, mod, tagstate[New][key]);
        // only creating a new block, if text is not empty as blocks with
        // empty text doesn't get rendered and that block becomes essentially
        // useless (very tiny memory/speed optimization).
        if (nbuf)
          createblk(&blks[nblks++], tagstate[Cur], buf, nbuf);

        ptr += size;
        // State Update: copy 'New' tagstate to 'Cur' tagstate.
        LOOP(tagstate[Cur][key] = pop(tagstate[Cur][key]));
        tagstate[Cur][key] = mkcopy(tagstate[New][key]);
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
    LOOP(tagstate[Cur][i] = pop(tagstate[Cur][i]));
    LOOP(tagstate[New][i] = pop(tagstate[New][i]));
  }

  return nblks;
}

void freeblks(Block *blks, int nblks) {
  for (int b = 0; b < nblks; ++b) {
    Block *blk = &blks[b];
    for (int i = 0; i < NullKey; ++i)
      LOOP(blk->tags[i] = pop(blk->tags[i]));
  }
}
