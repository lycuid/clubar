#include "../config.h"
#include "blocks.h"
#include <stdlib.h>
#include <string.h>

#define LOOP(cond) while ((cond))
enum { Cur, New };

Attribute *mkcopy(Attribute *root) {
  if (root == NULL)
    return NULL;
  Attribute *attr = (Attribute *)malloc(sizeof(Attribute));
  strcpy(attr->val, root->val);
  attr->extension = root->extension;
  attr->previous = mkcopy(root->previous);
  return attr;
}

Attribute *push(const char *val, Extension extension, Attribute *previous) {
  Attribute *attr = (Attribute *)malloc(sizeof(Attribute));
  strcpy(attr->val, val);
  attr->extension = extension;
  attr->previous = previous;
  return attr;
}

Attribute *pop(Attribute *stale) {
  if (stale == NULL)
    return NULL;
  Attribute *attr = stale->previous;
  free(stale);
  return attr;
}

int parsetag(const char *text, Tag *tag, Extension *ext, char *val,
             int *closing) {
  int ptr = 0, nstart = strlen(TAG_START), nend = strlen(TAG_END), bufptr = 0;

  if (memcmp(text + ptr, TAG_START, nstart) != 0)
    return 0;
  ptr += nstart;
  *closing = text[ptr] == '/' && ptr++;

  for (Tag t = 0; t < NullTag; ++t) {
    const char *tag_repr = TagRepr[t];
    if (memcmp(text + ptr, tag_repr, strlen(tag_repr)) == 0) {
      ptr += strlen(tag_repr);
      *tag = t;
      break;
    }
  }
  if (*tag == NullTag)
    return 0;

  if (!*closing) {
    if (text[ptr] == ':' && ptr++) {
      const Extension *extensions = AllowedTagExtensions[*tag];
      for (int e = 0; extensions[e] != NullExt; ++e) {
        const char *ext_repr = ExtRepr[extensions[e]];
        if (memcmp(text + ptr, ext_repr, strlen(ext_repr)) == 0) {
          ptr += strlen(ext_repr);
          *ext = extensions[e];
          break;
        }
      }
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

void createblk(Block *blk, Attribute *attrs[NullTag], const char *text,
               int ntext) {
  memcpy(blk->text, text, ntext);
  blk->text[ntext] = 0;

  for (int i = 0; i < NullTag; ++i)
    blk->attrs[i] = mkcopy(attrs[i]);
}

int createblks(const char *name, Block *blks) {
  int len = strlen(name), nblks = 0, nbuf = 0, ptr, tagclose;
  char buf[len], val[len];
  Tag tag;
  Extension ext;

  Attribute *attrstate[2][NullTag];
  for (Tag tag = 0; tag < NullTag; ++tag)
    attrstate[Cur][tag] = attrstate[New][tag] = NULL;

  for (ptr = 0; ptr < len; ++ptr) {
    if (name[ptr] == TAG_START[0]) {
      tag = NullTag, ext = NullExt, tagclose = 0;
      memset(val, 0, sizeof(val));
      int size = parsetag(name + ptr, &tag, &ext, val, &tagclose);
      // check for 'out of place' closing tag (closing tag without
      // corresponding opening tag).
      int oop = tagclose && attrstate[New][tag] == NULL;
      // tag parse success check.
      if (size > 0 && tag != NullTag && !oop) {
        attrstate[New][tag] = tagclose ? pop(attrstate[New][tag])
                                       : push(val, ext, attrstate[New][tag]);
        // only creating a new block, if text is not empty as blocks with
        // empty text doesn't get rendered and that block becomes essentially
        // useless (very tiny memory/speed optimization).
        if (nbuf)
          createblk(&blks[nblks++], attrstate[Cur], buf, nbuf);

        ptr += size;
        // State Update: copy 'New' attrstate to 'Cur' attrstate.
        LOOP(attrstate[Cur][tag] = pop(attrstate[Cur][tag]));
        attrstate[Cur][tag] = mkcopy(attrstate[New][tag]);
        // reset temp value buffer and continue loop, once state is updated.
        memset(buf, nbuf = 0, sizeof(buf));
        continue;
      }
    }
    buf[nbuf++] = name[ptr];
  }

  if (nbuf)
    createblk(&blks[nblks++], attrstate[Cur], buf, nbuf);

  for (int i = 0; i < NullTag; ++i) {
    LOOP(attrstate[Cur][i] = pop(attrstate[Cur][i]));
    LOOP(attrstate[New][i] = pop(attrstate[New][i]));
  }

  return nblks;
}

void freeblks(Block *blks, int nblks) {
  for (int b = 0; b < nblks; ++b) {
    Block *blk = &blks[b];
    for (int i = 0; i < NullTag; ++i)
      LOOP(blk->attrs[i] = pop(blk->attrs[i]));
  }
}
