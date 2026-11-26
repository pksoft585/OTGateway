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
          #if defined(CONFIG_ETH_SPI_ETHERNET_W5500) && CONFIG_ETH_SPI_ETHERNET_W5500
          case EthernetImplementation::W5500: {
            Log.sinfoln(
              FPSTR(L_NETWORK),
              F("Ethernet: init W5500 (CS: %u, IRQ: %u, RST: %u, CLK: %u, MISO: %u, MOSI: %u)"),
              this->w5500Cs,
              this->w5500Irq,
              this->w5500Rst,
              this->w5500Clk,
              this->w5500Miso,
              this->w5500Mosi
            );
            started = ETH.begin(ETH_PHY_W5500, 1, this->w5500.cs, this->w5500.irq, this->w5500.rst, SPI2_HOST, this->w5500.clk, this->w5500.miso, this->w5500.mosi, 12);
            break;
          }
          #endif

          case EthernetImplementation::LAN8720: {
            Log.sinfoln(
              FPSTR(L_NETWORK),
              F("Ethernet: init LAN8720 (PHY: %u, MDC: %u, MDIO: %u, power: %d)"),
              this->lan8720Phy,
              this->lan8720Mdc,
              this->lan8720Mdio,
              this->lan8720Power
            );
            started = ETH.begin(ETH_PHY_LAN8720, this->lan8720Phy, this->lan8720Mdc, this->lan8720Mdio, this->lan8720Power, static_cast<eth_clock_mode_t>(this->lan8720Clk));
            break;
          }

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
          ETH.config(this->context->staticIp, this->context->staticGateway, this->context->staticSubnet, this->context->staticDns);
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
        if (ret == ESP_OK) {
          this->running = value;
        } else {
          Log.serrorln(FPSTR(L_NETWORK), F("Ethernet: driver %s failed"), value ? "start" : "stop");
        }
      }
      #endif
    }

    void setImplementation(EthernetImplementation value) {
      if (
        value != EthernetImplementation::NONE
        && value != EthernetImplementation::LAN8720
        #if defined(CONFIG_ETH_SPI_ETHERNET_W5500) && CONFIG_ETH_SPI_ETHERNET_W5500
        && value != EthernetImplementation::W5500
        #endif
      ) {
        return;
      }

      if (value != this->implementation) {
        if (this->started) this->restartFlag = true;
        this->implementation = value;
        this->startUnavailable = false;
      }
    }

    void setPinsW5500(uint8_t clk, uint8_t mosi, uint8_t miso, uint8_t cs, uint8_t irq, uint8_t rst) {
      if (
        clk != this->w5500Clk
        || mosi != this->w5500Mosi
        || miso != this->w5500Miso
        || cs != this->w5500Cs
        || irq != this->w5500Irq
        || rst != this->w5500Rst
      ) {
        this->markSettingsChanged();
        this->startUnavailable = false;
      }

      this->w5500Clk = clk;
      this->w5500Mosi = mosi;
      this->w5500Miso = miso;
      this->w5500Cs = cs;
      this->w5500Irq = irq;
      this->w5500Rst = rst;
    }

    void setPinsLAN8720(
      uint8_t mdc,
      uint8_t mdio,
      uint8_t phy,
      uint8_t power,
      EthernetClockMode clockMode
    ) {
      if (
        mdc != this->lan8720Mdc
        || mdio != this->lan8720Mdio
        || phy != this->lan8720Phy
        || power != this->lan8720Power
        || clockMode != this->lan8720Clk
      ) {
        this->markSettingsChanged();
        this->startUnavailable = false;
      }

      this->lan8720Mdc = mdc;
      this->lan8720Mdio = mdio;
      this->lan8720Phy = phy;
      this->lan8720Power = power;
      this->lan8720Clk = clockMode;
    }

    void markSettingsChanged() {
      if (this->started) this->restartFlag = true;
    }

    bool hasPins() const {
      switch (this->implementation) {
        case EthernetImplementation::LAN8720:
          return this->lan8720Mdc != GPIO_IS_NOT_CONFIGURED
            && this->lan8720Mdio != GPIO_IS_NOT_CONFIGURED
            && this->lan8720Power != GPIO_IS_NOT_CONFIGURED
            && this->lan8720Phy != PARM_IS_NOT_CONFIGURED
            && static_cast<uint8_t>(this->lan8720Clk) != PARM_IS_NOT_CONFIGURED;

        case EthernetImplementation::W5500:
          #if defined(CONFIG_ETH_SPI_ETHERNET_W5500) && CONFIG_ETH_SPI_ETHERNET_W5500
          return this->w5500Clk != GPIO_IS_NOT_CONFIGURED
            && this->w5500Mosi != GPIO_IS_NOT_CONFIGURED
            && this->w5500Miso != GPIO_IS_NOT_CONFIGURED
            && this->w5500Cs != GPIO_IS_NOT_CONFIGURED
            && this->w5500Irq != GPIO_IS_NOT_CONFIGURED
            && this->w5500Rst != GPIO_IS_NOT_CONFIGURED;
          #else
          return false;
          #endif

        case EthernetImplementation::NONE:
        default:
          return false;
      }
    }

    EthernetImplementation getImplementation() const { return this->implementation;
    }

    IPAddress localIP() override {
      #ifdef NETWORK_HAS_ETHERNET
      return ETH.localIP();
      #else
      return IPAddress();
      #endif
    }

    IPAddress subnetMask() override {
      #ifdef NETWORK_HAS_ETHERNET
      return ETH.subnetMask();
      #else
      return IPAddress();
      #endif
    }

    IPAddress gatewayIP() override {
      #ifdef NETWORK_HAS_ETHERNET
      return ETH.gatewayIP();
      #else
      return IPAddress();
      #endif
    }

    IPAddress dnsIP() override {
      #ifdef NETWORK_HAS_ETHERNET
      return ETH.dnsIP();
      #else
      return IPAddress();
      #endif
    }

    String macAddress() override {
      #ifdef NETWORK_HAS_ETHERNET
      return ETH.macAddress();
      #else
      return String();
      #endif
    }

  protected:
    const unsigned long linkCheckInterval = 1000;
    const unsigned long linkGrace = 5000;
    const unsigned long leaseGrace = 20000;

    EthernetImplementation implementation = EthernetImplementation::NONE;

    uint8_t w5500Clk = GPIO_IS_NOT_CONFIGURED;
    uint8_t w5500Mosi = GPIO_IS_NOT_CONFIGURED;
    uint8_t w5500Miso = GPIO_IS_NOT_CONFIGURED;
    uint8_t w5500Cs = GPIO_IS_NOT_CONFIGURED;
    uint8_t w5500Irq = GPIO_IS_NOT_CONFIGURED;
    uint8_t w5500Rst = GPIO_IS_NOT_CONFIGURED;

    uint8_t lan8720Mdc = GPIO_IS_NOT_CONFIGURED;
    uint8_t lan8720Mdio = GPIO_IS_NOT_CONFIGURED;
    uint8_t lan8720Phy = PARM_IS_NOT_CONFIGURED;
    uint8_t lan8720Power = GPIO_IS_NOT_CONFIGURED;
    EthernetClockMode lan8720Clk = static_cast<EthernetClockMode>(PARM_IS_NOT_CONFIGURED);

    bool enabled = false;
    bool started = false;
    bool running = false;
    bool active = false;
    bool linkUp = false;
    bool restartFlag = false;
    bool startUnavailable = false;

    unsigned long prevLinkCheck = 0;
  };
}