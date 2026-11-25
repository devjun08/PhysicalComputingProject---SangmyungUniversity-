#!/usr/bin/env python3
"""Run actual motor/game/protocol C with mocked SPL GPIO/timer operations.
This checks control logic; it does not validate electrical timing or SPL linkage.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET
root = Path(__file__).resolve().parents[1]
firmware = root / 'firmware'
motor = (firmware / 'motor.c').read_text()
fields = sorted(set(re.findall(r'\b(?:gpio|tim|oc)\.(\w+)', motor)))
functions = set(re.findall(r'\b((?:GPIO|TIM|RCC)_\w+)\(', motor))
constants = set(re.findall(r'\b(?:GPIO|TIM|RCC)_\w+\b', motor)) - functions
constants -= set(fields)
constants -= {'GPIO_InitTypeDef','TIM_TimeBaseInitTypeDef','TIM_OCInitTypeDef'}
header = '#include <stdint.h>\ntypedef struct { unsigned CCR1, CCR2; } MockTimer;\n'
header += 'extern MockTimer mock_tim;\nextern unsigned mock_switches;\n#define TIM1 (&mock_tim)\n#define GPIOA 0\n#define GPIOB 1\n#define ENABLE 1\n#define Bit_RESET 0\n'
header += 'typedef struct {' + ''.join('unsigned '+v+';' for v in fields) + '} MockInit;\n'
header += 'typedef MockInit GPIO_InitTypeDef;\ntypedef MockInit TIM_TimeBaseInitTypeDef;\ntypedef MockInit TIM_OCInitTypeDef;\n'
for i, name in enumerate(sorted(constants)):
    value = (1 << int(name.rsplit('_',1)[1])) if name.startswith('GPIO_Pin_') else i
    header += f'#define {name} {value}U\n'
for name in sorted(functions - {'GPIO_ReadInputDataBit'}):
    header += f'#define {name}(...) ((void)0)\n'
header += '#define GPIO_ReadInputDataBit(port,pin) ((mock_switches & (pin)) ? 0 : 1)\n'
with tempfile.TemporaryDirectory() as work:
    work = Path(work)
    (work / 'stm32f4xx.h').write_text(header)
    for enabled in [0,1]:
        binary = work / f'control-{enabled}'
        subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Wno-unused-but-set-variable',
            f'-DHIT_DETECTION_ENABLED={enabled}', '-I'+str(work), '-I'+str(firmware),
            str(root/'tests/test_control.c'), str(firmware/'motor.c'),
            str(firmware/'game.c'), str(firmware/'hit_detector.c'), str(firmware/'protocol.c'), '-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
project=ET.parse(root/'Integration.ewp').getroot()
for node in project.findall('file/name'):
    relative=node.text.replace('$PROJ_DIR$/','')
    if not relative.startswith('Libraries/'):
        assert (root/relative).is_file(), relative
sources=[p.read_text() for p in firmware.glob('*.c')]
for handler in ['main','TIM2_IRQHandler','USART2_IRQHandler']:
    assert sum(len(re.findall(r'\b'+handler+r'\(void\)\s*\{',s)) for s in sources)==1,handler
print('IAR source paths and interrupt ownership checks passed')
