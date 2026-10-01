# Unique coverage per test — /home/dns/Desktop/sqe-a2-px4-kit/evidence/coverage/pertest/functional-SqeFailureDetector

| Test | Lines covered | Branch outcomes covered | Unique lines (lost if removed) | Unique branch outcomes (lost if removed) |
|---|---|---|---|---|
| SqeFailureDetectorTest_FD01_AttitudeControlDisabled_ResetsAllAttitudeFlags | 43 | 49 | 4 FailureDetector.cpp:64, FailureDetector.cpp:65, FailureDetector.cpp:66, FailureDetector.cpp:67 | 1 FailureDetector.cpp:56 b0.1 |
| SqeFailureDetectorTest_FD02_RollBeyondLimit_FlagSetsAfterHysteresis | 39 | 48 | 0  | 0  |
| SqeFailureDetectorTest_FD03_RollWithinLimit_FlagStaysFalse | 39 | 48 | 0  | 0  |
| SqeFailureDetectorTest_FD04_RollLimitZero_ChecksDisabled | 39 | 47 | 0  | 1 FailureDetector.cpp:123 b0.1 |
| SqeFailureDetectorTest_FD05_PitchBeyondLimit_FlagSetsAfterHysteresis | 39 | 48 | 0  | 0  |
| SqeFailureDetectorTest_FD06_PitchWithinLimit_FlagStaysFalse | 39 | 48 | 0  | 0  |
| SqeFailureDetectorTest_FD07_PitchLimitZero_ChecksDisabled | 39 | 47 | 0  | 1 FailureDetector.cpp:124 b0.1 |
| SqeFailureDetectorTest_FD08_NoNewAttitude_FlagsUnchangedAndNoChangeReported | 39 | 49 | 0  | 0  |
| SqeFailureDetectorTest_FD09_TailsitterInTransition_AttitudeCheckDisabled | 42 | 49 | 2 FailureDetector.cpp:106, FailureDetector.cpp:107 | 1 FailureDetector.cpp:104 b0.0 |
| SqeFailureDetectorTest_FD10_TailsitterFixedWing_RotatesAttitudeBeforeCheck | 45 | 58 | 4 FailureDetector.cpp:111, FailureDetector.cpp:112, FailureDetector.cpp:113, FailureDetector.cpp:114 | 9 FailureDetector.cpp:109 b0.0, FailureDetector.cpp:111 b0.0, FailureDetector.cpp:111 b0.2, FailureDetector.cpp:111 b0.4, FailureDetector.cpp:111 b0.6, FailureDetector.cpp:111 b0.8 … |
| SqeFailureDetectorTest_FD11_ExternalAtsPulseInWindow_FlagSetsAfterHysteresis | 32 | 42 | 0  | 0  |
| SqeFailureDetectorTest_FD12_PulseWidthBoundaries_WindowIsHalfOpen | 32 | 44 | 0  | 2 FailureDetector.cpp:147 b0.1, FailureDetector.cpp:147 b0.3 |
| SqeFailureDetectorTest_FD13_ExternalAtsDisabled_ExtFlagNeverSet | 22 | 34 | 0  | 0  |
| SqeFailureDetectorTest_FD14_NoNewPwm_ExtFlagUnchanged | 32 | 43 | 0  | 1 FailureDetector.cpp:144 b0.3 |
| SqeFailureDetectorTest_FD15_AllEscsArmedNoFailures_ArmEscsStaysFalse | 64 | 67 | 0  | 0  |
| SqeFailureDetectorTest_FD16_EscArmedMismatch_ArmEscsSetsAfterHysteresis | 65 | 67 | 0  | 0  |
| SqeFailureDetectorTest_FD17_PerEscFailureFlag_ArmEscsSetsEvenWithMatchingMask | 65 | 69 | 0  | 1 FailureDetector.cpp:171 b0.2 |
| SqeFailureDetectorTest_FD18_HealthyAfterTrigger_ArmEscsStaysLatchedWhileArmed | 65 | 69 | 0  | 0  |
| SqeFailureDetectorTest_FD19_Disarmed_ArmEscsStaysFalse | 42 | 46 | 2 FailureDetector.cpp:183, FailureDetector.cpp:184 | 2 FailureDetector.cpp:163 b0.1, FailureDetector.cpp:183 b0.0 |
| SqeFailureDetectorTest_FD20_EscCountAboveMax_ClampedToEight | 64 | 68 | 0  | 0  |
| SqeFailureDetectorTest_FD21_EscsCheckDisabled_ArmEscsNeverSet | 50 | 57 | 0  | 0  |
| SqeFailureDetectorTest_FD22_NoNewEscStatus_EscAndMotorLogicSkipped | 64 | 68 | 0  | 0  |
| SqeFailureDetectorTest_FD23_EscTelemetryTimeout_SetsThenClearsMotorFlag | 66 | 75 | 0  | 1 FailureDetector.cpp:288 b0.5 |
| SqeFailureDetectorTest_FD24_EscNeverReportsCurrent_NoTimeoutFlag | 50 | 56 | 0  | 0  |
| SqeFailureDetectorTest_FD25_UnderCurrentWhileArmed_LatchesMotorFlagAfterTimeout | 67 | 77 | 0  | 0  |
| SqeFailureDetectorTest_FD26_ThrottleDropsBeforeTimeout_ResetsUnderCurrentTimer | 66 | 72 | 0  | 0  |
| SqeFailureDetectorTest_FD27_NanControl_TreatedAsZeroThrottle | 61 | 64 | 0  | 1 FailureDetector.cpp:305 b0.1 |
| SqeFailureDetectorTest_FD28_NonMotorActuatorFunction_SkippedByUnsignedGuard | 43 | 50 | 1 FailureDetector.cpp:275 | 1 FailureDetector.cpp:274 b0.0 |
| SqeFailureDetectorTest_FD29_DisarmAfterUnderCurrentLatch_ClearsMotorFlagAndMask | 71 | 80 | 0  | 0  |
| SqeFailureDetectorTest_FD30_MotorCheckDisabled_MotorLogicSkipped | 27 | 38 | 0  | 1 FailureDetector.cpp:80 b0.1 |
| SqeFailureDetectorTest_FD31_InjectedStuckFailure_TriggersDetectorMotorTimeout | 96 | 92 | 32 FailureInjector.cpp:45, FailureInjector.cpp:55, FailureInjector.cpp:56, FailureInjector.cpp:57, FailureInjector.cpp:59, FailureInjector.cpp:60, FailureInjector.cpp:61, FailureInjector.cpp:64 … | 26 FailureInjector.cpp:44 b0.2, FailureInjector.cpp:44 b0.4, FailureInjector.cpp:51 b0.1, FailureInjector.cpp:55 b0.0, FailureInjector.cpp:55 b0.2, FailureInjector.cpp:55 b0.3 … |
| SqeFailureDetectorTest_FD32_UpdateReturnValue_ReflectsStatusChange | 39 | 49 | 0  | 0  |
| SqeFailureDetectorTest_FD33_DisarmAfterTimeout_MotorFlagClearsButTimedOutMaskSurvivesCharacterization | 68 | 72 | 0  | 0  |
| SqeFailureDetectorTest_FD34_TailsitterRotaryWing_NoRotationAppliedToCheck | 41 | 50 | 0  | 1 FailureDetector.cpp:109 b0.1 |
| SqeFailureDetectorTest_FD36_UnderCurrentConditionButEscAlreadyTimedOut_TimeoutPathFiresNotUnderCurrent | 69 | 78 | 0  | 1 FailureDetector.cpp:313 b0.5 |
| SqeFailureDetectorTest_FD37_UnderCurrentMaskAlreadyLatched_SecondExpiryIsNoOp | 68 | 82 | 0  | 1 FailureDetector.cpp:326 b0.3 |

