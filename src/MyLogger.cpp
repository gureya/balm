/*
 * MyLogger.cpp
 *
 *  Created on: Jan 27, 2020
 *      Author: David Daharewa Gureya
 */

#include "include/MyLogger.hpp"

MyLogger::MyLogger(int crr, int cml, double hpt, double hps, double bes) {
  current_remote_ratio = crr;
  current_mba_level = cml;
  HPA_target_stall_rate = hpt;
  HPA_stall_rate = hps;
  BEA_stall_rate = bes;
}

