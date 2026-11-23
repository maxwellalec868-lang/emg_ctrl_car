function [samples, leftover, status] = parse_emg_chunk(text)
%PARSE_EMG_CHUNK  Parse raw serial text from full_2_channel_wireless_ctrl_alg.ino.
%   [samples, leftover, status] = parse_emg_chunk(text)
%
%   The Uno sends one line per sample, "%04d %04d\r\n" (A0 A1, raw ADC counts 0..1023), plus status
%   lines starting with '#'. Serial reads arrive in arbitrary chunks, so a chunk can start or end in
%   the middle of a line: pass the previous call's LEFTOVER in front of the new text.
%
%   text     : char or string, e.g. [leftover, read(emg, emg.NumBytesAvailable, "char")]
%   samples  : N x 2 double, one row per sample, oldest first (col 1 = A0, col 2 = A1)
%   leftover : incomplete last line, to prepend to the next chunk
%   status   : cell array of '#' status lines (for printing)
%
%   Malformed lines (e.g. the partial line right after the Uno resets) are dropped.

text = char(text);
samples = zeros(0, 2);
status = {};

lastNewline = find(text == newline, 1, 'last');
if isempty(lastNewline)
    leftover = text;
    if numel(leftover) > 64          % a valid line is at most 11 chars: this is garbage
        leftover = '';
    end
    return
end
leftover = text(lastNewline+1:end);

lines = strtrim(strsplit(text(1:lastNewline-1), newline));   % strtrim also removes the '\r'
lines = lines(~cellfun('isempty', lines));

isStatus = startsWith(lines, '#');
status = lines(isStatus);

data = lines(~isStatus);
% The Uno always zero-pads to exactly 4 digits ("%04d"), so a shorter line is a fragment (e.g. after a reset)
data = data(~cellfun('isempty', regexp(data, '^\d{4} \d{4}$', 'once')));
if isempty(data)
    return
end

values = sscanf(strjoin(data, ' '), '%d');
samples = reshape(values, 2, []).';
samples = samples(all(samples <= 1023, 2), :);
end
