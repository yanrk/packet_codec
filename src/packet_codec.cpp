/********************************************************
 * Description : packet codec means divide and unify
 * Author      : yanrk
 * Email       : yanrkchina@163.com
 * Version     : 1.0
 * History     :
 * Copyright(C): 2019-2020
 ********************************************************/

#ifdef _MSC_VER
    #include <windows.h>
#else
    #include <sys/time.h>
#endif // _MSC_VER

#include <ctime>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <list>
#include <vector>
#include <algorithm>

#include "packet_codec.h"

static void byte_order_convert(void * obj, size_t size)
{
    assert(nullptr != obj);

    static union
    {
        unsigned short us;
        unsigned char  uc[sizeof(unsigned short)];
    } un;
    un.us = 0x0001;

    if (0x01 == un.uc[0])
    {
        unsigned char * bytes = static_cast<unsigned char *>(obj);
        for (size_t i = 0; i < size / 2; ++i)
        {
            unsigned char temp = bytes[i];
            bytes[i] = bytes[size - 1 - i];
            bytes[size - 1 - i] = temp;
        }
    }
}

static void host_to_net(void * obj, size_t size)
{
    byte_order_convert(obj, size);
}

static void net_to_host(void * obj, size_t size)
{
    byte_order_convert(obj, size);
}

#pragma pack(push, 1)

struct block_t
{
    uint64_t                            group_index;
    uint32_t                            group_bytes;
    uint32_t                            block_pos;
    uint32_t                            block_index;
    uint32_t                            block_count;
    uint32_t                            block_bytes;

    void encode()
    {
        host_to_net(&group_index, sizeof(group_index));
        host_to_net(&group_bytes, sizeof(group_bytes));
        host_to_net(&block_pos, sizeof(block_pos));
        host_to_net(&block_index, sizeof(block_index));
        host_to_net(&block_count, sizeof(block_count));
        host_to_net(&block_bytes, sizeof(block_bytes));
    }

    void decode()
    {
        net_to_host(&group_index, sizeof(group_index));
        net_to_host(&group_bytes, sizeof(group_bytes));
        net_to_host(&block_pos, sizeof(block_pos));
        net_to_host(&block_index, sizeof(block_index));
        net_to_host(&block_count, sizeof(block_count));
        net_to_host(&block_bytes, sizeof(block_bytes));
    }
};

#pragma pack(pop)

struct group_head_t
{
    uint64_t                            group_index;
    uint32_t                            group_bytes;
    uint32_t                            need_block_count;
    uint32_t                            recv_block_count;

    group_head_t()
        : group_index(0)
        , group_bytes(0)
        , need_block_count(0)
        , recv_block_count(0)
    {

    }
};

struct group_body_t
{
    std::vector<uint8_t>                block_bitmap;
    std::vector<uint8_t>                group_data;
};

struct group_t
{
    group_head_t                        head;
    group_body_t                        body;
};

struct decode_timer_t
{
    uint64_t                            group_index;
    uint32_t                            decode_seconds;
    uint32_t                            decode_microseconds;
};

struct groups_t
{
    uint64_t                            min_group_index;
    uint64_t                            new_group_index;
    std::map<uint64_t, group_t>         group_items;
    std::list<decode_timer_t>           decode_timer_list;

    groups_t()
        : min_group_index(0)
        , new_group_index(0)
        , group_items()
        , decode_timer_list()
    {

    }

    void reset()
    {
        min_group_index = 0;
        new_group_index = 0;
        group_items.clear();
        decode_timer_list.clear();
    }
};

static void get_current_time(uint32_t & seconds, uint32_t & microseconds)
{
#ifdef _MSC_VER
    SYSTEMTIME sys_now = { 0x0 };
    GetLocalTime(&sys_now);
    seconds = static_cast<uint32_t>(time(nullptr));
    microseconds = static_cast<uint32_t>(sys_now.wMilliseconds * 1000);
#else
    struct timeval tv_now = { 0x0 };
    gettimeofday(&tv_now, nullptr);
    seconds = static_cast<uint32_t>(tv_now.tv_sec);
    microseconds = static_cast<uint32_t>(tv_now.tv_usec);
#endif // _MSC_VER
}

