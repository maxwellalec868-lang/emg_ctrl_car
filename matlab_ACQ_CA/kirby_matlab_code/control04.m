
function control_value = control04(emg_buffer, window, threshold, ~, ~)
% Control 04 - Binary Threshold Controller
% - Returns 1 when moving RMS exceeds threshold
% - Returns 0 otherwise

% Extract most recent EMG samples
recent_emg = emg_buffer.data(emg_buffer.ptr-window:emg_buffer.ptr-1);

% Calculate moving RMS
moving_rms = sqrt(mean(recent_emg.^2));

% Binary control
control_value = double(moving_rms > threshold);

end
