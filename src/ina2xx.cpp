#include "ina2xx.h"
#include <RemoteDebug.h>
#include "sensesp.h"

INA2xx::INA2xx(uint16_t max_amps, uint32_t shunt_micro_ohms, INA_Class* ina_class) {
  if (ina_class == NULL) {
    ina_class = new INA_Class;
  }
  ina_ = ina_class;
  uint8_t devicesFound = 0;
  devicesFound = ina_->begin(max_amps, shunt_micro_ohms);
  if (devicesFound == 0) {
    debugE("No INA2xx sensors found: retry in 10 seconds.");
    delay(10000);
  }
  if (devicesFound == 0) {
    sensor_detected_ = false;
    debugE("No INA2xx sensors found: check wiring.");
    return;
  } 
  else {
    debugI("Found %d INA2xx devices on the I2C bus.", devicesFound);
  }
  for (uint8_t i = 0; i < devicesFound; i++) {
    debugI("Device #, addr, and name: %2d 0x%2x %s", i, ina_->getDeviceAddress(i), ina_->getDeviceName(i));
  }
  ina_->setAveraging(60);
}

INA2xxValue::INA2xxValue(INA2xx* ina2xx, uint8_t deviceNum,
                         INA2xxValType val_type, uint read_delay,
                         String config_path)
    : NumericSensor(config_path),
      ina2xx_{ina2xx},
      deviceNum_{deviceNum},
      val_type_{val_type},
      read_delay_{read_delay} {
  load_configuration();
}

void INA2xxValue::enable() {
  if (!ina2xx_->sensor_detected_) {
    debugE("INA2xxValue not enabled: no INA2xx sensors detected");
  } else {
    app.onRepeat(read_delay_, [this]() {
      float reading = 0.0;
      switch (val_type_) {
        case bus_volts:
          reading = (ina2xx_->ina_->getBusMilliVolts(deviceNum_) /
                     1000.0);  // SK wants volts, not mV
          break;
        case shunt_volts:
          reading = (ina2xx_->ina_->getShuntMicroVolts(deviceNum_) /
                     1000000.0);  // SK wants volts, not uV
          break;
        case watts:
          reading = (ina2xx_->ina_->getBusMicroWatts(deviceNum_) /
                     1000000.0);  // SK wants watts, not uW
          break;
        case amps:
          reading = (ina2xx_->ina_->getBusMicroAmps(deviceNum_) /
                     1000000.0);  // SK want amps, not uA
          break;
      }
      output = reading;
      notify();
    });
  }
}

void INA2xxValue::get_configuration(JsonObject& root) {
  root["read_delay"] = read_delay_;
};

static const char SCHEMA[] PROGMEM = R"###({
    "type": "object",
    "properties": {
        "read_delay": { "title": "Read delay", "type": "number", "description": "The time, in milliseconds, between each read of the sensor" }
    }
  })###";

String INA2xxValue::get_config_schema() { return FPSTR(SCHEMA); }

bool INA2xxValue::set_configuration(const JsonObject& config) {
  String expected[] = {"read_delay"};
  for (auto str : expected) {
    if (!config.containsKey(str)) {
      return false;
    }
  }
  read_delay_ = config["read_delay"];
  return true;
}
