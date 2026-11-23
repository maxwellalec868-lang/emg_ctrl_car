%% Proportional Controller (draft 03)

function control_value= proportional_control(emg_buffer, window, threshold, rms_rest, rms_fist)

    % Analyze live EMG (last 250ms)
    window_start=   max(1, emg_buffer.ptr- window);
    live_emg=       emg_buffer.data(window_start:emg_buffer.ptr-1, 1);
    live_emg=       live_emg(~isnan(live_emg)); % remove nan
    live_rms=       rms(live_emg);

    % Determine control
    if live_rms <= threshold
        % No activation
        control_value = 0;
    else
        % Activation
        control_value= (live_rms- rms_rest)/ (rms_fist- rms_rest); % scale
        control_value= min(max(control_value, 0), 1); % normalize (0-1)
    end

end