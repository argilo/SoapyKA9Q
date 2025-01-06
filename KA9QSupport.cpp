#include <SoapySDR/Device.hpp>
#include <SoapySDR/Registry.hpp>

/***********************************************************************
 * Device interface
 **********************************************************************/
class KA9Q : public SoapySDR::Device
{
    //Implement constructor with device specific arguments...

    //Implement all applicable virtual methods from SoapySDR::Device
};

/***********************************************************************
 * Find available devices
 **********************************************************************/
SoapySDR::KwargsList findKA9Q(const SoapySDR::Kwargs &args)
{
    (void)args;
    //locate the device on the system...
    //return a list of 0, 1, or more argument maps that each identify a device

    return SoapySDR::KwargsList();
}

/***********************************************************************
 * Make device instance
 **********************************************************************/
SoapySDR::Device *makeKA9Q(const SoapySDR::Kwargs &args)
{
    (void)args;
    //create an instance of the device object given the args
    //here we will translate args into something used in the constructor
    return new KA9Q();
}

/***********************************************************************
 * Registration
 **********************************************************************/
static SoapySDR::Registry registerKA9Q("ka9q", &findKA9Q, &makeKA9Q, SOAPY_SDR_ABI_VERSION);