Tests with no unique structural contribution (19): SqeFailureDetectorTest_FD02_RollBeyondLimit_FlagSetsAfterHysteresis, SqeFailureDetectorTest_FD03_RollWithinLimit_FlagStaysFalse, SqeFailureDetectorTest_FD05_PitchBeyondLimit_FlagSetsAfterHysteresis, SqeFailureDetectorTest_FD06_PitchWithinLimit_FlagStaysFalse, SqeFailureDetectorTest_FD08_NoNewAttitude_FlagsUnchangedAndNoChangeReported, SqeFailureDetectorTest_FD11_ExternalAtsPulseInWindow_FlagSetsAfterHysteresis, SqeFailureDetectorTest_FD13_ExternalAtsDisabled_ExtFlagNeverSet, SqeFailureDetectorTest_FD15_AllEscsArmedNoFailures_ArmEscsStaysFalse, SqeFailureDetectorTest_FD16_EscArmedMismatch_ArmEscsSetsAfterHysteresis, SqeFailureDetectorTest_FD18_HealthyAfterTrigger_ArmEscsStaysLatchedWhileArmed, SqeFailureDetectorTest_FD20_EscCountAboveMax_ClampedToEight, SqeFailureDetectorTest_FD21_EscsCheckDisabled_ArmEscsNeverSet, SqeFailureDetectorTest_FD22_NoNewEscStatus_EscAndMotorLogicSkipped, SqeFailureDetectorTest_FD24_EscNeverReportsCurrent_NoTimeoutFlag, SqeFailureDetectorTest_FD25_UnderCurrentWhileArmed_LatchesMotorFlagAfterTimeout, SqeFailureDetectorTest_FD26_ThrottleDropsBeforeTimeout_ResetsUnderCurrentTimer, SqeFailureDetectorTest_FD29_DisarmAfterUnderCurrentLatch_ClearsMotorFlagAndMask, SqeFailureDetectorTest_FD32_UpdateReturnValue_ReflectsStatusChange, SqeFailureDetectorTest_FD33_DisarmAfterTimeout_MotorFlagClearsButTimedOutMaskSurvivesCharacterization
(A test without unique coverage can still be essential: MC/DC pairs and oracles are not measured by line/branch uniqueness.)
