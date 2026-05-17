#include "reach/platform/windows_adapters.h"

#include <new>

struct reach_search_provider {
    int has_query;
};

static reach_result reach_search_stub_query(reach_search_provider *provider, const uint16_t *query)
{
    if (provider == nullptr || query == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    provider->has_query = query[0] != 0;
    return REACH_OK;
}

static size_t reach_search_stub_result_count(const reach_search_provider *provider)
{
    return provider != nullptr && provider->has_query ? 1 : 0;
}

static reach_result reach_search_stub_result_at(const reach_search_provider *provider, size_t index, reach_search_result *out_result)
{
    if (provider == nullptr || out_result == nullptr || !provider->has_query || index != 0) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_result = {};
    out_result->id = 1;
    const uint16_t text[] = {
        'w','a','i','t','i','n','g',' ','f','o','r',' ',
        'a','p','i',' ','i','m','p','l','e','m','e','n','t','a','t','i','o','n',0
    };
    for (size_t i = 0; text[i] != 0 && i < 127; ++i) {
        out_result->title[i] = text[i];
    }
    return REACH_OK;
}

static void reach_search_stub_destroy(reach_search_provider *provider)
{
    delete provider;
}

reach_result reach_windows_create_search_stub(reach_search_provider_port *out_port)
{
    if (out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_search_provider *provider = new (std::nothrow) reach_search_provider();
    if (provider == nullptr) {
        return REACH_ERROR;
    }

    out_port->provider = provider;
    out_port->ops.query = reach_search_stub_query;
    out_port->ops.result_count = reach_search_stub_result_count;
    out_port->ops.result_at = reach_search_stub_result_at;
    out_port->ops.destroy = reach_search_stub_destroy;
    return REACH_OK;
}
