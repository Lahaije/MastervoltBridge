#include "inverter_data.h"

#include "inverter_monitor.h"

bool HomeData::isValid() const {
  return operatingStatus.length() > 0;
}

void HomeData::clear() {
  operatingStatus = "";
  errorAlarmCode = "";
  operatingMode = "";
  inverterModel = "";
  inverterMacAddress = "";
  instantaneousPower = "";
  lifetimeEnergy = "";
  dailySessionEnergy = "";
}

HomeData getInverterData() {
  HomeData data;
  InverterMonitor::getInstance().getLatestHomeData(data);
  return data;
}
