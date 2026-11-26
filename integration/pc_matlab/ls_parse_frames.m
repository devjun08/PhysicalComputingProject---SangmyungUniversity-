function [frames, buf, nBad] = ls_parse_frames(buf)
% LS_PARSE_FRAMES  수신 바이트 버퍼에서 27바이트 프레임을 모두 꺼냄
%
%   [frames, buf, nBad] = ls_parse_frames(buf)
%     buf    : 지금까지 받은 바이트 (double 행벡터). 남은 조각이 다시 반환됨
%     frames : 구조체 배열 (seq, state, hitCnt, zone, rt, adc(1x9))
%     nBad   : 헤더(A5)는 맞았지만 꼬리/CRC가 틀려 버린 횟수
%
% 수업 HealthMonitor 앱의 "첫 값 A5, 마지막 값 5A" 방식과 같고, CRC 검사만 추가함.
%
%  idx(1-based): 1   2    3      4       5     6     7     8..25        26    27
%  내용        : A5  Seq  State  HitCnt  Zone  RT_H  RT_L  CH1_H..CH9_L CRC8  5A

HEAD = 165;   % 0xA5
TAIL = 90;    % 0x5A
LEN  = 27;

frames = struct('seq', {}, 'state', {}, 'hitCnt', {}, 'zone', {}, 'rt', {}, 'adc', {});
nBad = 0;

while true
    k = find(buf == HEAD, 1);
    if isempty(k)
        buf = [];                % 헤더가 하나도 없으면 전부 버림
        return;
    end
    buf = buf(k:end);            % 헤더 앞 쓰레기 제거
    if numel(buf) < LEN
        return;                  % 아직 덜 들어옴 -> 다음 번에
    end

    f = buf(1:LEN);
    if f(LEN) == TAIL && ls_crc8(f(2:25)) == f(26)
        s.seq    = f(2);
        s.state  = f(3);
        s.hitCnt = f(4);
        s.zone   = f(5);
        s.rt     = f(6) * 256 + f(7);                 % 상위 바이트 먼저
        s.adc    = f(8:2:25) * 256 + f(9:2:25);       % 9채널
        frames(end + 1) = s; %#ok<AGROW>
        buf = buf(LEN + 1:end);
    else
        nBad = nBad + 1;         % 데이터 중간의 A5였음 -> 한 칸 밀고 다시 찾기
        buf = buf(2:end);
    end
end
end
