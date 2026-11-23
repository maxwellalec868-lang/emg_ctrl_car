function calibration = compute_calibration_channel(calibration, ch)
%COMPUTE_CALIBRATION_CHANNEL  Calibration values for one channel from its raw recordings.
%   calibration = compute_calibration_channel(calibration, ch)
%
%   From Kirby's final_project-2channel-original.m (local function recalculateCalibrationChannel).
%   The math is unchanged. It is used for both new and revised calibrations: Section 1A's inline
%   loop was the same computation.
%
%   Uses:  calibration.fs, .window (250 ms of samples), .plateau_fraction, .n_requested,
%          .raw_rest (rest recording, N x 2 volts) and .raw_fist{ch} (contraction recording, N x 2 volts)
%   Sets for channel ch:
%     threshold      mean + 3*std of the resting moving RMS (above this = muscle active)
%     rest_rms       RMS of the whole rest recording
%     contraction_rms, mean_contr_rms   RMS of the middle 50 % of each detected contraction
%     snr, mean_snr, std_snr            20*log10(contraction RMS / rest RMS)
%     n_detected     contractions found (moving RMS above threshold; gaps <= 150 ms merged)

fs = calibration.fs;
window = calibration.window;

rest = calibration.raw_rest(:, ch);
fist = calibration.raw_fist{ch}(:, ch);

% Moving RMS
rms_rest_moving = sqrt(movmean(rest.^2, window));
rms_fist_moving = sqrt(movmean(fist.^2, window));

% Rest-based threshold
threshold = mean(rms_rest_moving) + 3*std(rms_rest_moving);
rest_rms = sqrt(mean(rest.^2));

calibration.threshold(ch) = threshold;
calibration.rest_rms(ch) = rest_rms;

% Detect contractions
active = rms_fist_moving > threshold;
edges = diff([false; active; false]);

starts = find(edges == 1);
stops = find(edges == -1) - 1;

% Handle missing contractions
if isempty(starts)
    calibration.contraction_rms{ch} = [];
    calibration.mean_contr_rms(ch) = NaN;
    calibration.snr{ch} = [];
    calibration.mean_snr(ch) = NaN;
    calibration.std_snr(ch) = NaN;
    calibration.n_detected(ch) = 0;

    warning('No contractions detected on Channel %d.', ch);
    return
end

% Merge nearby segments (150 ms)
merged_starts = starts(1);
merged_stops = stops(1);
min_gap = round(0.15*fs);

for k = 2:numel(starts)
    if starts(k) - merged_stops(end) <= min_gap
        merged_stops(end) = stops(k);
    else
        merged_starts(end+1) = starts(k); %#ok<AGROW>
        merged_stops(end+1) = stops(k); %#ok<AGROW>
    end
end

% Middle 50% contraction RMS
contraction_rms = zeros(numel(merged_starts), 1);

for k = 1:numel(merged_starts)
    segment = fist(merged_starts(k):merged_stops(k));

    trim = floor(numel(segment) * (1-calibration.plateau_fraction)/2);

    if 2*trim >= numel(segment)
        trim = 0;
    end

    plateau = segment(1+trim:end-trim);
    contraction_rms(k) = sqrt(mean(plateau.^2));
end

% SNR
if rest_rms > 0
    snr = 20*log10(contraction_rms / rest_rms);
else
    snr = nan(size(contraction_rms));
end

% Store results
calibration.contraction_rms{ch} = contraction_rms;
calibration.mean_contr_rms(ch) = mean(contraction_rms);
calibration.snr{ch} = snr;
calibration.mean_snr(ch) = mean(snr);
calibration.std_snr(ch) = std(snr);
calibration.n_detected(ch) = numel(merged_starts);

if numel(merged_starts) ~= calibration.n_requested
    warning('Channel %d: detected %d contractions, expected %d.', ...
        ch, numel(merged_starts), calibration.n_requested);
end
end
