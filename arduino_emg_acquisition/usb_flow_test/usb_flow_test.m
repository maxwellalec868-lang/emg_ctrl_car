function pass = usb_flow_test(unoPort, nanoPort, durationS)
%USB_FLOW_TEST  Check the data path  Uno --USB--> MATLAB --USB--> Nano ESP32  (no radio, no car).
%
%   usb_flow_test                         list the serial ports
%   usb_flow_test("COM5", "COM7")         run for 10 s
%   usb_flow_test("COM5", "COM7", 30)     run for 30 s
%   pass = usb_flow_test(...)             true if every check passed
%
%   Needs:  Uno  running usb_flow_test/uno_usb_counter/uno_usb_counter.ino
%           Nano running usb_flow_test/nano_usb_echo/nano_usb_echo.ino
%   Close the Arduino Serial Monitors first: only one program can hold each port.
%
%   What it checks (same port settings and rates as emg_ctrl_bridge.m):
%     Uno -> MATLAB : ~1000 lines/s of "%04d %04d", parsed by parse_emg_chunk.m. The first number
%                     counts 0..1023, so any skipped value = a lost line. The second must equal
%                     1023 - first, so any mismatch = a corrupted line.
%     MATLAB -> Nano: every 50 ms the low byte of the newest counter value is written to the Nano.
%     Nano -> MATLAB: the Nano answers each byte with its inverse (b XOR 0xFF). Every reply is
%                     checked against what was sent, in order, and the round-trip time is measured.