static bool packet_divide(const uint8_t * src_data, uint32_t src_size, uint32_t max_block_size, uint64_t & group_index, std::list<std::vector<uint8_t>> & dst_list)
{
    if (nullptr == src_data || 0 == src_size)
    {
        return (false);
    }

    if (max_block_size <= sizeof(block_t))
    {
        return (false);
    }

    uint32_t max_block_bytes = static_cast<uint32_t>(max_block_size - sizeof(block_t));
    uint32_t group_bytes = src_size;
    uint32_t block_pos = 0;
    uint32_t block_index = 0;
    uint32_t block_count = (group_bytes + max_block_bytes - 1) / max_block_bytes;

    while (0 != src_size)
    {
        uint32_t block_bytes = std::min<uint32_t>(max_block_bytes, src_size);

        block_t block = { 0x0 };
        block.group_index = group_index;
        block.group_bytes = group_bytes;
        block.block_pos = block_pos;
        block.block_index = block_index;
        block.block_count = block_count;
        block.block_bytes = block_bytes;
        block.encode();

        std::vector<uint8_t> buffer(static_cast<uint32_t>(sizeof(block) + block_bytes), 0x0);
        memcpy(&buffer[0], &block, sizeof(block));
        memcpy(&buffer[sizeof(block)], src_data, block_bytes);

        dst_list.emplace_back(std::move(buffer));

        src_data += block_bytes;
        src_size -= block_bytes;
        block_pos += block_bytes;

        ++block_index;
    }

    ++group_index;

    return (true);
}

static bool insert_group_block(const void * data, uint32_t size, groups_t & groups, uint32_t max_delay_microseconds)
{
    block_t block = *reinterpret_cast<const block_t *>(data);
    block.decode();

    if (sizeof(block) + block.block_bytes != size)
    {
        return (false);
    }

    if (block.block_index >= block.block_count || block.block_pos + block.block_bytes > block.group_bytes)
    {
        return (false);
    }

    if (block.group_index < groups.min_group_index)
    {
        return (false);
    }

    groups.new_group_index = block.group_index;

    group_t & group = groups.group_items[groups.new_group_index];
    group_head_t & group_head = group.head;
    group_body_t & group_body = group.body;

    if (0 == group_head.recv_block_count)
    {
        group_head.group_index = block.group_index;
        group_head.group_bytes = block.group_bytes;
        group_head.need_block_count = block.block_count;
        group_head.recv_block_count += 1;

        group_body.block_bitmap.resize((block.block_count + 7) / 8, 0x0);
        group_body.block_bitmap[block.block_index >> 3] |= (1 << (block.block_index & 7));
        group_body.group_data.resize(block.group_bytes, 0x0);
        memcpy(&group_body.group_data[block.block_pos], reinterpret_cast<const uint8_t *>(data) + sizeof(block), size - sizeof(block));

        decode_timer_t decode_timer = { 0x0 };
        decode_timer.group_index = block.group_index;
        get_current_time(decode_timer.decode_seconds, decode_timer.decode_microseconds);
        decode_timer.decode_microseconds += max_delay_microseconds * (group_head.need_block_count / 100 + 1);
        decode_timer.decode_seconds += decode_timer.decode_microseconds / 1000000;
        decode_timer.decode_microseconds %= 1000000;

        groups.decode_timer_list.push_back(decode_timer);
    }
    else if (group_head.recv_block_count < group_head.need_block_count)
    {
        if (block.group_index != group_head.group_index || block.group_bytes != group_head.group_bytes || block.block_count != group_head.need_block_count)
        {
            return (false);
        }

        if (group_body.block_bitmap[block.block_index >> 3] & (1 << (block.block_index & 7)))
        {
            return (false);
        }

        group_head.recv_block_count += 1;

        group_body.block_bitmap[block.block_index >> 3] |= (1 << (block.block_index & 7));
        memcpy(&group_body.group_data[block.block_pos], reinterpret_cast<const uint8_t *>(data) + sizeof(block), size - sizeof(block));
    }

    return (true);
}

