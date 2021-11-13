typedef enum {
  Fn,
  Fg,
  Bg,
  Box,
  BtnL,
  BtnM,
  BtnR,
  ScrlU,
  ScrlD,
  NullAttrTag
} AttrTag;

typedef enum {
  Shift,
  Ctrl,
  Super,
  Alt,
  Left,
  Right,
  Top,
  Bottom,
  NullAttrExt
} AttrExt;

typedef enum { Cur, New } state_t;

typedef struct _Attribute {
  char val[128];
  AttrExt extension;
  struct _Attribute *previous;
} Attribute;

typedef struct {
  int x, width;
} RenderInfo;

typedef struct {
  char *text;
  int ntext;
  Attribute **attrs;
  RenderInfo renderinfo;
} Block;

void freeblks(Block *, int);
Block createblk(Attribute **, char *, int);
int createblks(char *, Block *);
void createrenderinfo(const char *, int, int, RenderInfo *);
