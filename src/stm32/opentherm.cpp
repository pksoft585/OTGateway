#include "opentherm.h"

OpenTherm::OpenTherm(uint8_t inPin, uint8_t outPin, uint8_t bootPin, uint8_t resetPin) :
    Serial1(1),
    status(OpenThermStatus::NOT_INITIALIZED),
    inPin(inPin),
    outPin(outPin),
    bootPin(bootPin),
    resetPin(resetPin)
{
    response = 0;
    responseStatus = OpenThermResponseStatus::NONE;
}

OpenTherm::~OpenTherm()
{
    end();
}

bool OpenTherm::begin()
{
    delayedInitialize();
    status = OpenThermStatus::READY;
    return true;
}

void OpenTherm::delayedInitialize()
{
    if (initialized_)
    {
        return;
    }
    initialized_ = true;

    delay(1000);

    Serial1.begin(200000, outPin, inPin);

    pinMode(resetPin, OUTPUT);
    pinMode(bootPin, OUTPUT);

    digitalWrite(resetPin, LOW);
    digitalWrite(bootPin, LOW);
    digitalWrite(resetPin, HIGH);

    m_Serializer.On<stm32::OtCommandResponse>(
        [&](const stm32::OtCommandResponse* resp)
        {
            response = resp->Payload;

            responseStatus =
                isValidResponse(response)
                    ? OpenThermResponseStatus::SUCCESS
                    : OpenThermResponseStatus::INVALID;

            status = OpenThermStatus::READY;
        });

    delay(1000);
}

void OpenTherm::process()
{
    while (Serial1.available())
    {
        m_Serializer.OnNextByte(Serial1.read());
    }
}

void OpenTherm::end()
{
    Serial1.end();
    initialized_ = false;
    status = OpenThermStatus::NOT_INITIALIZED;
}

bool OpenTherm::isReady()
{
    return status == OpenThermStatus::READY;
}

unsigned long OpenTherm::sendRequest(unsigned long request)
{
    if (!sendRequestAsync(request))
    {
        responseStatus = OpenThermResponseStatus::INVALID;
        return 0;
    }

    uint32_t start = millis();

    while (status != OpenThermStatus::READY)
    {
        process();

        if ((millis() - start) > 1000)
        {
            responseStatus = OpenThermResponseStatus::TIMEOUT;
            status = OpenThermStatus::READY;
            return 0;
        }

        delay(1);
    }

    return response;
}

bool OpenTherm::sendRequestAsync(unsigned long request)
{
    if (!initialized_)
    {
        delayedInitialize();
    }

    if (!isReady())
    {
        return false;
    }

    response = 0;
    responseStatus = OpenThermResponseStatus::NONE;

    stm32::OtCommandRequest req{
        .Payload = request
    };

    uint8_t txBuffer[32];

    int len =
        m_Serializer.prepareRequest(
            &req,
            txBuffer);

    Serial1.write(txBuffer, len);

    status = OpenThermStatus::RESPONSE_WAITING;

    return true;
}

unsigned long OpenTherm::getLastResponse()
{
    return response;
}

OpenThermResponseStatus
OpenTherm::getLastResponseStatus()
{
    return responseStatus;
}

OpenThermMessageType
OpenTherm::getMessageType(unsigned long message)
{
    return (OpenThermMessageType)
        ((message >> 28) & 0x7);
}

OpenThermMessageID
OpenTherm::getDataID(unsigned long frame)
{
    return (OpenThermMessageID)
        ((frame >> 16) & 0xFF);
}

bool OpenTherm::isValidResponse(
    unsigned long response)
{
    OpenThermMessageType type =
        getMessageType(response);

    return
        type == OpenThermMessageType::READ_ACK ||
        type == OpenThermMessageType::WRITE_ACK ||
        type == OpenThermMessageType::DATA_INVALID ||
        type == OpenThermMessageType::UNKNOWN_DATA_ID;
}

unsigned long OpenTherm::buildRequest(
    OpenThermMessageType type,
    OpenThermMessageID id,
    unsigned int data)
{
    unsigned long request = 0;

    request |=
        ((unsigned long)type & 0x7) << 28;

    request |=
        ((unsigned long)id & 0xFF) << 16;

    request |=
        data & 0xFFFF;

    return request;
}

uint16_t OpenTherm::getUInt(
    const unsigned long response)
{
    return response & 0xFFFF;
}

float OpenTherm::getFloat(
    const unsigned long response)
{
    uint16_t data = getUInt(response);
    return (float)data / 256.0f;
}

unsigned int OpenTherm::temperatureToData(
    float temperature)
{
    return (unsigned int)(temperature * 256.0f);
}

bool OpenTherm::isFault(unsigned long response)
{
    return response & 0x1;
}

bool OpenTherm::isCentralHeatingActive(
    unsigned long response)
{
    return response & 0x2;
}

bool OpenTherm::isHotWaterActive(
    unsigned long response)
{
    return response & 0x4;
}

bool OpenTherm::isFlameOn(
    unsigned long response)
{
    return response & 0x8;
}

bool OpenTherm::isCoolingActive(
    unsigned long response)
{
    return response & 0x10;
}

bool OpenTherm::isDiagnostic(
    unsigned long response)
{
    return response & 0x40;
}

unsigned long OpenTherm::setBoilerStatus(
    bool enableCentralHeating,
    bool enableHotWater,
    bool enableCooling,
    bool enableOutsideTemperatureCompensation,
    bool enableCentralHeating2,
    bool summerWinterMode,
    bool dhwBlocking,
    uint8_t lb)
{
    uint8_t hb = 0;

    if (enableCentralHeating)
        hb |= 1 << 0;

    if (enableHotWater)
        hb |= 1 << 1;

    if (enableCooling)
        hb |= 1 << 2;

    if (enableOutsideTemperatureCompensation)
        hb |= 1 << 3;

    if (enableCentralHeating2)
        hb |= 1 << 4;

    if (summerWinterMode)
        hb |= 1 << 5;

    if (dhwBlocking)
        hb |= 1 << 6;

    uint16_t data =
        ((uint16_t)hb << 8) | lb;

    return buildRequest(
        OpenThermMessageType::READ_DATA,
        OpenThermMessageID::Status,
        data);
}

const char* OpenTherm::statusToString(
    OpenThermResponseStatus status)
{
    switch (status)
    {
        case OpenThermResponseStatus::NONE:
            return "NONE";

        case OpenThermResponseStatus::SUCCESS:
            return "SUCCESS";

        case OpenThermResponseStatus::INVALID:
            return "INVALID";

        case OpenThermResponseStatus::TIMEOUT:
            return "TIMEOUT";

        default:
            return "UNKNOWN";
    }
}

const char* OpenTherm::messageTypeToString(
    OpenThermMessageType type)
{
    switch (type)
    {
        case OpenThermMessageType::READ_DATA:
            return "READ_DATA";

        case OpenThermMessageType::WRITE_DATA:
            return "WRITE_DATA";

        case OpenThermMessageType::INVALID_DATA:
            return "INVALID_DATA";

        case OpenThermMessageType::READ_ACK:
            return "READ_ACK";

        case OpenThermMessageType::WRITE_ACK:
            return "WRITE_ACK";

        case OpenThermMessageType::DATA_INVALID:
            return "DATA_INVALID";

        case OpenThermMessageType::UNKNOWN_DATA_ID:
            return "UNKNOWN_DATA_ID";

        default:
            return "UNKNOWN";
    }
}