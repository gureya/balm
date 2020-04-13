/*
 * MyLogger.hpp
 *
 *  Created on: Jan 27, 2020
 *      Author: David Daharewa Gureya
 */

#ifndef INCLUDE_MYLOGGER_HPP_
#define INCLUDE_MYLOGGER_HPP_

#include <chrono>
#include <string>

class MyLogger {
 public:
  std::chrono::system_clock::time_point timenow;
  int current_remote_ratio;
  int current_mba_level;
  double HPA_target_slo;
  double HPA_currency_latency;
  double HPA_stall_rate;
  double BEA_stall_rate;
  std::string action;

  // constructor
  MyLogger(std::chrono::system_clock::time_point tn, int crr, int cml,
           double hpt, double hcl, double hps, double bes, std::string act);
};

#endif /* INCLUDE_MYLOGGER_HPP_ */
