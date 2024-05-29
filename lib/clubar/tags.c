#include "tags.h"
#include <stdlib.h>
#include <string.h>

#define TAG_STASH_CAPACITY (1 << 5)

// lifetime for this is going to be the entire runtime of the application, (no
// need to explicitly free).
static Tag *stash = NULL;

static inline Tag *tag_request(void)
{
    if (!stash)
        return malloc(sizeof(Tag));
    Tag *tag = stash;
    stash    = stash->previous;
    memset(tag->val, 0, sizeof(tag->val));
    tag->previous = NULL, tag->tmod_mask = 0x0;
    return tag;
}

static inline void tag_release(Tag *tag)
{
    // Storing tag allocator size in 'tmod_mask'.
    tag->tmod_mask = stash ? stash->tmod_mask + 1 : 0;
    if (tag->tmod_mask == TAG_STASH_CAPACITY)
        free(tag);
    else
        tag->previous = stash, stash = tag;
}

Tag *tag_create(Tag *previous, const char *val, TagModifierMask tmod_mask)
{
    Tag *tag = tag_request();
    strcpy(tag->val, val);
    tag->tmod_mask = tmod_mask, tag->previous = previous;
    return tag;
}

Tag *tag_remove(Tag *stale)
{
    if (!stale)
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
