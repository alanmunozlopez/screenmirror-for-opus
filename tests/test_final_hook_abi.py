"""Independent static ABI proof for the built Blackfin final-frame entry.

Checks assembled instructions, not source formatting. A symbolic hostile C
callee overwrites every caller-visible register and its 12-byte argument area;
the bridge must restore original registers/arguments/RETS and tail-chain exactly
once to the SDK cache flush. This is not execution on Blackfin or hardware proof.

Run scripts/build.sh first, then run this script from any directory.
"""
from pathlib import Path
import os
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / 'build/modes'
OBJDUMP = Path(os.environ.get('OP1_BFIN_TOOLCHAIN', str(ROOT / '.work/bfin-native/install'))) / 'bin/bfin-elf-objdump'
OBJECT = PLUGIN / 'screenmirror.build/flush_entry.S.o'
REGISTERS = ({f'R{i}' for i in range(8)} | {f'P{i}' for i in range(6)} |
             {'FP', 'ASTAT', 'RETS'} |
             {f'{prefix}{i}' for prefix in ('LC', 'LT', 'LB') for i in range(2)} |
             {f'A{i}.{part}' for i in range(2) for part in ('X', 'W')} |
             {f'{prefix}{i}' for prefix in ('I', 'M', 'B', 'L') for i in range(4)})


def assembled_entry():
    output = subprocess.check_output([str(OBJDUMP), '-dr', str(OBJECT)], text=True)
    instructions = []
    relocations = {}
    for line in output.splitlines():
        instruction = re.match(r'^\s*([0-9a-f]+):\s+(?:[0-9a-f]{2}\s+)+(.+?);', line)
        relocation = re.match(r'^\s*([0-9a-f]+):\s+(R_BFIN_\S+)\s+(\S+)', line)
        if instruction:
            instructions.append((int(instruction[1], 16), instruction[2].strip()))
        elif relocation:
            relocations[int(relocation[1], 16)] = (relocation[2], relocation[3])
    return instructions, relocations


def verify(instructions, relocations):
    original = {name: f'original:{name}' for name in REGISTERS}
    registers = original.copy()
    stack = []
    saved = []
    restored = []
    calls = tails = 0
    area = 0
    peak = 0
    for index, (pc, instruction) in enumerate(instructions):
        push = re.fullmatch(r'\[--SP\] = (\S+)', instruction)
        pop = re.fullmatch(r'(\S+) = \[SP\+\+\]', instruction)
        if push:
            register = push[1]
            assert register in REGISTERS and not calls, f'unexpected save {instruction}'
            stack.append(registers[register]);saved.append(register)
        elif pop:
            register = pop[1]
            assert calls == 1 and not area and stack, f'bad restore {instruction}'
            registers[register] = stack.pop();restored.append(register)
        elif instruction == 'SP += -0xc':
            assert not area and not calls
            area = 12;stack.extend(['argument-slot'] * 3)
        elif instruction.startswith('CALL '):
            assert not calls and area == 12
            assert len(saved) == len(REGISTERS) and set(saved) == REGISTERS
            assert relocations[pc + 2] == ('R_BFIN_PCREL24', '_screenmirror_capture_flush')
            assert all(registers[r] == original[r] for r in ('R0', 'R1', 'R2'))
            # GCC fixes L0..3 at zero; its I/M address arithmetic may otherwise
            # wrap through the stock caller's circular buffers. The compiled
            # capture currently uses I1 += M0 even for a small integer counter.
            assert all(registers[f'L{i}'] == 0 for i in range(4)), 'C requires linear DAG addressing'
            # The C ABI permits spilling its register arguments here. This must
            # not overlap any saved machine state.
            stack[-3:] = ['callee-written-slot'] * 3
            registers = {name: f'hostile-callee:{name}' for name in REGISTERS}
            calls += 1
        elif instruction == 'R3 = 0x0 (X)':
            assert not calls and len(saved) == len(REGISTERS)
            registers['R3'] = 0
        elif re.fullmatch(r'L[0-3] = R3', instruction):
            assert not calls and registers['R3'] == 0
            registers[instruction.split()[0]] = 0
        elif instruction == 'SP += 0xc':
            assert calls == 1 and area == 12
            del stack[-3:];area = 0
        elif instruction.startswith('JUMP.L '):
            assert index == len(instructions) - 1
            assert relocations[pc + 2] == ('R_BFIN_PCREL24_JUMP_L', '_fw_dcache_flush')
            assert calls == 1 and not stack and not area and registers == original
            assert restored == list(reversed(saved))
            tails += 1
        else:
            raise AssertionError(f'unexpected instruction: {instruction}')
        peak = max(peak, len(stack) * 4)
    assert calls == tails == 1 and peak == 184
    return {'saved_registers': len(saved), 'bridge_stack_bytes': peak}


class FinalHookABI(unittest.TestCase):
    def test_actual_assembled_bridge_preserves_state_and_chains_flush(self):
        instructions, relocations = assembled_entry()
        result = verify(instructions, relocations)
        self.assertEqual(result, {'saved_registers': 43, 'bridge_stack_bytes': 184})
        print('PASS: assembled bridge restores 43 registers and original arguments/RETS; '
              'linear DAG for C; 12-byte C argument area; 184-byte bridge stack; exactly one flush tail-chain')

    def test_missing_restore_is_detected(self):
        instructions, relocations = assembled_entry()
        damaged = [item for item in instructions if item[1] != 'FP = [SP++]']
        with self.assertRaises(AssertionError):
            verify(damaged, relocations)

    def test_missing_argument_area_is_detected(self):
        instructions, relocations = assembled_entry()
        damaged = [item for item in instructions if item[1] != 'SP += -0xc']
        with self.assertRaises(AssertionError):
            verify(damaged, relocations)

    def test_incoming_circular_dag_state_is_not_allowed_into_c(self):
        instructions, relocations = assembled_entry()
        damaged = [item for item in instructions if item[1] != 'L1 = R3']
        with self.assertRaises(AssertionError):
            verify(damaged, relocations)


if __name__ == '__main__':
    unittest.main()
