#include <SoapySDR/Device.hpp>
#include <SoapySDR/Registry.hpp>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <thread>

extern "C" {
char const *App_path = "foo";
#include "multicast.h"
#include "status.h"
}

/***********************************************************************
 * Device interface
 **********************************************************************/
class KA9Q : public SoapySDR::Device
{
private:
    std::string m_status;
    std::string m_data;
    uint32_t m_ssrc;

    double m_sampleRate = 1000000;
    int m_Status_sock = -1;
    int m_Control_sock = -1;
    int m_Input_fd = -1;
    uint8_t m_buffer[PKTSIZE];
    uint8_t *m_dp = nullptr;
    int m_bufferLen = 0;
    int m_bufferOffset = 0;
    std::chrono::time_point<std::chrono::steady_clock> m_lastCommandTime{};

    void sendCommand(const void *buf, int len)
    {
        std::this_thread::sleep_until(m_lastCommandTime + std::chrono::milliseconds(25));

        if (send(m_Control_sock, buf, len, 0) != len) {
            // TODO
        }

        m_lastCommandTime = std::chrono::steady_clock::now();
    }

public:
    KA9Q(const SoapySDR::Kwargs &args)
    {
        m_status = args.at("status");
        m_data = args.at("data");
        m_ssrc = std::stoi(args.at("ssrc"));

        std::cout << m_status << " " << m_data << " " << m_ssrc << std::endl;
    }

    ~KA9Q() {}

    std::string getDriverKey(void) const
    {
        return "KA9Q";
    }

    std::string getHardwareKey(void) const
    {
        return "KA9Q";
    }

    size_t getNumChannels(const int direction) const
    {
        return (direction == SOAPY_SDR_RX) ? 1 : 0;
    }

    bool getFullDuplex(const int direction, const size_t channel) const
    {
        (void)direction;
        (void)channel;

        return false;
    }

    void setSampleRate(const int direction, const size_t channel, const double rate)
    {
        (void)direction;
        (void)channel;

        uint8_t cmd_buffer[PKTSIZE];
        uint8_t *bp = cmd_buffer;
        *bp++ = 1; // Generate command packet
        uint32_t sent_tag = arc4random();
        encode_int(&bp, COMMAND_TAG, sent_tag);
        encode_int(&bp, OUTPUT_SSRC, m_ssrc);
        encode_int(&bp, OUTPUT_SAMPRATE, (int)rate);
        encode_float(&bp, LOW_EDGE, (int)(rate * -0.49));
        encode_float(&bp, HIGH_EDGE, (int)(rate * 0.49));

        float Gain = 0;
        encode_float(&bp, GAIN, Gain);
        encode_int(&bp, AGC_ENABLE, false);
        // encode_int(&bp, AGC_ENABLE, true);

        enum encoding Encoding = F32LE;
        encode_int(&bp, OUTPUT_ENCODING, Encoding);

        encode_eol(&bp);

        int cmd_len = bp - cmd_buffer;
        sendCommand(cmd_buffer, cmd_len);

        m_sampleRate = rate;
    }

    double getSampleRate(const int direction, const size_t channel) const
    {
        (void)direction;
        (void)channel;

        return m_sampleRate;
    }

    std::vector<double> listSampleRates(const int direction, const size_t channel) const
    {
        (void)direction;
        (void)channel;

        std::vector<double> results;

        results.push_back(1000000);
        results.push_back(2000000);
        results.push_back(3000000);
        results.push_back(4000000);
        results.push_back(5000000);
        results.push_back(6000000);
        results.push_back(8000000);
        results.push_back(10000000);

        return results;
    }

    SoapySDR::RangeList getSampleRateRange(const int direction, const size_t channel) const
    {
        (void)direction;
        (void)channel;

        SoapySDR::RangeList results;

        results.push_back(SoapySDR::Range(200, 100000000));

        return results;
    }

