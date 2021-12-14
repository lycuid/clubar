#include "../config.h"
#include "blocks.h"
#include <stdlib.h>
#include <string.h>

#define LOOP(cond) while ((cond))

void allowed_tag_extensions(Tag tag, Extension extensions[NullExt]) {
  // Always end with 'NullExt'.
  switch (tag) {
  case Box: {
    Extension temp[5] = {Left, Right, Top, Bottom, NullExt};
    memcpy(extensions, temp, sizeof(temp));
    break;
  }
  case BtnL:
  case BtnM:
  case BtnR:
  case ScrlU:
  case ScrlD: {
    Extension temp[5] = {Shift, Ctrl, Super, Alt, NullExt};
    memcpy(extensions, temp, sizeof(temp));
    break;
  }
  default:
    break;
  }
}

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
  Attribute *attribute = (Attribute *)malloc(sizeof(Attribute));
  strcpy(attribute->val, val);
  attribute->extension = extension;
  attribute->previous = previous;
  return attribute;
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
    Extension extensions[NullExt] = {NullExt};
    allowed_tag_extensions(*tag, extensions);
    for (int e = 0; extensions[e] != NullExt; ++e) {
      const char *ext_repr = ExtRepr[extensions[e]];
      if (memcmp(text + ptr, ext_repr, strlen(ext_repr)) == 0) {
        ptr += strlen(ext_repr);
        *ext = extensions[e];
        break;
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
  memset(blk->text, 0, 64);
  memcpy(blk->text, text, ntext);

  for (int i = 0; i < NullTag; ++i)
    blk->attrs[i] = mkcopy(attrs[i]);
}

int createblks(const char *name, Block *blks) {
  int len = strlen(name), nblks = 0, nbuf = 0, ptr, stateupdated, tagclose;
  char buf[len], val[len];
  Tag tag;
  Extension ext;

  Attribute *attrstate[2][NullTag];
  for (Tag tag = 0; tag < NullTag; ++tag) {
    attrstate[Cur][tag] = NULL;
    attrstate[New][tag] = NULL;
  }

  for (ptr = 0; ptr < len; ++ptr) {
    tag = NullTag, stateupdated = 0;

    if (name[ptr] == TAG_START[0]) {
      ext = NullExt, tagclose = 0;
      memset(val, 0, sizeof(val));
      int size = parsetag(name + ptr, &tag, &ext, val, &tagclose);
      if (size > 0 && tag != NullTag) {
        // On tag parse success, the only time state is not updated is when tag
        // is closed, with no corresponding opening tag.
        stateupdated = !(tagclose && attrstate[New][tag] == NULL);
        ptr += stateupdated ? size : 0;
        attrstate[New][tag] = tagclose ? pop(attrstate[New][tag])
                                       : push(val, ext, attrstate[New][tag]);
      }
    }

    if (stateupdated) {
      if (nbuf)
        createblk(&blks[nblks++], attrstate[Cur], buf, nbuf);

      if (tag != NullTag) {
        LOOP(attrstate[Cur][tag] = pop(attrstate[Cur][tag]));
        attrstate[Cur][tag] = mkcopy(attrstate[New][tag]);
      }
      memset(buf, nbuf = 0, sizeof(buf));
      continue;
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
