#pragma once
#include <windows.h>
#include <iphlpapi.h>
#include <cstddef>
#include <cstdlib>
#include <string>

inline std::string NetworkRowDescription(const MIB_IFROW &row)
{
    const size_t bound = row.dwDescrLen < sizeof(row.bDescr) ? row.dwDescrLen : sizeof(row.bDescr);
    size_t length{};
    while (length < bound && row.bDescr[length] != 0)
        ++length;
    return std::string(reinterpret_cast<const char *>(row.bDescr), length);
}

// Keep allocated capacity separate from the size returned by GetIfTable.
// A failed query never makes its partially filled buffer a valid sample.
template <typename Query = decltype(&GetIfTable)>
DWORD ReadNetworkTable(MIB_IFTABLE *&table, DWORD &capacity, bool &resized, Query query = &GetIfTable)
{
    resized = false;
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        DWORD required = table == nullptr ? 0 : capacity;
        DWORD result = query(table, &required, FALSE);
        if (result == NO_ERROR)
        {
            const size_t header = offsetof(MIB_IFTABLE, table);
            if (table == nullptr || capacity < header || table->dwNumEntries > (capacity - header) / sizeof(MIB_IFROW))
                return ERROR_INVALID_DATA;
        }
        if (result != ERROR_INSUFFICIENT_BUFFER)
            return result;
        if (required < offsetof(MIB_IFTABLE, table))
            return ERROR_INVALID_DATA;
        auto replacement = static_cast<MIB_IFTABLE *>(std::malloc(required));
        if (replacement == nullptr)
            return ERROR_NOT_ENOUGH_MEMORY;
        std::free(table);
        table = replacement;
        capacity = required;
        resized = true;
    }
    return ERROR_INSUFFICIENT_BUFFER;
}
