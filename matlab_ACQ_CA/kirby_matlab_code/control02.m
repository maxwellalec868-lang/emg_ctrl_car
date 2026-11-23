%% Recursively Filtered Proportional Controller (draft 03)

function control_value= control02(emg_buffer, window, threshold, rms_rest, rms_fist)

    % Samples arrive already DC-corrected: SerialComm.get_recent_emg
    % subtracts the SpikerShield's 2.5V pedestal before buffering, and the
    % calibration constants were captured through that same path.

    persistent P N last_ptr

    g_base= 1/window;   % gain when nothing surprising is happening
    g_max=  1/25;       % ceiling on how far the gain may open
    g_n=    1/100;      % smoothing on the surprise estimate

    % Initialize state on first call
    if isempty(P)
        P=        rms_rest^2;  % start assuming a resting muscle
        N=        1;           % assume well-tuned
        last_ptr= max(1, emg_buffer.ptr- window);
    end

    % Update filter once per new sample since last call
    for k= last_ptr:emg_buffer.ptr-1
        x= emg_buffer.data(k, 1);
        if isnan(x); continue; end

        d=   x^2- P;                % innovation (surprise)
        nis= d^2/(2*P^2+ eps);      % normalized; ~1 when well-tuned

        % N must average over many samples. If it tracked single samples the
        % gain would correlate with the current innovation, which biases P
        % upward, since x^2 is bounded below by 0 but unbounded above.
        N=   N+ g_n*(nis- N);

        g_eff= min(g_max, g_base*max(1, N));
        P=     P+ g_eff*d;          % running power estimate
    end
    last_ptr= emg_buffer.ptr;

    % Recover envelope
    live_rms= sqrt(P);

    % Determine control (identical to control01)
    if live_rms <= threshold
        % No activation
        control_value= 0;
    else
        % Activation
        control_value= (live_rms- rms_rest)/ (rms_fist- rms_rest); % scale
        control_value= min(max(control_value, 0), 1); % normalize (0-1)
    end

end
