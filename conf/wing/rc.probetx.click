elementclass RateControl {
  $rate, $rates|

  rate_control :: ProbeTXRate(OFFSET 4, RT $rates, WINDOW 1);

  input -> rate_control -> output;
  input [1] -> [1] rate_control [1] -> [1] output;

};

