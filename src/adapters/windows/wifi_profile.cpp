#include "wifi_profile.h"

namespace
{

struct reach_wifi_xml_writer
{
    uint16_t *buffer;
    size_t capacity;
    size_t length;
    int32_t overflow;
};

void reach_wifi_xml_write(reach_wifi_xml_writer *writer, const char *ascii)
{
    for (size_t index = 0; ascii[index] != 0; ++index)
    {
        if (writer->length + 1 >= writer->capacity)
        {
            writer->overflow = 1;
            return;
        }
        writer->buffer[writer->length++] = (uint16_t)(unsigned char)ascii[index];
    }
    writer->buffer[writer->length] = 0;
}

void reach_wifi_xml_write_escaped(reach_wifi_xml_writer *writer, const uint16_t *text)
{
    uint16_t escaped[REACH_WIFI_PROFILE_CAPACITY] = {};
    if (reach_wifi_escape_xml(escaped, REACH_WIFI_PROFILE_CAPACITY, text) == 0 && text[0] != 0)
    {
        writer->overflow = 1;
        return;
    }
    for (size_t index = 0; escaped[index] != 0; ++index)
    {
        if (writer->length + 1 >= writer->capacity)
        {
            writer->overflow = 1;
            return;
        }
        writer->buffer[writer->length++] = escaped[index];
    }
    writer->buffer[writer->length] = 0;
}

const char *reach_wifi_authentication_name(reach_wifi_security security)
{
    switch (security)
    {
    case REACH_WIFI_SECURITY_OPEN:
    case REACH_WIFI_SECURITY_WEP:
        return "open";
    case REACH_WIFI_SECURITY_WPA2_PERSONAL:
        return "WPA2PSK";
    case REACH_WIFI_SECURITY_WPA3_PERSONAL:
        return "WPA3SAE";
    default:
        return nullptr;
    }
}

const char *reach_wifi_encryption_name(reach_wifi_security security)
{
    switch (security)
    {
    case REACH_WIFI_SECURITY_OPEN:
        return "none";
    case REACH_WIFI_SECURITY_WEP:
        return "WEP";
    case REACH_WIFI_SECURITY_WPA2_PERSONAL:
    case REACH_WIFI_SECURITY_WPA3_PERSONAL:
        return "AES";
    default:
        return nullptr;
    }
}

} // namespace

size_t reach_wifi_escape_xml(uint16_t *out_text, size_t capacity, const uint16_t *text)
{
    if (out_text == nullptr || capacity == 0)
    {
        return 0;
    }
    out_text[0] = 0;
    if (text == nullptr)
    {
        return 0;
    }

    size_t length = 0;
    for (size_t index = 0; text[index] != 0; ++index)
    {
        const char *entity = nullptr;
        switch (text[index])
        {
        case u'&':
            entity = "&amp;";
            break;
        case u'<':
            entity = "&lt;";
            break;
        case u'>':
            entity = "&gt;";
            break;
        case u'"':
            entity = "&quot;";
            break;
        case u'\'':
            entity = "&apos;";
            break;
        default:
            break;
        }

        if (entity == nullptr)
        {
            if (length + 1 >= capacity)
            {
                out_text[0] = 0;
                return 0;
            }
            out_text[length++] = text[index];
            continue;
        }

        for (size_t entity_index = 0; entity[entity_index] != 0; ++entity_index)
        {
            if (length + 1 >= capacity)
            {
                out_text[0] = 0;
                return 0;
            }
            out_text[length++] = (uint16_t)(unsigned char)entity[entity_index];
        }
    }
    out_text[length] = 0;
    return length;
}

size_t reach_wifi_build_profile_xml(uint16_t *out_xml, size_t capacity, const uint16_t *ssid,
                                    const uint16_t *key, reach_wifi_security security,
                                    int32_t connect_automatically, int32_t hidden)
{
    if (out_xml == nullptr || capacity == 0)
    {
        return 0;
    }
    out_xml[0] = 0;
    if (ssid == nullptr || ssid[0] == 0)
    {
        return 0;
    }

    const char *authentication = reach_wifi_authentication_name(security);
    const char *encryption = reach_wifi_encryption_name(security);
    if (authentication == nullptr || encryption == nullptr)
    {
        return 0;
    }

    size_t key_length = 0;
    while (key != nullptr && key[key_length] != 0)
    {
        ++key_length;
    }
    if (!reach_wifi_key_length_valid(security, key_length))
    {
        return 0;
    }

    reach_wifi_xml_writer writer = {out_xml, capacity, 0, 0};
    reach_wifi_xml_write(&writer, "<?xml version=\"1.0\"?>\r\n<WLANProfile "
                                  "xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\">"
                                  "\r\n\t<name>");
    reach_wifi_xml_write_escaped(&writer, ssid);
    reach_wifi_xml_write(&writer, "</name>\r\n\t<SSIDConfig>\r\n\t\t<SSID>\r\n\t\t\t<name>");
    reach_wifi_xml_write_escaped(&writer, ssid);
    reach_wifi_xml_write(&writer, "</name>\r\n\t\t</SSID>\r\n\t\t<nonBroadcast>");
    reach_wifi_xml_write(&writer, hidden ? "true" : "false");
    reach_wifi_xml_write(&writer, "</nonBroadcast>\r\n\t</SSIDConfig>\r\n\t"
                                  "<connectionType>ESS</connectionType>\r\n\t<connectionMode>");
    reach_wifi_xml_write(&writer, connect_automatically ? "auto" : "manual");
    reach_wifi_xml_write(&writer, "</connectionMode>\r\n\t<MSM>\r\n\t\t<security>\r\n\t\t\t"
                                  "<authEncryption>\r\n\t\t\t\t<authentication>");
    reach_wifi_xml_write(&writer, authentication);
    reach_wifi_xml_write(&writer, "</authentication>\r\n\t\t\t\t<encryption>");
    reach_wifi_xml_write(&writer, encryption);
    reach_wifi_xml_write(&writer,
                         "</encryption>\r\n\t\t\t\t<useOneX>false</useOneX>\r\n\t\t\t"
                         "</authEncryption>\r\n");

    if (security != REACH_WIFI_SECURITY_OPEN)
    {
        reach_wifi_xml_write(&writer, "\t\t\t<sharedKey>\r\n\t\t\t\t<keyType>");
        reach_wifi_xml_write(&writer,
                             security == REACH_WIFI_SECURITY_WEP ? "networkKey" : "passPhrase");
        reach_wifi_xml_write(&writer, "</keyType>\r\n\t\t\t\t<protected>false</protected>\r\n\t\t\t"
                                      "\t<keyMaterial>");
        reach_wifi_xml_write_escaped(&writer, key);
        reach_wifi_xml_write(&writer, "</keyMaterial>\r\n\t\t\t</sharedKey>\r\n");
    }

    reach_wifi_xml_write(&writer, "\t\t</security>\r\n\t</MSM>\r\n</WLANProfile>\r\n");

    if (writer.overflow)
    {
        out_xml[0] = 0;
        return 0;
    }
    return writer.length;
}
