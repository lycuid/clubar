#include "tags.h"
#include <string.h>

#define TAG_ALLOCATOR_CAPACITY (1 << 5)

// lifetime of this struct is the entire runtime of the application.
// no need to explicitly free pointers... i guess.
static struct {
  Tag *tags;
  size_t size;
} allocator = {.tags = NULL, .size = 0};

static inline Tag *tag_request(void)
{
  if (!allocator.tags && !(allocator.size = 0))
    return (Tag *)malloc(sizeof(Tag));
  Tag *tag       = allocator.tags;
  allocator.tags = tag->previous, tag->previous = NULL, allocator.size--;
  return tag;
}

static inline void tag_release(Tag *tag)
{
  if (tag) {
    if (allocator.size == TAG_ALLOCATOR_CAPACITY)
      free(tag);
    else
      tag->previous = allocator.tags, allocator.tags = tag, allocator.size++;
  }
}

Tag *tag_create(Tag *previous, const char *val, TagModifierMask tmod_mask)
{
  Tag *tag = tag_request();
  strcpy(tag->val, val), tag->tmod_mask = tmod_mask, tag->previous = previous;
  return tag;
}

Tag *tag_remove(Tag *stale)
{
  if (stale == NULL)
    return NULL;
  Tag *tag = stale->previous;
  tag_release(stale);
  return tag;
}

Tag *tag_clone(const Tag *root)
{
  if (!root)
    return NULL;
  return tag_create(tag_clone(root->previous), root->val, root->tmod_mask);
}
