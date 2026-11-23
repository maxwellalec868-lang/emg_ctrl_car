%% Smoothed Proportional Controller (draft 03)

function control_value = control03b(emg_buffer, window, threshold, rms_rest, rms_fist, channel, sensitivity, smoothing, baseline)

    persistent previous_control

    % Initialize independent channel states
    if isempty(previous_control)
        previous_control = zeros(1,2);
    end

    % Analyze live EMG
    window_start = max(1,emg_buffer.ptr-window);
    live_emg = emg_buffer.data(window_start:emg_buffer.ptr-1,1);
    live_emg = live_emg(~isnan(live_emg));
    live_rms = rms(live_emg);

    % Adjust threshold sensitivity
    adjusted_threshold = threshold * sensitivity;

    % Determine proportional control
    if live_rms <= adjusted_threshold
        proportional_control = 0;
    else
        proportional_control = (live_rms-rms_rest)/(rms_fist-rms_rest);
        proportional_control = min(max(proportional_control,0),1);
    end

    % Smooth control output independently
    smoothed_control = previous_control(channel) + smoothing*(proportional_control-previous_control(channel));
    previous_control(channel) = smoothed_control;

    % Adjust baseline while preserving 0-1 output range
    control_value = (smoothed_control-baseline)/(1-baseline);
    control_value = min(max(control_value,0),1);

end
