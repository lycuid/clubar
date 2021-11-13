#include "../config.h"
#include "blocks.h"
#include <stdlib.h>
#include <string.h>

char *AttrTagRepr(AttrTag tag) {
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
  case NullAttrTag:
    return NULL;
  }
}

char *AttrExtRepr(AttrExt ext) {
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
  case NullAttrExt:
    return NULL;
  }
}

void AllowedExtensions(AttrTag tag, AttrExt extensions[NullAttrExt]) {
  switch (tag) {
  case Box:
    extensions[0] = Left;
    extensions[1] = Right;
    extensions[2] = Top;
    extensions[3] = Bottom;
    extensions[4] = NullAttrExt;
    break;
  case BtnL:
  case BtnM:
  case BtnR:
  case ScrlU:
  case ScrlD:
    extensions[0] = Shift;
    extensions[1] = Ctrl;
    extensions[2] = Super;
    extensions[3] = Alt;
    extensions[4] = NullAttrExt;
    break;
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

void push(Attribute **root, char *val, AttrExt extension, Attribute *previous) {
  Attribute *attribute = (Attribute *)malloc(sizeof(Attribute));
  strcpy(attribute->val, val);
  attribute->extension = extension;
  attribute->previous = previous;
  *root = attribute;
}

void pop(Attribute **attrs) {
  Attribute *stale = *attrs;
  *attrs = (*attrs)->previous;
  free(stale);
}

int parsetag(char *const text, char *val, AttrExt *ext, AttrTag *tag,
             int *closing) {
  int ptr = 0, nstart = strlen(ATTR_TAG_START), nend = strlen(ATTR_TAG_END),
      bufptr = 0;

  if (memcmp(text + ptr, ATTR_TAG_START, nstart) != 0)
    return 0;
  ptr += nstart;
  *closing = text[ptr] == '/' && ptr++;

  AttrExt extensions[NullAttrExt];
  for (int i = 0; i < NullAttrExt; ++i)
    extensions[i] = NullAttrExt;

  for (AttrTag t = 0; t < NullAttrTag; ++t) {
    char *tagrepr = AttrTagRepr(t);
    if (tagrepr != NULL && memcmp(text + ptr, tagrepr, strlen(tagrepr)) == 0) {
      ptr += strlen(tagrepr);
      *tag = t;

      AllowedExtensions(t, extensions);
      for (int e = 0; extensions[e] != NullAttrExt; ++e) {
        char *extrepr = AttrExtRepr(extensions[e]);
        if (extrepr != NULL &&
            memcmp(text + ptr, extrepr, strlen(extrepr)) == 0) {
          ptr += strlen(extrepr);
          *ext = extensions[e];
          break;
        }
      }

      break;
    }
  }

  if (*tag == NullAttrTag)
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

Block createblk(Attribute **attrs, char *text, int ntext) {
  RenderInfo renderinfo;
  int fnindex = attrs[Fn] != NULL ? atoi(attrs[Fn]->val) : 0;
  createrenderinfo(text, ntext, fnindex, &renderinfo);

  Block blk =
      (Block){.text = malloc(ntext * sizeof(*text)),
              .ntext = ntext,
              .attrs = (Attribute **)malloc(NullAttrTag * sizeof(Attribute *)),
              .renderinfo = renderinfo};

  memcpy(blk.text, text, ntext * sizeof(*text));
  for (int i = 0; i < NullAttrTag; ++i)
    blk.attrs[i] = mkcopy(attrs[i]);

  return blk;
}

int createblks(char *name, Block *blks) {
  int len = strlen(name), ptr = 0, nblks = 0, stateupdated = 0, nbuf = 0,
      tagclose;
  char buf[len], val[len];

  Attribute *attrstate[2][NullAttrTag];
  for (AttrTag tag = 0; tag < NullAttrTag; ++tag) {
    attrstate[Cur][tag] = NULL;
    attrstate[New][tag] = NULL;
  }

  for (ptr = 0; ptr < len; ++ptr) {
    AttrTag tag = NullAttrTag;
    AttrExt ext = NullAttrExt;
    stateupdated = 0;

    if (name[ptr] == ATTR_TAG_START[0]) {
      tagclose = 0;
      memset(val, 0, len);
      int size = parsetag(name + ptr, val, &ext, &tag, &tagclose);
      if (size > 0 && tag >= 0 && tag < NullAttrTag) {
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

      if (tag >= 0 && tag < NullAttrTag) {
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

  for (int i = 0; i < NullAttrTag; ++i) {
    while (attrstate[Cur][i] != NULL)
      pop(&attrstate[Cur][i]);
    while (attrstate[New][i] != NULL)
      pop(&attrstate[New][i]);
  }

  return nblks;
}

void freeblks(Block *blks, int nblks) {
  for (int b = 0; b < nblks; ++b) {
    Block *blk = &blks[b];

    for (int i = 0; i < NullAttrTag; ++i)
      while (blk->attrs[i] != NULL)
        pop(&blk->attrs[i]);

    free(blk->text);
    free(blk->attrs);
  }
}
