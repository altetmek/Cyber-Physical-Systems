#define CATCH_CONFIG_MAIN // This tells Catch to provide a main() - only do this once per test-runner!

#include "catch.hpp"
#include "PrimeChecker.hpp"

TEST_CASE("Test PrimeChecker 1.")
{
    PrimeChecker pc;

    // Check correct detection of prime numbers
    REQUIRE(pc.isPrime(3));
    REQUIRE(pc.isPrime(5));
    REQUIRE(pc.isPrime(7));
    REQUIRE(!pc.isPrime(9));
    REQUIRE(pc.isPrime(11));
    REQUIRE(pc.isPrime(13));

    // Test NON prime number
    REQUIRE(!pc.isPrime(12));
}
