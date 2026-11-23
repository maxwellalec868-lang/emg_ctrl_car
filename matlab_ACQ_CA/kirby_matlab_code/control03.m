%% Smoothed Proportional Controller (draft 01)

function control_value= control03(emg_buffer, window, threshold, rms_rest, rms_fist)

    persistent previous_control

    smoothing= 0.1;

    % Initialize state
    if isempty(previous_control)
        previous_control= 0;
    end

    % Analyze live EMG
    window_start= max(1,emg_buffer.ptr-window);
    live_emg= emg_buffer.data(window_start:emg_buffer.ptr-1,1);
    live_emg= live_emg(~isnan(live_emg));
    live_rms= rms(live_emg);

    % Determine proportional control
    if live_rms <= threshold
        proportional_control= 0;
    else
        proportional_control= (live_rms-rms_rest)/(rms_fist-rms_rest);
        proportional_control= min(max(proportional_control,0),1);
    end

    % Smooth control output
    control_value= previous_control+smoothing*(proportional_control-previous_control);

    previous_control= control_value;

end