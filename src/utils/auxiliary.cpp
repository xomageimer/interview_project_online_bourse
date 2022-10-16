#include "auxiliary.h"

#include <boost/compute/detail/sha1.hpp>
#include <boost/function_output_iterator.hpp>

std::string GetSHA1(const std::string& p_arg)
{
    boost::uuids::detail::sha1 sha1;
    sha1.process_bytes(p_arg.data(), p_arg.size());
    unsigned hash[5] = {0};
    sha1.get_digest(hash);

    // Back to string
    char buf[41] = {0};

    for (int i = 0; i < 5; i++)
    {
        std::sprintf(buf + (i << 3), "%08x", hash[i]);
    }

    return std::string(buf);
}

std::string unquoted(const std::string & str) {
    if (str.size() > 1 && (str.front() == '\'' || str.front() == '\"')
        && (str.back() == '\'' || str.back() == '\"'))
        return str.substr(1, str.size()-2);

    return str;
}

std::time_t toUnixTime(const std::string &str) {
    std::tm t{};
    std::istringstream ss(str);

    ss >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
    return mktime(&t);
}

random_generator &random_generator::Random() {
    static random_generator rg;
    return rg;
}

random_generator::random_generator() : std::mt19937_64(std::random_device{}()) {}