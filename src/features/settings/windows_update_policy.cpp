#include "reach/features/windows_update.h"

static uint16_t fold_ascii(uint16_t value)
{
    return value >= (uint16_t)'A' && value <= (uint16_t)'Z'
               ? (uint16_t)(value + ((uint16_t)'a' - (uint16_t)'A'))
               : value;
}

static int32_t contains_case_insensitive(const uint16_t *text, const char *needle)
{
    if (text == nullptr || needle == nullptr || needle[0] == 0)
    {
        return 0;
    }
    for (size_t start = 0; text[start] != 0; ++start)
    {
        size_t offset = 0;
        while (needle[offset] != 0 && text[start + offset] != 0 &&
               fold_ascii(text[start + offset]) ==
                   fold_ascii((uint16_t)(unsigned char)needle[offset]))
        {
            ++offset;
        }
        if (needle[offset] == 0)
        {
            return 1;
        }
    }
    return 0;
}

static void copy_ascii(uint16_t *destination, size_t capacity, const char *source)
{
    if (destination == nullptr || capacity == 0)
    {
        return;
    }
    size_t index = 0;
    while (source != nullptr && source[index] != 0 && index + 1 < capacity)
    {
        destination[index] = (uint16_t)(unsigned char)source[index];
        ++index;
    }
    destination[index] = 0;
}

extern "C" int32_t
reach_windows_update_matches_security_maintenance(const reach_windows_update_item *update)
{
    if (update == nullptr)
    {
        return 0;
    }
    static const char *category_terms[] = {
        "Security Updates", "Critical Updates",          "Definition Updates",
        "Update Rollups",   "Windows Security platform", "Microsoft Defender Antivirus"};
    static const char *title_terms[] = {"Security Update",    "Cumulative Update",
                                        "Servicing Stack",    "Windows Security platform",
                                        "Microsoft Defender", "Security Intelligence Update",
                                        "Definition Update",  "Malicious Software Removal Tool"};
    for (const char *term : category_terms)
    {
        if (contains_case_insensitive(update->categories, term))
            return 1;
    }
    for (const char *term : title_terms)
    {
        if (contains_case_insensitive(update->identity.title, term))
            return 1;
    }
    return 0;
}

extern "C" void reach_windows_update_apply_default_selection(reach_windows_update_list *updates)
{
    if (updates == nullptr)
        return;
    for (size_t index = 0; index < updates->count && index < REACH_WINDOWS_UPDATE_MAX_UPDATES;
         ++index)
    {
        reach_windows_update_item *update = &updates->updates[index];
        update->selected = reach_windows_update_matches_security_maintenance(update);
        update->state =
            update->selected ? REACH_WINDOWS_UPDATE_SELECTED : REACH_WINDOWS_UPDATE_DISCOVERED;
        if (update->selected)
            copy_ascii(update->selected_reason, REACH_WINDOWS_UPDATE_TEXT_CAPACITY,
                       "SecurityMaintenance");
    }
}

extern "C" const uint16_t *reach_windows_update_state_label(reach_windows_update_state state)
{
    switch (state)
    {
    case REACH_WINDOWS_UPDATE_DISCOVERED:
        return (const uint16_t *)u"Available";
    case REACH_WINDOWS_UPDATE_SELECTED:
        return (const uint16_t *)u"Selected";
    case REACH_WINDOWS_UPDATE_DOWNLOADING:
        return (const uint16_t *)u"Downloading";
    case REACH_WINDOWS_UPDATE_DOWNLOADED:
        return (const uint16_t *)u"Downloaded";
    case REACH_WINDOWS_UPDATE_INSTALLING:
        return (const uint16_t *)u"Installing";
    case REACH_WINDOWS_UPDATE_INSTALLED_NO_REBOOT_REQUIRED:
        return (const uint16_t *)u"Installed";
    case REACH_WINDOWS_UPDATE_INSTALLED_REBOOT_REQUIRED:
        return (const uint16_t *)u"Installed - restart required";
    case REACH_WINDOWS_UPDATE_REBOOT_OBSERVED:
        return (const uint16_t *)u"Verifying";
    case REACH_WINDOWS_UPDATE_VERIFIED_INSTALLED:
        return (const uint16_t *)u"Verified installed";
    case REACH_WINDOWS_UPDATE_FAILED:
        return (const uint16_t *)u"Failed";
    default:
        return (const uint16_t *)u"Available";
    }
}

extern "C" const uint16_t *
reach_windows_update_failure_label(reach_windows_update_failure_class failure)
{
    switch (failure)
    {
    case REACH_WINDOWS_UPDATE_NOT_ELEVATED:
        return (
            const uint16_t *)u"Administrator permission is required to install Windows updates.";
    case REACH_WINDOWS_UPDATE_DOWNLOAD_FAILED:
        return (const uint16_t *)u"Download failed";
    case REACH_WINDOWS_UPDATE_INSTALL_FAILED:
        return (const uint16_t *)u"Install failed";
    case REACH_WINDOWS_UPDATE_VERIFICATION_FAILED:
        return (const uint16_t *)u"Verification failed";
    case REACH_WINDOWS_UPDATE_REBOOT_REQUIRED_BEFORE_INSTALL:
        return (const uint16_t *)u"Restart required before installation";
    case REACH_WINDOWS_UPDATE_SUPERSEDED_OR_NO_LONGER_APPLICABLE:
        return (const uint16_t *)u"Superseded or no longer applicable";
    case REACH_WINDOWS_UPDATE_POLICY_BLOCKED:
        return (const uint16_t *)u"Blocked by update policy";
    case REACH_WINDOWS_UPDATE_ANOTHER_OPERATION_IN_PROGRESS:
        return (const uint16_t *)u"Another update operation is in progress";
    default:
        return (const uint16_t *)u"";
    }
}