if nargin < 2
    fprintf("Available serial ports:\n");
    disp(serialportlist("available").');
    fprintf("Usage: usb_flow_test(""COM_uno"", ""COM_nano"")\n");
    fprintf("Tip: unplug one board and list again to see which port disappears.\n");
    pass = false;
    return
end
if nargin < 3
    durationS = 10;
end

UNO_BAUD      = 250000;   % same as full_2_channel_wireless_ctrl_alg.ino
NANO_BAUD     = 115200;   % same as nrf_tx_drive.ino
FS            = 1000;
SEND_PERIOD_S = 0.05;     % same command rate as emg_ctrl_bridge.m

% parse_emg_chunk.m lives next to emg_ctrl_bridge.m
addpath(fullfile(fileparts(mfilename('fullpath')), '..', 'full_2_channel_wireless_ctrl_alg'));

uno = serialport(unoPort, UNO_BAUD);
nano = serialport(nanoPort, NANO_BAUD);
fprintf("Ports open. Waiting 2 s for the Uno to restart (opening its port resets it)...\n");
pause(2);
flush(uno);
flush(nano);

%% ---------------- Step 1: is the Nano answering? ----------------
fprintf("\n[1/2] Nano probe: sending 0x5A, expecting 0xA5... ");
write(nano, uint8(0x5A), "uint8");
reply = waitForBytes(nano, 1, 2);
if isempty(reply)
    fprintf("NO REPLY\n");
    fprintf("FAIL: the Nano did not answer. Is nano_usb_echo.ino flashed (blue LED on)? Is %s the Nano's port?\n", nanoPort);
    pass = false;
    return
elseif reply ~= 0xA5
    fprintf("got 0x%02X\n", reply);
    fprintf("FAIL: wrong reply. Is nano_usb_echo.ino flashed, and is %s really the Nano (not the Uno)?\n", nanoPort);
    pass = false;
    return
end
fprintf("OK\n");

%% ---------------- Step 2: full flow for durationS ----------------
fprintf("[2/2] Running the full flow for %g s...\n", durationS);
leftover = '';
lastCount = -1;
nLines = 0; nLost = 0; nCorrupt = 0;
pendingVal = []; pendingT = [];      % bytes sent to the Nano, waiting for their reply (FIFO)
nSent = 0; nReplyOK = 0; nReplyBad = 0; latencyMs = [];

tStart = tic; tSend = tic; tPrint = tic;
linesAtLastPrint = 0;
while toc(tStart) < durationS
    % Uno -> MATLAB
    nBytes = uno.NumBytesAvailable;
    if nBytes > 0
        [samples, leftover, status] = parse_emg_chunk([leftover, read(uno, nBytes, "char")]);
        for i = 1:numel(status)
            fprintf("   [uno] %s\n", status{i});
        end
        if ~isempty(samples)
            nCorrupt = nCorrupt + sum(samples(:, 2) ~= 1023 - samples(:, 1));
            counts = samples(:, 1);
            if lastCount >= 0
                steps = mod(diff([lastCount; counts]), 1024);
            else
                steps = mod(diff(counts), 1024);
            end
            nLost = nLost + sum(mod(steps - 1, 1024));   % step 1 = no loss, step k = k-1 lost
            lastCount = counts(end);
            nLines = nLines + numel(counts);
        end
    end

    % MATLAB -> Nano
    if toc(tSend) >= SEND_PERIOD_S && lastCount >= 0
        tSend = tic;
        b = mod(lastCount, 256);
        write(nano, uint8(b), "uint8");
        pendingVal(end+1) = b; %#ok<AGROW>
        pendingT(end+1) = toc(tStart); %#ok<AGROW>
        nSent = nSent + 1;
    end

    % Nano -> MATLAB
    [pendingVal, pendingT, nReplyOK, nReplyBad, latencyMs] = ...
        checkReplies(nano, toc(tStart), pendingVal, pendingT, nReplyOK, nReplyBad, latencyMs);

    if toc(tPrint) >= 1
        fprintf("   %4.0f s: Uno %5.0f lines/s | Nano sent %4d, replies OK %4d, bad %d\n", ...
            toc(tStart), (nLines - linesAtLastPrint) / toc(tPrint), nSent, nReplyOK, nReplyBad);
        linesAtLastPrint = nLines;
        tPrint = tic;
    end
    pause(0.001);
end

% Give the last replies time to arrive
tDrain = tic;
while ~isempty(pendingVal) && toc(tDrain) < 0.5
    [pendingVal, pendingT, nReplyOK, nReplyBad, latencyMs] = ...
        checkReplies(nano, toc(tStart), pendingVal, pendingT, nReplyOK, nReplyBad, latencyMs);
    pause(0.01);
end
nMissing = numel(pendingVal);
elapsed = toc(tStart);

%% ---------------- Report ----------------
rate = nLines / elapsed;
okRate    = rate >= 0.95 * FS;
okLost    = nLost == 0;
okCorrupt = nCorrupt == 0;
okReplies = nSent > 0 && nReplyOK == nSent && nReplyBad == 0 && nMissing == 0;
pass = okRate && okLost && okCorrupt && okReplies;

fprintf("\n================ USB flow test report ================\n");
fprintf("Uno -> MATLAB   lines received : %d in %.1f s (%.0f lines/s, expect ~%d)  %s\n", ...
    nLines, elapsed, rate, FS, verdict(okRate));
fprintf("                lost lines     : %d  %s\n", nLost, verdict(okLost));
fprintf("                corrupted lines: %d  %s\n", nCorrupt, verdict(okCorrupt));
fprintf("MATLAB -> Nano  bytes sent     : %d\n", nSent);
fprintf("Nano -> MATLAB  replies OK     : %d, wrong: %d, missing: %d  %s\n", ...
    nReplyOK, nReplyBad, nMissing, verdict(okReplies));
if ~isempty(latencyMs)
    fprintf("                round trip     : median %.1f ms, max %.1f ms\n", median(latencyMs), max(latencyMs));
end
fprintf("------------------------------------------------------\n");
if pass
    fprintf("PASS: data flows Uno -> MATLAB -> Nano (and back) correctly.\n");
else
    fprintf("FAIL: see the lines marked FAIL above.\n");
    if ~okCorrupt && nCorrupt > nLines / 2
        fprintf("  Most lines failed the pattern check: is uno_usb_counter.ino flashed (not the EMG sketch)?\n");
    end
    if ~okRate
        fprintf("  Too few lines: check the Uno's port/baud, or close other programs using it.\n");
    end
end
fprintf("Afterwards: flash full_2_channel_wireless_ctrl_alg.ino (Uno) and nrf_tx_drive.ino (Nano) back.\n");
end


function data = waitForBytes(port, count, timeoutS)
% Waits up to timeoutS for COUNT bytes; returns [] on timeout
data = [];
t = tic;
while toc(t) < timeoutS
    if port.NumBytesAvailable >= count
        data = double(read(port, count, "uint8"));
        return
    end
    pause(0.01);
end
end


function [pendingVal, pendingT, nOK, nBad, latencyMs] = checkReplies(nano, tNow, pendingVal, pendingT, nOK, nBad, latencyMs)
% Matches every reply waiting on the Nano port against the oldest unanswered byte
m = nano.NumBytesAvailable;
if m == 0
    return
end
replies = double(read(nano, m, "uint8"));
replies = replies(:).';                               % row, so the for-loop visits each byte
for r = replies
    if isempty(pendingVal)
        nBad = nBad + 1;                              % a reply nobody asked for
        continue
    end
    if r == bitxor(pendingVal(1), 255)
        nOK = nOK + 1;
        latencyMs(end+1) = (tNow - pendingT(1)) * 1000; %#ok<AGROW>
    else
        nBad = nBad + 1;
    end
    pendingVal(1) = [];
    pendingT(1) = [];
end
end


function s = verdict(ok)
if ok
    s = 'PASS';
else
    s = 'FAIL';
end
end
