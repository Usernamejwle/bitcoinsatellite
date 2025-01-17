#include <boost/test/unit_test.hpp>
#include <iostream>
#include <test/util/setup_common.h>
#include <throttle.h>

BOOST_FIXTURE_TEST_SUITE(throttle_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(test_quota_usage)
{
    const double units_per_sec = 10;
    // NOTE: use a low rate of units per second such that the accumulated quota
    // does not vary significantly on slow builds
    const unsigned int wait_ms = 400;
    uint32_t nominal_quota = (units_per_sec * wait_ms / 1000);

    Throttle throttle(units_per_sec);

    // Start with zero quota
    auto quota = throttle.GetQuota();
    BOOST_CHECK(quota == 0);

    // Accumulate quota over time
    std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));

    // Get the accumulated quota
    quota = throttle.GetQuota();
    BOOST_CHECK(quota == nominal_quota);

    // Use the full quota
    BOOST_CHECK(throttle.UseQuota(quota));
    quota = throttle.GetQuota();
    BOOST_CHECK(quota == 0);

    // Accumulate quota once again
    std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
    quota = throttle.GetQuota();
    BOOST_CHECK(quota == nominal_quota);

    // Use only half of the quota
    BOOST_CHECK(throttle.UseQuota(quota / 2));

    // Check that the other half remains
    quota = throttle.GetQuota();
    BOOST_CHECK(quota == (nominal_quota / 2));
}

BOOST_AUTO_TEST_CASE(test_quota_capping)
{
    const double units_per_sec = 20000;
    const double max_quota = 100;
    const unsigned int wait_ms = 10;

    Throttle throttle(units_per_sec);
    throttle.SetMaxQuota(max_quota);

    std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
    uint32_t uncapped_quota = (units_per_sec * wait_ms / 1000);
    BOOST_CHECK(!throttle.HasQuota(uncapped_quota));
    BOOST_CHECK(throttle.HasQuota(max_quota));
}

BOOST_AUTO_TEST_CASE(test_quota_wait_estimate)
{
    const double units_per_sec = 100;
    const uint32_t target_wait_ms = 10;
    const uint32_t expected_quota = (units_per_sec * target_wait_ms / 1000);

    Throttle throttle(units_per_sec);

    for (int i = 0; i < 10; i++) {
        // Predict the wait to accumulate the expected quota
        const uint32_t wait_ms = throttle.EstimateWait(expected_quota);
        BOOST_CHECK(wait_ms <= target_wait_ms);
        // The quota accumulates continuously as a fractional number. However,
        // only integer quotas can be consumed. As a result, some fractional
        // quota remains after every quota consumption. Over time, this residual
        // quota accumulates such that the estimated wait on any iteration can
        // become lower than the nominal wait.

        // Wait and accumulate
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));

        // Now there should be no need to wait any longer
        BOOST_CHECK(throttle.EstimateWait(expected_quota) == 0);
        BOOST_CHECK(throttle.UseQuota(expected_quota));
    }
}

BOOST_AUTO_TEST_SUITE_END()
