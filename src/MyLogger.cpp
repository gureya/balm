/*
 * MyLogger.cpp
 *
 *  Created on: Jan 27, 2020
 *      Author: David Daharewa Gureya
 */

#include "include/MyLogger.hpp"

MyLogger::MyLogger(std::chrono::system_clock::time_point tn, int crr, int cml,
                   double hpt, double hcl, double slk, double hps, double bes,
                   std::string act, int lc) {
  timenow = tn;
  current_remote_ratio = crr;
  current_mba_level = cml;
  HPA_target_slo = hpt;
  HPA_currency_latency = hcl;
  HPA_slack = slk;
  HPA_stall_rate = hps;
  BEA_stall_rate = bes;
  action = act;
  logCounter = lc;
}