static void remove_expired_blocks(groups_t & groups)
{
    std::map<uint64_t, group_t> & group_items = groups.group_items;
    for (std::map<uint64_t, group_t>::iterator iter = group_items.begin(); group_items.end() != iter; iter = group_items.erase(iter))
    {
        if (iter->first >= groups.min_group_index)
        {
            break;
        }
    }
}

static bool packet_unify(const void * data, uint32_t size, groups_t & groups, std::list<std::vector<uint8_t>> & dst_list, uint32_t max_delay_microseconds, double fault_tolerance_rate)
{
    if (nullptr != data && 0 != size)
    {
        if (!insert_group_block(data, size, groups, max_delay_microseconds))
        {
            return (false);
        }

        const group_t & group = groups.group_items[groups.new_group_index];
        if (group.head.recv_block_count != group.head.need_block_count && groups.new_group_index < groups.min_group_index + 3)
        {
            return (false);
        }
    }

    const std::size_t old_dst_list_size = dst_list.size();

    uint32_t current_seconds = 0;
    uint32_t current_microseconds = 0;
    get_current_time(current_seconds, current_microseconds);
    std::list<decode_timer_t>::iterator iter = groups.decode_timer_list.begin();
    while (groups.decode_timer_list.end() != iter)
    {
        const decode_timer_t & decode_timer = *iter;
        group_t & group = groups.group_items[decode_timer.group_index];
        if (group.head.recv_block_count == group.head.need_block_count)
        {
            dst_list.emplace_back(std::move(group.body.group_data));
            groups.group_items.erase(decode_timer.group_index);
            groups.min_group_index = decode_timer.group_index + 1;
            iter = groups.decode_timer_list.erase(iter);
        }
        else if ((decode_timer.decode_seconds < current_seconds) || (decode_timer.decode_seconds == current_seconds && decode_timer.decode_microseconds < current_microseconds))
        {
            if (fault_tolerance_rate > 0.0 && fault_tolerance_rate < 1.0)
            {
                if (group.head.recv_block_count >= static_cast<uint32_t>(group.head.need_block_count * (1.0 - fault_tolerance_rate)))
                {
                    dst_list.emplace_back(std::move(group.body.group_data));
                }
            }
            groups.group_items.erase(decode_timer.group_index);
            groups.min_group_index = decode_timer.group_index + 1;
            iter = groups.decode_timer_list.erase(iter);
        }
        else
        {
            break;
        }
    }

    const std::size_t new_dst_list_size = dst_list.size();

    remove_expired_blocks(groups);

    return (new_dst_list_size > old_dst_list_size);
}

class PacketDividerImpl
{
public:
    PacketDividerImpl(uint32_t max_block_size);
    PacketDividerImpl(const PacketDividerImpl &) = delete;
    PacketDividerImpl(PacketDividerImpl &&) = delete;
    PacketDividerImpl & operator = (const PacketDividerImpl &) = delete;
    PacketDividerImpl & operator = (PacketDividerImpl &&) = delete;
    ~PacketDividerImpl();

public:
    bool encode(const uint8_t * src_data, uint32_t src_size, std::list<std::vector<uint8_t>> & dst_list);

public:
    void reset();

private:
    const uint32_t      m_max_block_size;

private:
    uint64_t            m_group_index;
};

PacketDividerImpl::PacketDividerImpl(uint32_t max_block_size)
    : m_max_block_size(std::max<uint32_t>(max_block_size, sizeof(block_t) + 1))
    , m_group_index(0)
{

}

