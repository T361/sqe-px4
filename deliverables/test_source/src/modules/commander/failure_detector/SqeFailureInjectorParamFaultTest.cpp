/**
 * SQE A2 — student-authored tests (SE3002 Assignment 02). Not upstream code.
 * System under test : FailureInjector.cpp — constructor (PX4-Autopilot v1.17.0, d6f12ad1)
 * Test level        : GTest functional (parameter store + uORB from gtest_functional_main)
 * Test double       : a link-time stub of param_get(). This binary is linked with -Wl,--wrap=param_get, so every
 *                      reference to param_get from the code under test resolves to __wrap_param_get below. The stub
 *                      forwards to the real function unless a test arms a failure for one parameter handle. This is
 *                      how FI-D01's first operand (param_get(...) == PX4_OK) is driven False — previously gap G-06,
 *                      unreachable with the real parameter store because SYS_FAILURE_EN is compiled in.
 * Decisions covered : FI-D01 (1st operand False, and the True/True positive control)
 * Authors           : SQE A2 team — AI assistance recorded in ai_assistance_log.md
 */

#include <gtest/gtest.h>

#include <parameters/param.h>
#include <uORB/Publication.hpp>
#include <uORB/Subscription.hpp>
#include <uORB/topics/vehicle_command.h>
#include <uORB/topics/vehicle_command_ack.h>

#include "FailureInjector.hpp"

namespace
{
param_t g_fail_handle{PARAM_INVALID};
int g_fail_hits{0};
}

extern "C" int __real_param_get(param_t param, void *val);
extern "C" int __wrap_param_get(param_t param, void *val)
{
	if (g_fail_handle != PARAM_INVALID && param == g_fail_handle) {
		g_fail_hits++;
		return PX4_ERROR;
	}

	return __real_param_get(param, val);
}

class SqeFailureInjectorParamFaultTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		param_control_autosave(false);
		param_reset_all();
		g_fail_handle = PARAM_INVALID;
		g_fail_hits = 0;

		int32_t enabled = 1;
		ASSERT_EQ(param_set(param_find("SYS_FAILURE_EN"), &enabled), PX4_OK);

		vehicle_command_ack_s ack{};

		while (_ack_sub.update(&ack)) {}
	}

	void TearDown() override { g_fail_handle = PARAM_INVALID; }

	void injectMotor1Off()
	{
		vehicle_command_s cmd{};
		cmd.timestamp = hrt_absolute_time();
		cmd.command = vehicle_command_s::VEHICLE_CMD_INJECT_FAILURE;
		cmd.param1 = static_cast<float>(vehicle_command_s::FAILURE_UNIT_SYSTEM_MOTOR);
		cmd.param2 = static_cast<float>(vehicle_command_s::FAILURE_TYPE_OFF);
		cmd.param3 = 1.f;
		_cmd_pub.publish(cmd);
	}

	uORB::Publication<vehicle_command_s> _cmd_pub{ORB_ID(vehicle_command)};
	uORB::Subscription _ack_sub{ORB_ID(vehicle_command_ack)};
};

// SQE-FIP-01 | FI-D01 1st operand False (param_get fails) — closes G-06
// Given: SYS_FAILURE_EN = 1 in the parameter store, but the param_get stub fails for that handle
// When : a FailureInjector is constructed, then a "motor 1 off" injection is published and update() runs
// Then : the stub was consulted exactly once (the constructor's read); because the read failed, injection stays
//        disabled even though the stored value is 1: no stop mask and no acknowledgement
TEST_F(SqeFailureInjectorParamFaultTest, FIP01_ParamReadFails_InjectionStaysDisabled)
{
	g_fail_handle = param_find("SYS_FAILURE_EN");
	FailureInjector injector;
	g_fail_handle = PARAM_INVALID;

	EXPECT_EQ(g_fail_hits, 1);

	injectMotor1Off();
	injector.update();

	EXPECT_EQ(injector.getMotorStopMask(), 0u);
	vehicle_command_ack_s ack{};
	EXPECT_FALSE(_ack_sub.update(&ack));
}

// SQE-FIP-02 | FI-D01 True∧True — positive control for FIP-01 with the stub in pass-through mode
// Given: the same stored SYS_FAILURE_EN = 1, stub not armed
// When : construct, inject "motor 1 off", update()
// Then : the read succeeds, injection is enabled: stop mask 0x01 and an ACCEPTED acknowledgement — proving FIP-01's
//        result is caused by the failed read and not by the test setup
TEST_F(SqeFailureInjectorParamFaultTest, FIP02_ParamReadSucceeds_InjectionEnabled)
{
	FailureInjector injector;
	EXPECT_EQ(g_fail_hits, 0);

	injectMotor1Off();
	injector.update();

	EXPECT_EQ(injector.getMotorStopMask(), 0x01u);
	vehicle_command_ack_s ack{};
	ASSERT_TRUE(_ack_sub.update(&ack));
	EXPECT_EQ(ack.result, vehicle_command_ack_s::VEHICLE_CMD_RESULT_ACCEPTED);
}
