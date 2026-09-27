// SPDX-License-Identifier: GPL-3.0-or-later
// Runtime qualification only; no game objects or assets.
#include <cstdio>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>
#include <stdexcept>
#include <dpmi.h>

template<class T> concept IntegerLike = requires(T t) { t + 1; };
static_assert(IntegerLike<int>);
static_assert(sizeof(void *) == 4);

int main()
{
    __dpmi_free_mem_info mem{};
    if(__dpmi_get_free_memory_information(&mem) != 0)
        return 1;
    std::printf("probe=djgpp_runtime\ncpp=%ld\npointer_bytes=%u\n",
        static_cast<long>(__cplusplus), static_cast<unsigned>(sizeof(void *)));
    std::printf("largest_free_block_bytes=%lu\nfree_physical_pages=%lu\n",
        mem.largest_available_free_block_in_bytes, mem.total_number_of_free_pages);
    std::vector<std::uint32_t> words(1024 * 1024);
    std::span<std::uint32_t> span(words);
    for(std::size_t i = 0; i < span.size(); ++i)
        span[i] = static_cast<std::uint32_t>(i ^ 0x13579BDF);
    for(std::size_t i = 0; i < span.size(); ++i)
        if(span[i] != static_cast<std::uint32_t>(i ^ 0x13579BDF))
            return 2;
    std::map<std::string, int> names{{"Sparrow", 1}, {"Shuttle", 2}};
    if(names.at("Shuttle") != 2)
        return 3;
    try { throw std::runtime_error("expected"); }
    catch(const std::runtime_error &) { std::puts("exceptions=pass"); }
    std::puts("allocation_touch_bytes=4194304\nstatus=ok");
}
