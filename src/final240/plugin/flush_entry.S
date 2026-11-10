/* Experimental #246 final-frame observer. The stock flush has a narrower
 * clobber set than a C function, so preserve the whole C-visible machine state
 * around the observer, including hardware loops, accumulators and DAG registers.
 * R0=start, R1=end, R2=invalidate remain exactly the original arguments.
 * The only tail destination is the declared SDK fw_dcache_flush import. */
        .text
        .global _screenmirror_flush_entry
        .type _screenmirror_flush_entry, STT_FUNC
        .align 2
_screenmirror_flush_entry:
        [--SP] = RETS;
        [--SP] = FP;
        [--SP] = ASTAT;
        [--SP] = R0;
        [--SP] = R1;
        [--SP] = R2;
        [--SP] = R3;
        [--SP] = R4;
        [--SP] = R5;
        [--SP] = R6;
        [--SP] = R7;
        [--SP] = P0;
        [--SP] = P1;
        [--SP] = P2;
        [--SP] = P3;
        [--SP] = P4;
        [--SP] = P5;
        [--SP] = LC0;
        [--SP] = LT0;
        [--SP] = LB0;
        [--SP] = LC1;
        [--SP] = LT1;
        [--SP] = LB1;
        [--SP] = A0.X;
        [--SP] = A0.W;
        [--SP] = A1.X;
        [--SP] = A1.W;
        [--SP] = I0;
        [--SP] = I1;
        [--SP] = I2;
        [--SP] = I3;
        [--SP] = M0;
        [--SP] = M1;
        [--SP] = M2;
        [--SP] = M3;
        [--SP] = B0;
        [--SP] = B1;
        [--SP] = B2;
        [--SP] = B3;
        [--SP] = L0;
        [--SP] = L1;
        [--SP] = L2;
        [--SP] = L3;
        /* GCC uses DAG index arithmetic as linear integer arithmetic. Firmware
         * may have circular DAG lengths live, so normalize lengths for the C
         * observer only; original lengths and R3 are restored below. */
        R3 = 0;
        L0 = R3;
        L1 = R3;
        L2 = R3;
        L3 = R3;
        /* GCC/Blackfin requires 12 bytes of outgoing argument area. */
        SP += -12;
        CALL _screenmirror_capture_flush;
        SP += 12;
        L3 = [SP++];
        L2 = [SP++];
        L1 = [SP++];
        L0 = [SP++];
        B3 = [SP++];
        B2 = [SP++];
        B1 = [SP++];
        B0 = [SP++];
        M3 = [SP++];
        M2 = [SP++];
        M1 = [SP++];
        M0 = [SP++];
        I3 = [SP++];
        I2 = [SP++];
        I1 = [SP++];
        I0 = [SP++];
        A1.W = [SP++];
        A1.X = [SP++];
        A0.W = [SP++];
        A0.X = [SP++];
        LB1 = [SP++];
        LT1 = [SP++];
        LC1 = [SP++];
        LB0 = [SP++];
        LT0 = [SP++];
        LC0 = [SP++];
        P5 = [SP++];
        P4 = [SP++];
        P3 = [SP++];
        P2 = [SP++];
        P1 = [SP++];
        P0 = [SP++];
        R7 = [SP++];
        R6 = [SP++];
        R5 = [SP++];
        R4 = [SP++];
        R3 = [SP++];
        R2 = [SP++];
        R1 = [SP++];
        R0 = [SP++];
        ASTAT = [SP++];
        FP = [SP++];
        RETS = [SP++];
        /* Original RETS and all arguments restored; original flush runs once. */
        JUMP.L _fw_dcache_flush;
        .size _screenmirror_flush_entry, .-_screenmirror_flush_entry
