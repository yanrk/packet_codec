/********************************************************
 * Description : packet codec means divide and unify
 * Author      : yanrk
 * Email       : yanrkchina@163.com
 * Version     : 1.0
 * History     :
 * Copyright(C): 2019-2020
 ********************************************************/

#ifndef PACKET_CODEC_H
#define PACKET_CODEC_H


#ifdef _MSC_VER
    #define PACKET_CODEC_CDECL            __cdecl
    #ifdef EXPORT_PACKET_CODEC_DLL
        #define PACKET_CODEC_TYPE         __declspec(dllexport)
    #else
        #ifdef USE_PACKET_CODEC_DLL
            #define PACKET_CODEC_TYPE     __declspec(dllimport)
        #else
            #define PACKET_CODEC_TYPE
        #endif // USE_PACKET_CODEC_DLL
    #endif // EXPORT_PACKET_CODEC_DLL
#else
    #define PACKET_CODEC_CDECL
    #define PACKET_CODEC_TYPE
#endif // _MSC_VER

#include <cstdint>
#include <list>
#include <vector>

class PacketDividerImpl;
class PacketUnifierImpl;

class PACKET_CODEC_TYPE PacketDivider
{
public:
    PacketDivider();
    PacketDivider(const PacketDivider &) = delete;
    PacketDivider(PacketDivider &&) = delete;
    PacketDivider & operator = (const PacketDivider &) = delete;
    PacketDivider & operator = (PacketDivider &&) = delete;
    ~PacketDivider();

public:
    bool init(uint32_t max_block_size);
    void exit();

public:
    bool encode(const uint8_t * src_data, uint32_t src_size, std::list<std::vector<uint8_t>> & dst_list);

public:
    void reset();

private:
    PacketDividerImpl    * m_divider;
};

class PACKET_CODEC_TYPE PacketUnifier
{
public:
    PacketUnifier();
    PacketUnifier(const PacketUnifier &) = delete;
    PacketUnifier(PacketUnifier &&) = delete;
    PacketUnifier & operator = (const PacketUnifier &) = delete;
    PacketUnifier & operator = (PacketUnifier &&) = delete;
    ~PacketUnifier();

public:
    bool init(uint32_t expire_millisecond = 15, double fault_tolerance_rate = 0.0);
    void exit();

public:
    bool decode(const uint8_t * src_data, uint32_t src_size, std::list<std::vector<uint8_t>> & dst_list);

public:
    void reset();

private:
    PacketUnifierImpl    * m_unifier;
};


#endif // PACKET_CODEC_H
