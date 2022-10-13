#include "auxiliary.h"

#include <boost/compute/detail/sha1.hpp>
#include <boost/function_output_iterator.hpp>

std::string GetSHA1(const std::string &p_arg) {
    boost::uuids::detail::sha1 sha1;
    sha1.process_bytes(p_arg.data(), p_arg.size());
    unsigned hash[5] = {0};
    sha1.get_digest(hash);

    union value_type {
        unsigned full;
        unsigned char u8[sizeof(unsigned)];
    } dest{};
    for (auto &el : hash) {
        value_type source{};
        source.full = el;

        for (size_t k = 0; k < sizeof(unsigned); k++) {
            dest.u8[k] = source.u8[sizeof(unsigned) - k - 1];
        }
        el = dest.full;
    }

    char str_hash[sizeof(unsigned) * 5];

    memcpy(str_hash, (char *)&hash, sizeof(unsigned) * 5);
    return {str_hash, std::size(str_hash)};
}

random_generator &random_generator::Random() {
    static random_generator rg;
    return rg;
}

random_generator::random_generator() : std::mt19937_64(std::random_device{}()) {}