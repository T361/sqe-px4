# Unique coverage per test — /home/dns/Desktop/sqe-a2-px4-kit/evidence/coverage/pertest/unit-SqeDataValidatorGroup

| Test | Lines covered | Branch outcomes covered | Unique lines (lost if removed) | Unique branch outcomes (lost if removed) |
|---|---|---|---|---|
| SqeDvgDeathTest_DVG10_AddNewValidatorOnEmptyGroup_CrashesOnNullLast | 0 | 0 | 0  | 0  |
| SqeDvgMcdcTest_MC01_EqualConfidenceHigherPriority_SwitchNotCountedAsFailover | 108 | 53 | 0  | 0  |
| SqeDvgMcdcTest_MC02_EqualConfidenceEqualPriority_NoSwitch | 102 | 48 | 0  | 0  |
| SqeDvgMcdcTest_MC03_LowerConfidenceHigherPriority_NoSwitch | 102 | 48 | 0  | 0  |
| SqeDvgMcdcTest_MC04_HigherConfidenceEqualPriority_SwitchCountedAsFailover | 109 | 54 | 0  | 0  |
| SqeDvgMcdcTest_MC05_HigherConfidenceLowerPriority_NoSwitch | 103 | 50 | 0  | 0  |
| SqeDvgMcdcTest_MC06_LowerConfidenceEqualPriority_NoSwitch | 102 | 48 | 0  | 0  |
| SqeDvgMcdcTest_MC07_BelowThresholdConfidenceReplacedByAboveThreshold_Switches | 109 | 54 | 0  | 0  |
| SqeDvgMcdcTest_MC08_AboveThresholdConfidenceLowerPriorityCandidate_NoSwitch | 103 | 50 | 0  | 0  |
| SqeDvgMcdcTest_MC09_FarBelowThresholdReplacedByAboveThreshold_Switches | 109 | 54 | 0  | 0  |
| SqeDvgMcdcTest_MC10_BothBelowThresholdLowerPriorityCandidate_NoSwitch | 103 | 50 | 0  | 0  |
| SqeDvgMcdcTest_MC11_FirstCallNeverPut_NoCandidateSelected | 52 | 26 | 0  | 0  |
| SqeDvgMcdcTest_MC12_FirstCallOneSensorPut_SelectsIt | 92 | 43 | 0  | 0  |
| SqeDvgMcdcTest_MC13_SensorZeroTimesOutSensorOneTakesOver_RealFailoverReported | 131 | 69 | 1 DataValidatorGroup.cpp:303 | 2 DataValidatorGroup.cpp:301 b0.4, DataValidatorGroup.cpp:302 b0.0 |
| SqeDvgMcdcTest_MC14_StableSensorKeepsWinningOnPriority_NoFailover | 102 | 49 | 0  | 0  |
| SqeDvgMcdcTest_MC18_SingleSensorFreshDataStable_NoFailover | 104 | 46 | 0  | 0  |
| SqeDvgMcdcTest_MC19_SingleSensorSilentPastTimeout_FailoverViaConfidenceDrop | 113 | 57 | 0  | 0  |
| SqeDvgMcdcTest_MC20_SingleSensorNeverFed_NoCandidateSelected | 51 | 25 | 0  | 0  |
| SqeDvgMcdcTest_MC21_FirstCallKFalseControlVector_NoFailoverCounted | 92 | 43 | 0  | 0  |
| SqeDvgMcdcTest_MC22_HigherPriorityButLargeConfidenceJump_CountsAsFailover | 110 | 55 | 0  | 1 DataValidatorGroup.cpp:208 b0.1 |
| SqeDvgMcdcTest_MC23_SensorZeroRecoversAfterFailover_NoFailoverReported | 139 | 77 | 0  | 0  |
| SqeDvgMcdcTest_MC24_SensorNeverBestTimesOut_NotReportedAsFailover | 135 | 68 | 0  | 2 DataValidatorGroup.cpp:283 b0.1, DataValidatorGroup.cpp:302 b0.1 |
| SqeDvgMcdcTest_MC25_PutWithTimestampZeroResetsUsed_NotReportedAsFailover | 135 | 74 | 0  | 0  |
| SqeDvgTest_DISABLED_PRB03_AllFailedShouldReturnNull | 0 | 0 | 0  | 0  |
| SqeDvgTest_DVG01_FreshSingleSensor_QueriesReturnDefaultsAndNotFoundSentinels | 52 | 24 | 0  | 0  |
| SqeDvgTest_DVG02_PutAtValidIndexSetsPriority_OutOfRangePutIgnored | 53 | 22 | 0  | 1 DataValidatorGroup.cpp:129 b0.1 |
| SqeDvgTest_DVG03_EmptyGroup_AllQueriesReturnSentinelsNoCrash | 62 | 17 | 0  | 0  |
| SqeDvgTest_DVG04_AddNewValidatorAfterSetTimeout_InheritsGroupTimeout | 133 | 65 | 0  | 0  |
| SqeDvgTest_DVG05_GroupTimeoutShorterThanDefault_BothSensorsTimeOutTogether | 94 | 41 | 0  | 0  |
| SqeDvgTest_DVG06_GroupEqualValueThreshold_PropagatesToAllSiblingsAndStales | 101 | 44 | 0  | 0  |
| SqeDvgTest_DVG07_EqualConfidenceDifferentPriority_HigherPriorityWins | 91 | 42 | 0  | 0  |
| SqeDvgTest_DVG08_SecondFailover_CountsTwiceAndPrintShowsFailsafeYes | 133 | 75 | 0  | 3 DataValidatorGroup.cpp:230 b0.1, DataValidatorGroup.cpp:250 b0.0, DataValidatorGroup.cpp:260 b0.6 |
| SqeDvgTest_DVG09_PrintSkipsUnusedSensorsAndShowsPerSensorFlags | 131 | 72 | 4 DataValidator.cpp:122, DataValidator.cpp:123, DataValidator.cpp:127, DataValidator.cpp:128 | 7 DataValidator.cpp:120 b0.0, DataValidator.cpp:125 b0.0, DataValidator.cpp:136 b0.1, DataValidatorGroup.cpp:257 b0.1, DataValidatorGroup.cpp:260 b0.2, DataValidatorGroup.cpp:260 b0.4 … |
| SqeDvgTest_DVG11_BothSensorsTimeOutAfterOneWasBest_IndexNegativeButPointerNonNull | 109 | 57 | 0  | 0  |
| SqeDvgTest_DVG12_ConstructDestroyVariousSizes_NoCrash | 26 | 12 | 0  | 0  |

