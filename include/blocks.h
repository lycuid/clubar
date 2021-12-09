typedef enum { Fn, Fg, Bg, Box, BtnL, BtnM, BtnR, ScrlU, ScrlD, NullTag } Tag;
static const char *const TagRepr[NullTag] = {
    "Fn", "Fg", "Bg", "Box", "BtnL", "BtnM", "BtnR", "ScrlU", "ScrlD"};

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
static const char *const ExtRepr[NullExt] = {
    ":Shift", ":Ctrl", ":Super", ":Alt", ":Left", ":Right", ":Top", ":Bottom"};

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
} Block;

void allowed_tag_extensions(Tag, Extension[NullExt]);
Attribute *mkcopy(Attribute *);
Attribute *push(const char *, Extension, Attribute *);
Attribute *pop(Attribute *);
int parsetag(const char *, Tag *, Extension *, char *, int *);
Block *createblk(Attribute **, const char *, int);

// public.
void freeblks(Block **, int);
int createblks(const char *, Block **);
