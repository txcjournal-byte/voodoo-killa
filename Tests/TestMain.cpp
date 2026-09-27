#include <juce_core/juce_core.h>

int main()
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("VoodooKilla");

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        failures += runner.getResult (i)->failures;

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures);
    return failures == 0 ? 0 : 1;
}