PacketDividerImpl::~PacketDividerImpl()
{

}

bool PacketDividerImpl::encode(const uint8_t * src_data, uint32_t src_size, std::list<std::vector<uint8_t>> & dst_list)
{
    return (packet_divide(src_data, src_size, m_max_block_size, m_group_index, dst_list));
}

void PacketDividerImpl::reset()
{
    m_group_index = 0;
}

class PacketUnifierImpl
{
public:
    PacketUnifierImpl(uint32_t max_delay_microseconds = 1000 * 15, double fault_tolerance_rate = 0.0);
    PacketUnifierImpl(const PacketUnifierImpl &) = delete;
    PacketUnifierImpl(PacketUnifierImpl &&) = delete;
    PacketUnifierImpl & operator = (const PacketUnifierImpl &) = delete;
    PacketUnifierImpl & operator = (PacketUnifierImpl &&) = delete;
    ~PacketUnifierImpl();

public:
    bool decode(const uint8_t * src_data, uint32_t src_size, std::list<std::vector<uint8_t>> & dst_list);

public:
    void reset();

private:
    const uint32_t      m_max_delay_microseconds;
    const double        m_fault_tolerance_rate;

private:
    groups_t            m_groups;
};

PacketUnifierImpl::PacketUnifierImpl(uint32_t max_delay_microseconds, double fault_tolerance_rate)
    : m_max_delay_microseconds(std::max<uint32_t>(max_delay_microseconds, 500))
    , m_fault_tolerance_rate(std::max<double>(std::min<double>(fault_tolerance_rate, 1.0), 0.0))
    , m_groups()
{

}

PacketUnifierImpl::~PacketUnifierImpl()
{

}

bool PacketUnifierImpl::decode(const uint8_t * src_data, uint32_t src_size, std::list<std::vector<uint8_t>> & dst_list)
{
    return (packet_unify(src_data, src_size, m_groups, dst_list, m_max_delay_microseconds, m_fault_tolerance_rate));
}

void PacketUnifierImpl::reset()
{
    m_groups.reset();
}

PacketDivider::PacketDivider()
    : m_divider(nullptr)
{

}

PacketDivider::~PacketDivider()
{
    exit();
}

bool PacketDivider::init(uint32_t max_block_size)
{
    exit();

    return (nullptr != (m_divider = new PacketDividerImpl(max_block_size)));
}

void PacketDivider::exit()
{
    if (nullptr != m_divider)
    {
        delete m_divider;
        m_divider = nullptr;
    }
}

bool PacketDivider::encode(const uint8_t * src_data, uint32_t src_size, std::list<std::vector<uint8_t>> & dst_list)
{
    return (nullptr != m_divider && m_divider->encode(src_data, src_size, dst_list));
}

void PacketDivider::reset()
{
    if (nullptr != m_divider)
    {
        m_divider->reset();
    }
}

PacketUnifier::PacketUnifier()
    : m_unifier(nullptr)
{

}

PacketUnifier::~PacketUnifier()
{
    exit();
}

bool PacketUnifier::init(uint32_t expire_millisecond, double fault_tolerance_rate)
{
    exit();

    return (nullptr != (m_unifier = new PacketUnifierImpl(expire_millisecond * 1000, fault_tolerance_rate)));
}

void PacketUnifier::exit()
{
    if (nullptr != m_unifier)
    {
        delete m_unifier;
        m_unifier = nullptr;
    }
}

bool PacketUnifier::decode(const uint8_t * src_data, uint32_t src_size, std::list<std::vector<uint8_t>> & dst_list)
{
    return (nullptr != m_unifier && m_unifier->decode(src_data, src_size, dst_list));
}

void PacketUnifier::reset()
{
    if (nullptr != m_unifier)
    {
        m_unifier->reset();
    }
}
