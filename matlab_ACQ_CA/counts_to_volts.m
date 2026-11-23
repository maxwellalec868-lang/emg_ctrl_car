function volts = counts_to_volts(counts)
%COUNTS_TO_VOLTS  Uno ADC counts (0..1023) -> EMG volts centred on the SpikerShield's 2.5 V reference.
%   volts = counts_to_volts(counts)
%
%   Same conversion as Kirby's final_project-2channel-original.m (v = counts/1023*5 - 2.5).
%   Every calibration value (threshold, rest RMS, contraction RMS) is in these units, so calibration
%   and live control must both convert through this one function.

volts = counts / 1023 * 5 - 2.5;
end
