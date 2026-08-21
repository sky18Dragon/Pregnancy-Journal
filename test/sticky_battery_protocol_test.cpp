#include <cassert>

#include "sticky_battery_protocol.h"

int main()
{
    // DeviceType is requested through Control but returned through MAC data.
    // DeviceType从Control发起请求，但需要从MAC数据寄存器读取结果。
    assert(StickyBatteryProtocol::kControlRegister == 0x00U);
    assert(StickyBatteryProtocol::kMacDataRegister == 0x40U);
    assert(StickyBatteryProtocol::kControlRegister !=
           StickyBatteryProtocol::kMacDataRegister);
    assert(StickyBatteryProtocol::decode_word(0x20U, 0x02U) ==
           StickyBatteryProtocol::kExpectedDeviceId);
    assert(StickyBatteryProtocol::kStateOfChargeRegister == 0x2CU);
    return 0;
}
