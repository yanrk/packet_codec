/********************************************************
 * Description : udp serialize test
 * Author      : yanrk
 * Email       : yanrkchina@163.com
 * Version     : 1.0
 * History     :
 * Copyright(C): RAYVISION
 ********************************************************/

#ifdef _MSC_VER
    #include <windows.h>
#else
    #include <sys/time.h>
#endif // _MSC_VER

#include <ctime>
#include <iostream>
#include <algorithm>
#include "packet_codec.h"

static void get_system_time(int32_t & seconds, int32_t & microseconds)
{
#ifdef _MSC_VER
    SYSTEMTIME sys_now = { 0x0 };
    GetLocalTime(&sys_now);
    seconds = static_cast<int32_t>(time(nullptr));
    microseconds = static_cast<int32_t>(sys_now.wMilliseconds * 1000);
#else
    struct timeval tv_now = { 0x0 };
    gettimeofday(&tv_now, nullptr);
    seconds = static_cast<int32_t>(tv_now.tv_sec);
    microseconds = static_cast<int32_t>(tv_now.tv_usec);
#endif // _MSC_VER
}

int main()
{
    std::vector<uint8_t> src_data(307608, 0x0);

    srand(static_cast<uint32_t>(time(nullptr)));
    for (std::vector<uint8_t>::iterator iter = src_data.begin(); src_data.end() != iter; ++iter)
    {
        *iter = static_cast<uint8_t>(rand());
    }

    std::list<std::vector<uint8_t>> src_list;

    int32_t s1 = 0;
    int32_t m1 = 0;
    get_system_time(s1, m1);

    PacketDivider divider;
    if (!divider.init(1100))
    {
        return (1);
    }

    if (!divider.encode(&src_data[0], static_cast<uint32_t>(src_data.size()), src_list))
    {
        return (2);
    }

    int32_t s2 = 0;
    int32_t m2 = 0;
    get_system_time(s2, m2);

    int32_t delta12 = (s2 - s1) * 1000 + (m2 - m1) / 1000;
    std::cout << "encode use time " << delta12 << "ms" << std::endl;

    std::list<std::vector<uint8_t>> copy_fore_list;
    std::list<std::vector<uint8_t>> copy_back_list;
    std::list<std::vector<uint8_t>>::iterator iter_fore = src_list.begin();
    std::list<std::vector<uint8_t>>::reverse_iterator iter_back = src_list.rbegin();
    while (src_list.end() != iter_fore && src_list.rend() != iter_back)
    {
        if (0 != rand() % 2)
        {
            std::swap(*iter_fore, *iter_back);
        }
        else if (0 == rand() % 3)
        {
            copy_fore_list.push_back(*iter_fore);
        }
        else if (0 == rand() % 5)
        {
            copy_back_list.push_front(*iter_back);
        }
        ++iter_fore;
        ++iter_back;
    }
    src_list.splice(src_list.begin(), copy_back_list);
    src_list.splice(src_list.end(), copy_fore_list);

    std::list<std::vector<uint8_t>> dst_list;

    int32_t s3 = 0;
    int32_t m3 = 0;
    get_system_time(s3, m3);

    PacketUnifier unifier;
    if (!unifier.init(30))
    {
        return (3);
    }

    for (std::list<std::vector<uint8_t>>::const_iterator iter = src_list.begin(); src_list.end() != iter; ++iter)
    {
        const std::vector<uint8_t> & data = *iter;
        unifier.decode(&data[0], static_cast<uint32_t>(data.size()), dst_list);
    }

    if (1 != dst_list.size())
    {
        return (4);
    }

    int32_t s4 = 0;
    int32_t m4 = 0;
    get_system_time(s4, m4);

    int32_t delta34 = (s4 - s3) * 1000 + (m4 - m3) / 1000;
    std::cout << "decode use time " << delta34 << "ms" << std::endl;

    if (dst_list.front() != src_data)
    {
        return (5);
    }

    std::cout << "ok" << std::endl;

    return (0);
}
