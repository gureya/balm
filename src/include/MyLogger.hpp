/*
 * MyLogger.hpp
 *
 *  Created on: Jan 27, 2020
 *      Author: David Daharewa Gureya
 */

#ifndef INCLUDE_MYLOGGER_HPP_
#define INCLUDE_MYLOGGER_HPP_

#include <string>

class MyLogger {
 public:
  int current_remote_ratio;
  int current_mba_level;
  double HPA_target_stall_rate;
  double HPA_stall_rate;
  double BEA_stall_rate;
  std::string action;

  //constructor
  MyLogger(int crr, int cml, double hpt, double hps, double bes,
           std::string act);
};

#endif /* INCLUDE_MYLOGGER_HPP_ */
