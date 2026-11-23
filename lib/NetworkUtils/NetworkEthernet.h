#pragma once
#include <NetworkUplink.h>

#ifdef NETWORK_HAS_ETHERNET
  #include <ETH.h>
#endif

namespace NetworkUtils {
  /** Ethernet uplink using W5500 over SPI or LAN8720 over RMII. */
  class EthernetUplink : public NetworkUplink {
  public:
    explicit EthernetUplink(UplinkContext* context) : NetworkUplink(context) {}

    void loop() override {
      #ifdef NETWORK_HAS_ETHERNET
      if (this->restartFlag) {
        this->restartFlag = false;
        if (this->started) {
          Log.swarningln(FPSTR(L_NETWORK), F("Ethernet: settings changed, restart required to apply them"));
        }
      }

      if (!this->enabled || !this->hasPins()) {
        if (this->active || this->linkUp) {
          this->active = false;
          this->linkUp = false;
          Log.sinfoln(FPSTR(L_NETWORK), F("Ethernet: disabled, handing the uplink back to WiFi"));
        }
        this->attempt.disarm();
        return;
      }

      if (!this->started) {
        if (this->startUnavailable) return;

        bool started = false;
        switch (this->implementation) {
          case EthernetImplementation::W5500:
            Log.sinfoln(FPSTR(L_NETWORK),
              F("Ethernet: init W5500 (CS: %u, IRQ: %u, RST: %u, CLK: %u, MISO: %u, MOSI: %u)"),
              this->cs, this->irq, this->rst, this->clk, this->miso, this->mosi);
            started = ETH.begin(ETH_PHY_W5500, 1, this->cs, this->irq, this->rst,
              SPI2_HOST, this->clk, this->miso, this->mosi, 12);
            break;

          case EthernetImplementation::LAN8720:
            Log.sinfoln(FPSTR(L_NETWORK),
              F("Ethernet: init LAN8720 (PHY: %u, MDC: %u, MDIO: %u, power: %d)"),
              DEFAULT_LAN8720_PHY_ADDR, DEFAULT_LAN8720_MDC_GPIO,
              DEFAULT_LAN8720_MDIO_GPIO, DEFAULT_LAN8720_POWER_GPIO);
            started = ETH.begin(ETH_PHY_LAN8720,
              DEFAULT_LAN8720_PHY_ADDR, DEFAULT_LAN8720_MDC_GPIO,
              DEFAULT_LAN8720_MDIO_GPIO, DEFAULT_LAN8720_POWER_GPIO,
              lan8720ClockMode(DEFAULT_LAN8720_CLK_MODE));
            break;

          case EthernetImplementation::NONE:
          default:
            this->attempt.disarm();
            return;
        }

        if (!started) {
          this->startUnavailable = true;
          this->attempt.disarm();
          Log.serrorln(FPSTR(L_NETWORK),
            F("Ethernet: initialization failed; check implementation and board wiring, then restart"));
          return;
        }

        this->started = true;
        this->running = true;
        this->prevLinkCheck = 0;
        this->attempt.arm(this->linkGrace);

        if (!this->context->useDhcp) {
          ETH.config(this->context->staticIp, this->context->staticGateway,
            this->context->staticSubnet, this->context->staticDns);
        }
        if (strlen(this->context->hostname) && UplinkContext::isHostnameValid(this->context->hostname)) {
          ETH.setHostname(this->context->hostname);
        }
        return;
      }

      if (millis() - this->prevLinkCheck < this->linkCheckInterval) return;
      this->prevLinkCheck = millis();

      if (!ETH.linkUp()) {
        if (this->linkUp) {
          this->linkUp = false;
          Log.sinfoln(FPSTR(L_NETWORK), F("Ethernet: cable disconnected"));
        }
        this->attempt.disarm();
        if (this->active) {
          this->active = false;
          Log.sinfoln(FPSTR(L_NETWORK), F("Ethernet: link lost, falling back to WiFi"));
          this->context->delayCallback(250);
        }
        return;
      }

      if (!this->linkUp) {
        this->linkUp = true;
        this->attempt.arm(this->leaseGrace);
        Log.sinfoln(FPSTR(L_NETWORK), F("Ethernet: cable connected"));
      }

      if (!ETH.connected() || this->active) return;

      this->active = true;
      this->attempt.disarm();
      Log.sinfoln(FPSTR(L_NETWORK),
        F("Ethernet: using ethernet instead of WiFi, IP: %s, link: %u Mbps, duplex: %u"),
        ETH.localIP().toString().c_str(), ETH.linkSpeed(), ETH.fullDuplex() ? 1 : 0);
      #endif
    }