Tests with no unique structural contribution (29): SqeDvgDeathTest_DVG10_AddNewValidatorOnEmptyGroup_CrashesOnNullLast, SqeDvgMcdcTest_MC01_EqualConfidenceHigherPriority_SwitchNotCountedAsFailover, SqeDvgMcdcTest_MC02_EqualConfidenceEqualPriority_NoSwitch, SqeDvgMcdcTest_MC03_LowerConfidenceHigherPriority_NoSwitch, SqeDvgMcdcTest_MC04_HigherConfidenceEqualPriority_SwitchCountedAsFailover, SqeDvgMcdcTest_MC05_HigherConfidenceLowerPriority_NoSwitch, SqeDvgMcdcTest_MC06_LowerConfidenceEqualPriority_NoSwitch, SqeDvgMcdcTest_MC07_BelowThresholdConfidenceReplacedByAboveThreshold_Switches, SqeDvgMcdcTest_MC08_AboveThresholdConfidenceLowerPriorityCandidate_NoSwitch, SqeDvgMcdcTest_MC09_FarBelowThresholdReplacedByAboveThreshold_Switches, SqeDvgMcdcTest_MC10_BothBelowThresholdLowerPriorityCandidate_NoSwitch, SqeDvgMcdcTest_MC11_FirstCallNeverPut_NoCandidateSelected, SqeDvgMcdcTest_MC12_FirstCallOneSensorPut_SelectsIt, SqeDvgMcdcTest_MC14_StableSensorKeepsWinningOnPriority_NoFailover, SqeDvgMcdcTest_MC18_SingleSensorFreshDataStable_NoFailover, SqeDvgMcdcTest_MC19_SingleSensorSilentPastTimeout_FailoverViaConfidenceDrop, SqeDvgMcdcTest_MC20_SingleSensorNeverFed_NoCandidateSelected, SqeDvgMcdcTest_MC21_FirstCallKFalseControlVector_NoFailoverCounted, SqeDvgMcdcTest_MC23_SensorZeroRecoversAfterFailover_NoFailoverReported, SqeDvgMcdcTest_MC25_PutWithTimestampZeroResetsUsed_NotReportedAsFailover, SqeDvgTest_DISABLED_PRB03_AllFailedShouldReturnNull, SqeDvgTest_DVG01_FreshSingleSensor_QueriesReturnDefaultsAndNotFoundSentinels, SqeDvgTest_DVG03_EmptyGroup_AllQueriesReturnSentinelsNoCrash, SqeDvgTest_DVG04_AddNewValidatorAfterSetTimeout_InheritsGroupTimeout, SqeDvgTest_DVG05_GroupTimeoutShorterThanDefault_BothSensorsTimeOutTogether, SqeDvgTest_DVG06_GroupEqualValueThreshold_PropagatesToAllSiblingsAndStales, SqeDvgTest_DVG07_EqualConfidenceDifferentPriority_HigherPriorityWins, SqeDvgTest_DVG11_BothSensorsTimeOutAfterOneWasBest_IndexNegativeButPointerNonNull, SqeDvgTest_DVG12_ConstructDestroyVariousSizes_NoCrash
(A test without unique coverage can still be essential: MC/DC pairs and oracles are not measured by line/branch uniqueness.)
