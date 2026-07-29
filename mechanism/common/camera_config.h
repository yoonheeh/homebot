#pragma once

struct CameraIntrinsics {
  int exposure_time_us;
  int sensitivity_iso;      // 100...1800
  int color_temperature_k;  // 1000...12000
  int focus;                // 0...255
};