    SoapySDR::Stream *setupStream(
        const int direction,
        const std::string &format,
        const std::vector<size_t> &channels = std::vector<size_t>(),
        const SoapySDR::Kwargs &args = SoapySDR::Kwargs())
    {
        (void)direction;
        (void)format;
        (void)channels;
        (void)args;

        struct sockaddr_storage Control_address;
        char iface[1024];
        resolve_mcast(m_status.c_str(), &Control_address, DEFAULT_STAT_PORT, iface, sizeof(iface), 0);

        m_Status_sock = listen_mcast(&Control_address, iface);
        if (m_Status_sock == -1) {
            // TODO
        }

        int Mcast_ttl = 1;
        int IP_tos = 0;
        m_Control_sock = connect_mcast(&Control_address, iface, Mcast_ttl, IP_tos);
        if (m_Control_sock == -1) {
            // TODO
        }

        uint8_t cmd_buffer[PKTSIZE];
        uint8_t *bp = cmd_buffer;
        *bp++ = 1; // Generate command packet
        uint32_t sent_tag = arc4random();
        encode_int(&bp, COMMAND_TAG, sent_tag);
        encode_int(&bp, OUTPUT_SSRC, m_ssrc);
        const char *Mode = "iq";
        encode_string(&bp, PRESET, Mode, strlen(Mode));
        encode_eol(&bp);

        int cmd_len = bp - cmd_buffer;
        sendCommand(cmd_buffer, cmd_len);

        m_Input_fd = setup_mcast_in(m_data.c_str(), NULL, 0, 0);
        if (m_Input_fd == -1) {
            // TODO
        }

        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // TODO: Set this appropriately
        if (setsockopt(m_Input_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            // TODO
        }

        return (SoapySDR::Stream *) this;
    }

    void closeStream(SoapySDR::Stream *stream)
    {
        (void)stream;

        uint8_t cmd_buffer[PKTSIZE];
        uint8_t *bp = cmd_buffer;
        *bp++ = 1; // Generate command packet
        uint32_t sent_tag = arc4random();
        encode_int(&bp, COMMAND_TAG, sent_tag);
        encode_int(&bp, OUTPUT_SSRC, m_ssrc);
        encode_double(&bp, RADIO_FREQUENCY, 0);
        encode_eol(&bp);

        int cmd_len = bp - cmd_buffer;
        sendCommand(cmd_buffer, cmd_len);

        m_Input_fd = -1;
    }

    int activateStream(
        SoapySDR::Stream *stream,
        const int flags = 0,
        const long long timeNs = 0,
        const size_t numElems = 0)
    {
        (void)stream;
        (void)flags;
        (void)timeNs;
        (void)numElems;

        return 0;
    }

    int deactivateStream(
        SoapySDR::Stream *stream,
        const int flags = 0,
        const long long timeNs = 0)
    {
        (void)stream;
        (void)flags;
        (void)timeNs;

        return 0;
    }

    int readStream(
        SoapySDR::Stream *stream,
        void * const *buffs,
        const size_t numElems,
        int &flags,
        long long &timeNs,
        const long timeoutUs = 100000)
    {
        (void)stream;
        (void)flags;
        (void)timeNs;
        (void)timeoutUs;

        float *out = (float *) buffs[0];
        size_t outOffset = 0;

        struct sockaddr sender;
        socklen_t socksize = sizeof(sender);

        auto startTime = std::chrono::steady_clock::now();
        while (outOffset < numElems * 2) {
            // TODO: Choose timeout appropriately
            if (std::chrono::steady_clock::now() > startTime + std::chrono::microseconds(100000)) {
                return outOffset / 2;
            }

            if (m_bufferOffset < m_bufferLen) {
                int toCopy = std::min(m_bufferLen - m_bufferOffset, (int)(2 * numElems - outOffset));
                std::memcpy(out, m_dp, toCopy * sizeof(float));
                out += toCopy;
                m_dp += (toCopy * sizeof(float));
                outOffset += toCopy;
                m_bufferOffset += toCopy;
            } else {
                int size = recvfrom(m_Input_fd, m_buffer, sizeof(m_buffer), 0, &sender, &socksize);
                if (size == -1) {
                    // TODO
                    continue;
                }

                if (size < RTP_MIN_SIZE) {
                    // TODO
                    continue;
                }

                struct rtp_header rtp;
                m_dp = (uint8_t *) ntoh_rtp(&rtp, m_buffer);

                size -= m_dp - m_buffer;
                if (rtp.pad) {
                    // Remove padding
                    size -= m_dp[size-1];
                    rtp.pad = 0;
                }
                if(size <= 0) {
                    // TODO
                    continue;
                }

                if(rtp.ssrc != m_ssrc) {
                    // TODO
                    continue;
                }

                m_bufferLen = size / sizeof(float);
                m_bufferOffset = 0;
            }
        }
        return numElems;
    }

    void setFrequency(
        const int direction,
        const size_t channel,
        const double frequency,
        const SoapySDR::Kwargs &args = SoapySDR::Kwargs())
    {
        setFrequency(direction, channel, "RF", frequency, args);
    }

    void setFrequency(
        const int direction,
        const size_t channel,
        const std::string &name,
        const double frequency,
        const SoapySDR::Kwargs &args = SoapySDR::Kwargs())
    {
        (void)direction;
        (void)channel;
        (void)args;

        if (name == "RF")
        {
            uint8_t cmd_buffer[PKTSIZE];
            uint8_t *bp = cmd_buffer;
            *bp++ = 1; // Generate command packet
            uint32_t sent_tag = arc4random();
            encode_int(&bp, COMMAND_TAG, sent_tag);
            encode_int(&bp, OUTPUT_SSRC, m_ssrc);
            encode_double(&bp, RADIO_FREQUENCY, frequency); // Hz
            encode_eol(&bp);

            int cmd_len = bp - cmd_buffer;
            sendCommand(cmd_buffer, cmd_len);
        }
    }
};

/***********************************************************************
 * Find available devices
 **********************************************************************/
SoapySDR::KwargsList findKA9Q(const SoapySDR::Kwargs &args)
{
    (void)args;

    std::srand(std::time(nullptr));
    int ssrc = std::rand();

    SoapySDR::KwargsList results;

    SoapySDR::Kwargs devInfo;
    devInfo["status"] = "hf.local";
    devInfo["data"] = "foobar.local";
    devInfo["ssrc"] = std::to_string(ssrc);
    devInfo["label"] = "KA9Q-Radio (" + devInfo["status"] + ", " + devInfo["data"] + ", " + devInfo["ssrc"] + ")";

    results.push_back(devInfo);

    return results;
}

/***********************************************************************
 * Make device instance
 **********************************************************************/
SoapySDR::Device *makeKA9Q(const SoapySDR::Kwargs &args)
{
    return new KA9Q(args);
}

/***********************************************************************
 * Registration
 **********************************************************************/
static SoapySDR::Registry registerKA9Q("ka9q", &findKA9Q, &makeKA9Q, SOAPY_SDR_ABI_VERSION);
