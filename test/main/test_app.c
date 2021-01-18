#include <stdio.h>
#include <string.h>

#include "unity.h"


static void print_banner(const char* text)
{
  printf("\n#### %s #####\n\n", text);
}

void app_main(void)
{

  /* These are the different ways of running registered tests.
   * In practice, only one of them is usually needed.
   *
   * UNITY_BEGIN() and UNITY_END() calls tell Unity to print a summary
   * (number of tests executed/failed/ignored) of tests executed between these calls.
   */
  // print_banner("Executing one test by its name");
  // UNITY_BEGIN();
  // unity_run_test_by_name("Mean of an empty array is zero");
  // UNITY_END();

  // print_banner("Running tests with [rc522] tag");
  // UNITY_BEGIN();
  // unity_run_tests_by_tag("[rc522]", false);
  // UNITY_END();

  // print_banner("Running tests without [fails] tag");
  // UNITY_BEGIN();
  // unity_run_tests_by_tag("[pn532]", true);
  // UNITY_END();

  print_banner("Running all the registered tests");
  UNITY_BEGIN();
  unity_run_all_tests();
  UNITY_END();

  print_banner("Starting interactive test menu");
  /* This function will not return, and will be busy waiting for UART input.
   * Make sure that task watchdog is disabled if you use this function.
   */
  unity_run_menu();
}


