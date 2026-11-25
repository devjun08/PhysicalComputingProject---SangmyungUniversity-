function f = ls_build_frame(seq, state, hitCnt, zone, rt, adc)
% LS_BUILD_FRAME  펌웨어 Proto_BuildFrame()과 같은 27바이트 프레임 생성
%   데모 모드(보드 없이 GUI 테스트)와 파서 테스트에 사용
adc = double(adc(:)');
f = zeros(1, 27);
f(1)  = 165;                                  % 0xA5
f(2)  = mod(seq, 256);
f(3)  = state;
f(4)  = mod(hitCnt, 256);
f(5)  = zone;
f(6)  = floor(rt / 256);  f(7) = mod(rt, 256);
f(8:2:25) = floor(adc / 256);
f(9:2:25) = mod(adc, 256);
f(26) = ls_crc8(f(2:25));
f(27) = 90;                                   % 0x5A
end
