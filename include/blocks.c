#include "../config.h"
#include "blocks.h"
#include <stdlib.h>
#include <string.h>

char *tag_repr(Tag tag) {
  switch (tag) {
  case Fn:
    return "Fn";
  case Fg:
    return "Fg";
  case Bg:
    return "Bg";
  case Box:
    return "Box";
  case BtnL:
    return "BtnL";
  case BtnM:
    return "BtnM";
  case BtnR:
    return "BtnR";
  case ScrlU:
    return "ScrlU";
  case ScrlD:
    return "ScrlD";
  default:
    return NULL;
  }
}

char *ext_repr(Extension ext) {
  switch (ext) {
  case Shift:
    return ":Shift";
  case Ctrl:
    return ":Ctrl";
  case Super:
    return ":Super";
  case Alt:
    return ":Alt";
  case Left:
    return ":Left";
  case Right:
    return ":Right";
  case Top:
    return ":Top";
  case Bottom:
    return ":Bottom";
  default:
    return NULL;
  }
}

void allowed_tag_extensions(Tag tag, Extension extensions[NullExt]) {
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

void push(Attribute **root, char *val, Extension extension,
          Attribute *previous) {
  Attribute *attribute = (Attribute *)malloc(sizeof(Attribute));
  strcpy(attribute->val, val);
  attribute->extension = extension;
  attribute->previous = previous;
  *root = attribute;
}

void pop(Attribute **attrs) {
  Attribute *stale = (*attrs);
  (*attrs) = (*attrs)->previous;
  free(stale);
}

int parsetag(const char *text, Tag *tag, Extension *ext, char *val,
             int *closing) {
  int ptr = 0, nstart = strlen(ATTR_TAG_START), nend = strlen(ATTR_TAG_END),
      bufptr = 0;

  if (memcmp(text + ptr, ATTR_TAG_START, nstart) != 0)
    return 0;
  ptr += nstart;
  *closing = text[ptr] == '/' && ptr++;

  Extension extensions[NullExt];
  for (int i = 0; i < NullExt; ++i)
    extensions[i] = NullExt;

  for (Tag t = 0; t < NullTag; ++t) {
    char *s_tag = tag_repr(t);
    if (s_tag != NULL && memcmp(text + ptr, s_tag, strlen(s_tag)) == 0) {
      ptr += strlen(s_tag);
      *tag = t;

      allowed_tag_extensions(t, extensions);
      for (int e = 0; extensions[e] != NullExt; ++e) {
        char *s_ext = ext_repr(extensions[e]);
        if (s_ext != NULL && memcmp(text + ptr, s_ext, strlen(s_ext)) == 0) {
          ptr += strlen(s_ext);
          *ext = extensions[e];
          break;
        }
      }

      break;
    }
  }

  if (*tag == NullTag)
    return 0;

  if (!*closing) {
    if (text[ptr++] != '=')
      return 0;
    while (text[ptr] != ATTR_TAG_END[0])
      val[bufptr++] = text[ptr++];
  }

  if (memcmp(text + ptr, ATTR_TAG_END, nend) != 0)
    return 0;

  return ptr + nend - 1;
}

Block *createblk(Attribute **attrs, char *text, int ntext) {
  Block *blk = (Block *)malloc(sizeof(Block));
  blk->text = (char *)malloc(ntext * strlen(text));
  strcpy(blk->text, text);
  blk->ntext = ntext;
  blk->attrs = (Attribute **)malloc(NullTag * sizeof(Attribute *));
  blk->data = NULL;

  for (int i = 0; i < NullTag; ++i)
    blk->attrs[i] = mkcopy(attrs[i]);

  return blk;
}

int createblks(const char *name, Block **blks) {
  int len = strlen(name), ptr = 0, nblks = 0, stateupdated = 0, nbuf = 0,
      tagclose;
  char buf[len], val[len];

  Attribute *attrstate[2][NullTag];
  for (Tag tag = 0; tag < NullTag; ++tag) {
    attrstate[Cur][tag] = NULL;
    attrstate[New][tag] = NULL;
  }

  for (ptr = 0; ptr < len; ++ptr) {
    Tag tag = NullTag;
    Extension ext = NullExt;
    stateupdated = 0;

    if (name[ptr] == ATTR_TAG_START[0]) {
      tagclose = 0;
      memset(val, 0, len);
      int size = parsetag(name + ptr, &tag, &ext, val, &tagclose);
      if (size > 0 && tag >= 0 && tag < NullTag) {
        ptr += size;
        stateupdated = 1;
        if (tagclose) {
          if (attrstate[New][tag] != NULL)
            pop(&attrstate[New][tag]);
          else {
            stateupdated = 0;
            ptr -= size;
          }
        } else
          push(&attrstate[New][tag], val, ext, attrstate[New][tag]);
      }
    }

    if (stateupdated) {
      if (nbuf)
        blks[nblks++] = createblk(attrstate[Cur], buf, nbuf);

      if (tag >= 0 && tag < NullTag) {
        while (attrstate[Cur][tag] != NULL)
          pop(&attrstate[Cur][tag]);
        attrstate[Cur][tag] = mkcopy(attrstate[New][tag]);
      }
      nbuf = 0;
      memset(buf, 0, len);
      continue;
    }

    buf[nbuf++] = name[ptr];
  }

  if (nbuf)
    blks[nblks++] = createblk(attrstate[Cur], buf, nbuf);

  for (int i = 0; i < NullTag; ++i) {
    while (attrstate[Cur][i] != NULL)
      pop(&attrstate[Cur][i]);
    while (attrstate[New][i] != NULL)
      pop(&attrstate[New][i]);
  }

  return nblks;
}

void freeblks(Block **blks, int nblks) {
  for (int b = 0; b < nblks && blks[b] != NULL; ++b) {
    Block *blk = blks[b];

    for (int i = 0; i < NullTag; ++i)
      while (blk->attrs[i] != NULL)
        pop(&blk->attrs[i]);

    free(blk->text);
    free(blk->attrs);
    if (blk->data)
      free(blk->data);
    free(blks[b]);
    blks[b] = NULL;
  }
}