    bool isActive() override {
      #ifdef NETWORK_HAS_ETHERNET
      return this->enabled && this->started && ETH.linkUp() && ETH.connected();
      #else
      return false;
      #endif
    }

    void setEnabled(bool value) {
      this->enabled = value;
      #ifdef NETWORK_HAS_ETHERNET
      if (this->started && ETH.handle() != nullptr && value != this->running) {
        const esp_err_t ret = value ? esp_eth_start(ETH.handle()) : esp_eth_stop(ETH.handle());
        if (ret == ESP_OK) this->running = value;
        else Log.serrorln(FPSTR(L_NETWORK), F("Ethernet: driver %s failed"), value ? "start" : "stop");
      }
      #endif
    }

    void setImplementation(EthernetImplementation value) {
      if (value != EthernetImplementation::NONE && value != EthernetImplementation::W5500
          && value != EthernetImplementation::LAN8720) return;
      if (value != this->implementation) {
        if (this->started) this->restartFlag = true;
        this->implementation = value;
        this->startUnavailable = false;
      }
    }

    void setPins(uint8_t clk, uint8_t mosi, uint8_t miso,
                 uint8_t cs, uint8_t irq, uint8_t rst) {
      if (clk != this->clk || mosi != this->mosi || miso != this->miso
          || cs != this->cs || irq != this->irq || rst != this->rst) {
        this->markSettingsChanged();
      }
      this->clk = clk;
      this->mosi = mosi;
      this->miso = miso;
      this->cs = cs;
      this->irq = irq;
      this->rst = rst;
    }

    void markSettingsChanged() {
      if (this->started) this->restartFlag = true;
    }

    bool hasPins() const {
      switch (this->implementation) {
        case EthernetImplementation::LAN8720:
          return true;
        case EthernetImplementation::W5500:
          return this->clk != GPIO_IS_NOT_CONFIGURED
            && this->mosi != GPIO_IS_NOT_CONFIGURED
            && this->miso != GPIO_IS_NOT_CONFIGURED
            && this->cs != GPIO_IS_NOT_CONFIGURED
            && this->irq != GPIO_IS_NOT_CONFIGURED
            && this->rst != GPIO_IS_NOT_CONFIGURED;
        case EthernetImplementation::NONE:
        default:
          return false;
      }
    }

    EthernetImplementation getImplementation() const { return this->implementation; }

    IPAddress localIP() override { #ifdef NETWORK_HAS_ETHERNET return ETH.localIP(); #else return IPAddress(); #endif }
    IPAddress subnetMask() override { #ifdef NETWORK_HAS_ETHERNET return ETH.subnetMask(); #else return IPAddress(); #endif }
    IPAddress gatewayIP() override { #ifdef NETWORK_HAS_ETHERNET return ETH.gatewayIP(); #else return IPAddress(); #endif }
    IPAddress dnsIP() override { #ifdef NETWORK_HAS_ETHERNET return ETH.dnsIP(); #else return IPAddress(); #endif }
    String macAddress() override { #ifdef NETWORK_HAS_ETHERNET return ETH.macAddress(); #else return String(); #endif }

  protected:
    static eth_clock_mode_t lan8720ClockMode(EthernetClockMode mode) {
      switch (mode) {
        case EthernetClockMode::GPIO0_IN:   return ETH_CLOCK_GPIO0_IN;
        case EthernetClockMode::GPIO0_OUT:  return ETH_CLOCK_GPIO0_OUT;
        case EthernetClockMode::GPIO16_OUT: return ETH_CLOCK_GPIO16_OUT;
        case EthernetClockMode::GPIO17_OUT:
        default:                            return ETH_CLOCK_GPIO17_OUT;
      }
    }

    const unsigned long linkCheckInterval = 1000;
    const unsigned long linkGrace = 5000;
    const unsigned long leaseGrace = 20000;

    bool enabled = false;
    bool started = false;
    bool running = false;
    bool active = false;
    bool linkUp = false;
    bool restartFlag = false;
    bool startUnavailable = false;

    EthernetImplementation implementation = DEFAULT_ETHERNET_IMPLEMENTATION;

    uint8_t clk = GPIO_IS_NOT_CONFIGURED;
    uint8_t mosi = GPIO_IS_NOT_CONFIGURED;
    uint8_t miso = GPIO_IS_NOT_CONFIGURED;
    uint8_t cs = GPIO_IS_NOT_CONFIGURED;
    uint8_t irq = GPIO_IS_NOT_CONFIGURED;
    uint8_t rst = GPIO_IS_NOT_CONFIGURED;

    unsigned long prevLinkCheck = 0;
  };
}
