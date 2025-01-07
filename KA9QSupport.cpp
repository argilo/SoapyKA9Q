#include <SoapySDR/Device.hpp>
#include <SoapySDR/Registry.hpp>
#include <iostream>

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

    double m_sampleRate = 0;
    int m_Status_sock = -1;
    int m_Control_sock = -1;
    int m_Input_fd = -1;
    uint8_t m_buffer[PKTSIZE];
    uint8_t *m_dp = nullptr;
    int m_bufferLen = 0;
    int m_bufferOffset = 0;

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

        m_sampleRate = rate;
    }

    double getSampleRate(const int direction, const size_t channel) const
    {
        (void)direction;
        (void)channel;

        return m_sampleRate;
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

        m_Input_fd = setup_mcast_in(m_data.c_str(), NULL, 0, 0);
        if (m_Input_fd == -1) {
            // TODO
        }

        return (SoapySDR::Stream *) this;
    }

    void closeStream(SoapySDR::Stream *stream)
    {
        (void)stream;

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

        while (outOffset < numElems * 2) {
            if (m_bufferOffset < m_bufferLen) {
                int16_t samp = (m_dp[2*m_bufferOffset] << 8) | m_dp[2*m_bufferOffset + 1];
                out[outOffset++] = samp / 32768.0;
                m_bufferOffset++;
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

                m_bufferLen = size / 2;
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
        (void)direction;
        (void)channel;
        (void)args;

        uint8_t cmd_buffer[PKTSIZE];
        uint8_t *bp = cmd_buffer;
        *bp++ = 1; // Generate command packet
        uint32_t sent_tag = arc4random();
        encode_int(&bp, COMMAND_TAG, sent_tag);
        encode_int(&bp, OUTPUT_SSRC, m_ssrc);
    	encode_double(&bp, RADIO_FREQUENCY, frequency); // Hz
        encode_eol(&bp);

        int cmd_len = bp - cmd_buffer;
        if (send(m_Control_sock, cmd_buffer, cmd_len, 0) != cmd_len) {
	        // TODO
        }
    }
};

/***********************************************************************
 * Find available devices
 **********************************************************************/
SoapySDR::KwargsList findKA9Q(const SoapySDR::Kwargs &args)
{
    (void)args;

    SoapySDR::KwargsList results;

    SoapySDR::Kwargs devInfo;
    devInfo["label"] = "KA9Q-Radio (hf.local, foobar.local, 1234)";
    devInfo["status"] = "hf.local";
    devInfo["data"] = "foobar.local";
    devInfo["ssrc"] = "1234";

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
