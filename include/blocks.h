typedef enum { Fn, Fg, Bg, Box, BtnL, BtnM, BtnR, ScrlU, ScrlD, NullTag } Tag;

typedef enum {
  Shift,
  Ctrl,
  Super,
  Alt,
  Left,
  Right,
  Top,
  Bottom,
  NullExt
} Extension;

typedef enum { Cur, New } State;

typedef struct _Attribute {
  char val[128];
  Extension extension;
  struct _Attribute *previous;
} Attribute;

typedef struct {
  char *text;
  int ntext;
  Attribute **attrs;
  void *data; // extra, custom info.
} Block;

char *tag_repr(Tag);
char *ext_repr(Extension);
void allowed_tag_extensions(Tag, Extension[NullExt]);
Attribute *mkcopy(Attribute *);
void push(Attribute **, char *, Extension, Attribute *);
void pop(Attribute **);
int parsetag(const char *, Tag *, Extension *, char *, int *);
Block *createblk(Attribute **, char *, int);

// public.
void freeblks(Block **, int);
int createblks(const char *, Block **);
