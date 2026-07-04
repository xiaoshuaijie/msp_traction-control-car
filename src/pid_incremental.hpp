#pragma once

#include <cmath>
#include <cstdint>

#include "timebase.hpp"

namespace Jie
{

enum class PIDImprovement : uint8_t {
  None              = 0x00,
  IntegralLimit     = 0x01,
  DerivativeOnMeas  = 0x02,
  TrapezoidIntegral = 0x04,
  FeedForward       = 0x08,
  OutputFilter      = 0x10,
  ChangingRate      = 0x20,
  DerivativeFilter  = 0x40,
};

inline PIDImprovement operator|(PIDImprovement a, PIDImprovement b) {
  return static_cast<PIDImprovement>(
      static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline bool operator&(PIDImprovement a, PIDImprovement b) {
  return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
}

class IncrementalPID {
 public:
  struct Config {
    float kp = 0.0f;
    float ki = 0.0f;
    float kd = 0.0f;
    float max_output = 0.0f;
    float integral_limit = 0.0f;
    float deadband = 0.0f;
    PIDImprovement improve = PIDImprovement::None;
    float derivative_lpf_rc = 0.1f;
    float output_lpf_rc = 0.05f;
    float coef_a = 0.0f;
    float coef_b = 0.0f;
    float kf = 0.0f;
    float feedforward_max = 0.0f;
  };

  IncrementalPID() = default;

  explicit IncrementalPID(const Config& cfg) { Init(cfg); }

  void Init(const Config& cfg) {
    kp_ = cfg.kp;
    ki_ = cfg.ki;
    kd_ = cfg.kd;
    max_out_ = cfg.max_output;
    integral_limit_ = cfg.integral_limit;
    deadband_ = cfg.deadband;
    improve_ = cfg.improve;
    derivative_lpf_rc_ = cfg.derivative_lpf_rc;
    output_lpf_rc_ = cfg.output_lpf_rc;
    coef_a_ = (cfg.coef_a > 0.0f) ? cfg.coef_a : cfg.max_output * 0.5f;
    coef_b_ = (cfg.coef_b > 0.0f) ? cfg.coef_b : cfg.max_output * 0.1f;
    kf_ = cfg.kf;
    feedforward_max_ =
        (cfg.feedforward_max > 0.0f) ? cfg.feedforward_max : cfg.max_output * 0.5f;

    first_call_ = true;
    last_time_ = LibXR::Timebase::GetMicroseconds();
  }

  float Calculate(float target, float actual) {
    auto now = LibXR::Timebase::GetMicroseconds();
    dt_ = (now - last_time_).ToSecondf();
    last_time_ = now;
    if (dt_ < 1e-6f) {
      dt_ = 1e-6f;
    }

    if (first_call_) {
      first_call_ = false;
      actual_ = actual;
      target_ = target;
      err_ = target - actual;
      last_err_ = err_;
      err_pre_ = err_;
      last_actual_ = actual;
      last_target_ = target;
      return output_;
    }

    last_actual_ = actual_;
    last_target_ = target_;
    actual_ = actual;
    target_ = target;

    err_pre_ = last_err_;
    last_err_ = err_;
    err_ = target_ - actual_;

    if (std::fabs(err_) <= deadband_) {
      output_inc_ = 0.0f;
      iout_ = 0.0f;
      return output_;
    }

    pout_ = kp_ * (err_ - last_err_);

    iout_ = ki_ * err_ * dt_;
    if (improve_ & PIDImprovement::TrapezoidIntegral) {
      TrapezoidIntegral();
    }
    if (improve_ & PIDImprovement::ChangingRate) {
      ChangingIntegrationRate();
    }
    if (improve_ & PIDImprovement::IntegralLimit) {
      IntegralLimitClamp();
    }

    float raw_dout = 0.0f;
    if (improve_ & PIDImprovement::DerivativeOnMeas) {
      DerivativeOnMeasurement();
    } else {
      raw_dout = kd_ * (err_ - 2.0f * last_err_ + err_pre_) / dt_;
      if (improve_ & PIDImprovement::DerivativeFilter) {
        DerivativeFilterApply(raw_dout);
      } else {
        dout_ = raw_dout;
      }
    }

    float feedforward = 0.0f;
    if (improve_ & PIDImprovement::FeedForward) {
      feedforward = FeedForwardCalc();
    }

    output_inc_ = pout_ + iout_ + dout_ + feedforward;
    float raw_output = last_output_ + output_inc_;

    if (improve_ & PIDImprovement::OutputFilter) {
      OutputFilterApply(raw_output);
    } else {
      output_ = raw_output;
    }

    OutputLimitClamp();
    last_output_ = output_;

    return output_;
  }

  float GetIncrement() const { return output_inc_; }

  void Reset() {
    actual_ = 0.0f;
    last_actual_ = 0.0f;
    target_ = 0.0f;
    last_target_ = 0.0f;
    err_ = 0.0f;
    last_err_ = 0.0f;
    err_pre_ = 0.0f;
    pout_ = 0.0f;
    iout_ = 0.0f;
    dout_ = 0.0f;
    output_ = 0.0f;
    output_inc_ = 0.0f;
    last_output_ = 0.0f;
    last_dout_ = 0.0f;
    first_call_ = true;
    last_time_ = LibXR::Timebase::GetMicroseconds();
  }

 private:
  float kp_ = 0.0f;
  float ki_ = 0.0f;
  float kd_ = 0.0f;
  float max_out_ = 0.0f;
  float deadband_ = 0.0f;
  float dt_ = 0.0f;
  PIDImprovement improve_ = PIDImprovement::None;

  float integral_limit_ = 0.0f;
  float coef_a_ = 0.0f;
  float coef_b_ = 0.0f;

  float derivative_lpf_rc_ = 0.1f;
  float output_lpf_rc_ = 0.05f;

  float kf_ = 0.0f;
  float feedforward_max_ = 0.0f;

  float actual_ = 0.0f;
  float last_actual_ = 0.0f;
  float target_ = 0.0f;
  float last_target_ = 0.0f;

  float err_ = 0.0f;
  float last_err_ = 0.0f;
  float err_pre_ = 0.0f;

  float pout_ = 0.0f;
  float iout_ = 0.0f;
  float dout_ = 0.0f;

  float output_ = 0.0f;
  float output_inc_ = 0.0f;
  float last_output_ = 0.0f;
  float last_dout_ = 0.0f;

  LibXR::MicrosecondTimestamp last_time_{};
  bool first_call_ = true;

  void TrapezoidIntegral() {
    iout_ = ki_ * ((err_ + last_err_) * 0.5f) * dt_;
  }

  void ChangingIntegrationRate() {
    float abs_err = std::fabs(err_);
    if (abs_err > coef_b_ && abs_err <= (coef_a_ + coef_b_)) {
      float factor = (coef_a_ - abs_err + coef_b_) / coef_a_;
      iout_ *= factor;
    } else {
      iout_ = 0.0f;
    }
  }

  void IntegralLimitClamp() {
    if (iout_ > integral_limit_) {
      iout_ = integral_limit_;
    } else if (iout_ < -integral_limit_) {
      iout_ = -integral_limit_;
    }
  }

  void DerivativeOnMeasurement() {
    dout_ = kd_ * (last_actual_ - actual_) / dt_;
  }

  void DerivativeFilterApply(float raw_dout) {
    float factor = dt_ / (derivative_lpf_rc_ + dt_);
    dout_ = raw_dout * factor + last_dout_ * (1.0f - factor);
    last_dout_ = dout_;
  }

  void OutputFilterApply(float raw_output) {
    float factor = dt_ / (output_lpf_rc_ + dt_);
    output_ = raw_output * factor + last_output_ * (1.0f - factor);
  }

  void OutputLimitClamp() {
    if (output_ > max_out_) {
      output_ = max_out_;
    } else if (output_ < -max_out_) {
      output_ = -max_out_;
    }
  }

  float FeedForwardCalc() {
    float delta_target = target_ - last_target_;
    float delta_ff = kf_ * delta_target / dt_;
    if (delta_ff > feedforward_max_) {
      delta_ff = feedforward_max_;
    } else if (delta_ff < -feedforward_max_) {
      delta_ff = -feedforward_max_;
    }
    return delta_ff;
  }
};

}  // namespace Jie
