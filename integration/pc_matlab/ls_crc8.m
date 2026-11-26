function crc = ls_crc8(data)
% LS_CRC8  CRC-8 (poly 0x07, init 0x00) - 펌웨어 Proto_CRC8()과 동일
%   data : 0~255 값의 벡터 (double 또는 uint8)
crc = 0;
data = double(data);
for k = 1:numel(data)
    crc = bitxor(crc, data(k));
    for b = 1:8
        if bitand(crc, 128)
            crc = bitand(bitxor(crc * 2, 7), 255);
        else
            crc = bitand(crc * 2, 255);
        end
    end
end
end
