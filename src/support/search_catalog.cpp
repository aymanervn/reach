#include "reach/support/search_catalog.h"

#define REACH_SEARCH_ALIAS_TERM_CAPACITY 96

static const reach_search_alias_entry reach_search_alias_table[] = {
    {"Control Panel", "control.exe", "", "control panel|control"},
    {"Environment Variables", "rundll32.exe", "sysdm.cpl,EditEnvironmentVariables",
     "environment variables|env|envvar|path|system variables|user variables"},
    {"Advanced System Settings", "SystemPropertiesAdvanced.exe", "",
     "advanced system settings|system properties"},
    {"Performance Options", "SystemPropertiesPerformance.exe", "",
     "performance options|visual effects|virtual memory|pagefile|perf"},
    {"Device Manager", "devmgmt.msc", "", "device manager|devices|drivers|devmgmt"},
    {"Disk Management", "diskmgmt.msc", "", "disk management|partitions|volumes|diskmgmt"},
    {"Services", "services.msc", "", "services|service manager"},
    {"Computer Management", "compmgmt.msc", "", "computer management|compmgmt"},
    {"Event Viewer", "eventvwr.msc", "", "event viewer|event log|logs|eventvwr"},
    {"Task Scheduler", "taskschd.msc", "", "task scheduler|scheduled tasks|schedule|taskschd"},
    {"Performance Monitor", "perfmon.msc", "", "performance monitor|perfmon"},
    {"Resource Monitor", "resmon.exe", "", "resource monitor|resmon"},
    {"Registry Editor", "regedit.exe", "", "registry editor|registry|regedit"},
    {"Local Group Policy Editor", "gpedit.msc", "", "group policy|policy|gpedit"},
    {"Local Users and Groups", "lusrmgr.msc", "", "local users|users and groups|lusrmgr"},
    {"Certificates", "certmgr.msc", "", "certificates|certs|certmgr"},
    {"Firewall with Advanced Security", "wf.msc", "", "firewall advanced|advanced firewall|wf"},
    {"Windows Defender Firewall", "firewall.cpl", "", "firewall|windows firewall"},
    {"Programs and Features", "appwiz.cpl", "",
     "programs and features|add or remove programs|uninstall a program|uninstall|appwiz"},
    {"Network Connections", "ncpa.cpl", "",
     "network connections|adapter settings|network adapters|ncpa"},
    {"Internet Options", "inetcpl.cpl", "", "internet options|proxy|inetcpl"},
    {"Sound", "mmsys.cpl", "", "sound|audio|playback devices|recording devices|mmsys"},
    {"Mouse Properties", "main.cpl", "", "mouse|cursor|pointer"},
    {"Keyboard Properties", "main.cpl", "@1", "keyboard"},
    {"Display Settings", "desk.cpl", "", "display|screen resolution|desk"},
    {"Power Options", "powercfg.cpl", "", "power options|power plan|power|battery|powercfg"},
    {"Date and Time", "timedate.cpl", "", "date and time|clock|time zone|timezone|timedate"},
    {"Region", "intl.cpl", "", "region|locale|regional settings|intl"},
    {"System", "sysdm.cpl", "", "computer name|sysdm"},
    {"User Accounts", "netplwiz.exe", "", "user accounts|accounts|autologon|netplwiz"},
    {"System Information", "msinfo32.exe", "", "system information|system info|specs|msinfo32"},
    {"DirectX Diagnostic Tool", "dxdiag.exe", "", "dxdiag|directx|graphics info"},
    {"System Configuration", "msconfig.exe", "", "system configuration|boot options|msconfig"},
    {"Windows Features", "optionalfeatures.exe", "",
     "windows features|turn windows features on or off|optional features"},
    {"Credential Manager", "control.exe", "/name Microsoft.CredentialManager",
     "credential manager|credentials|saved passwords"},
    {"Fonts", "control.exe", "fonts", "fonts"},
    {"Devices and Printers", "control.exe", "printers",
     "devices and printers|printers|add printer"},
    {"Disk Cleanup", "cleanmgr.exe", "", "disk cleanup|free space|cleanmgr"},
    {"Defragment and Optimize Drives", "dfrgui.exe", "",
     "defragment|optimize drives|defrag|dfrgui"},
    {"Character Map", "charmap.exe", "", "character map|symbols|unicode|charmap"},
    {"On-Screen Keyboard", "osk.exe", "", "on-screen keyboard|virtual keyboard|osk"},
    {"Magnifier", "magnify.exe", "", "magnifier|zoom|magnify"},
    {"Task Manager", "taskmgr.exe", "", "task manager|processes|taskmgr"},
    {"Command Prompt", "cmd.exe", "", "command prompt|console|cmd"},
    {"Windows PowerShell", "powershell.exe", "", "powershell"},
    {"Remote Desktop Connection", "mstsc.exe", "", "remote desktop|rdp|mstsc"},
    {"System Restore", "rstrui.exe", "", "system restore|restore point|rstrui"},
    {"Notepad", "notepad.exe", "", "notepad|text editor"},
    {"Print Management", "printmanagement.msc", "", "print management"},
    {"Component Services", "dcomcnfg.exe", "", "component services|dcom"},
    {"ODBC Data Sources", "odbcad32.exe", "", "odbc|data sources"},
    {"Windows Memory Diagnostic", "mdsched.exe", "", "memory diagnostic|ram test|mdsched"},
    {"DiskPart", "diskpart.exe", "", "diskpart"}};

size_t reach_search_alias_count(void)
{
    return sizeof(reach_search_alias_table) / sizeof(reach_search_alias_table[0]);
}

const reach_search_alias_entry *reach_search_alias_at(size_t index)
{
    if (index >= reach_search_alias_count())
    {
        return nullptr;
    }
    return &reach_search_alias_table[index];
}

static size_t reach_search_alias_copy_term(const char *term, size_t length, uint16_t *out_term,
                                           size_t capacity)
{
    size_t index = 0;
    while (index < length && index + 1 < capacity)
    {
        out_term[index] = (uint16_t)(unsigned char)term[index];
        ++index;
    }
    out_term[index] = 0;
    return index;
}

reach_search_match_tier reach_search_alias_match(const reach_search_alias_entry *entry,
                                                 const uint16_t *query)
{
    if (entry == nullptr || query == nullptr || query[0] == 0)
    {
        return REACH_SEARCH_MATCH_NONE;
    }

    reach_search_match_tier best = REACH_SEARCH_MATCH_NONE;
    uint16_t term[REACH_SEARCH_ALIAS_TERM_CAPACITY] = {};

    if (entry->display != nullptr)
    {
        size_t length = 0;
        while (entry->display[length] != 0)
        {
            ++length;
        }
        reach_search_alias_copy_term(entry->display, length, term,
                                     REACH_SEARCH_ALIAS_TERM_CAPACITY);
        best = reach_search_match_name(term, query);
    }

    if (entry->aliases == nullptr)
    {
        return best;
    }

    const char *cursor = entry->aliases;
    while (*cursor != 0)
    {
        const char *start = cursor;
        while (*cursor != 0 && *cursor != '|')
        {
            ++cursor;
        }
        reach_search_alias_copy_term(start, (size_t)(cursor - start), term,
                                     REACH_SEARCH_ALIAS_TERM_CAPACITY);
        reach_search_match_tier tier = reach_search_match_name(term, query);
        if (tier > best)
        {
            best = tier;
        }
        if (*cursor == '|')
        {
            ++cursor;
        }
    }

    return best;
}
