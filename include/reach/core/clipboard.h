#ifndef REACH_CORE_CLIPBOARD_H
#define REACH_CORE_CLIPBOARD_H

#include <stddef.h>
#include <stdint.h>

#define REACH_CLIPBOARD_PREVIEW_CAPACITY 241

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_clipboard_item_kind
    {
        REACH_CLIPBOARD_ITEM_TEXT = 1,
        REACH_CLIPBOARD_ITEM_IMAGE = 2
    } reach_clipboard_item_kind;

    typedef struct reach_clipboard_item
    {
        uint64_t id;
        uint64_t content_hash;
        reach_clipboard_item_kind kind;
        uint16_t preview[REACH_CLIPBOARD_PREVIEW_CAPACITY];
        uint64_t thumbnail_id;
        uint32_t image_width;
        uint32_t image_height;
    } reach_clipboard_item;

    void reach_clipboard_build_text_preview(const uint16_t *text, uint16_t *out_preview,
                                            size_t out_capacity);

#ifdef __cplusplus
}
#endif

#endif
